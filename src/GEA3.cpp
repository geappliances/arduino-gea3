/*!
 * @file
 * @brief
 */

#include "GEA3.h"

extern "C" {
#include "tiny_time_source.h"
}

struct AsyncReadSubscription {
  AsyncReadSubscription(void* context, void (*callback)(void* context, GEA3::ReadStatus status, const void* value, uint8_t valueSize), tiny_gea3_erd_client_request_id_t requestId, tiny_gea3_erd_client_t* erdClient)
    : context(context), callback(callback), requestId(requestId), erdClient(erdClient), subscription()
  {
  }

  void* context;
  void (*callback)(void* context, GEA3::ReadStatus status, const void* value, uint8_t valueSize);
  tiny_gea3_erd_client_request_id_t requestId;
  tiny_gea3_erd_client_t* erdClient;
  tiny_event_subscription_t subscription;
};

struct AsyncWriteSubscription {
  AsyncWriteSubscription(void* context, void (*callback)(void* context, GEA3::WriteStatus status), tiny_gea3_erd_client_request_id_t requestId, tiny_gea3_erd_client_t* erdClient)
    : context(context), callback(callback), requestId(requestId), erdClient(erdClient), subscription()
  {
  }

  void* context;
  void (*callback)(void* context, GEA3::WriteStatus status);
  tiny_gea3_erd_client_request_id_t requestId;
  tiny_gea3_erd_client_t* erdClient;
  tiny_event_subscription_t subscription;
};

void GEA3::begin(Stream& uart, uint8_t clientAddress, uint32_t requestTimeout, uint8_t requestRetries)
{
  tiny_timer_group_init(&timerGroup, tiny_time_source_init());

  tiny_stream_uart_init(&streamUart, &timerGroup, uart);

  tiny_gea3_interface_init(
    &gea3Interface,
    &streamUart.interface,
    clientAddress,
    sendQueueBuffer,
    sizeof(sendQueueBuffer),
    receiveBuffer,
    sizeof(receiveBuffer),
    false);

  clientConfiguration.request_timeout = requestTimeout;
  clientConfiguration.request_retries = requestRetries;

  tiny_gea3_erd_client_init(
    &erdClient,
    &timerGroup,
    &gea3Interface.interface,
    clientQueueBuffer,
    sizeof(clientQueueBuffer),
    &clientConfiguration);
}

void GEA3::loop()
{
  tiny_timer_group_run(&timerGroup);
  tiny_gea3_interface_run(&gea3Interface);
}

void GEA3::sendPacket(const GEA3::Packet& packet)
{
  auto rawPacket = packet.getRawPacket();

  struct Context {
    const tiny_gea_packet_t* rawPacket;
  };
  auto context = Context{ rawPacket };

  tiny_gea_interface_send(
    &gea3Interface.interface,
    rawPacket->destination,
    rawPacket->payload_length,
    &context,
    +[](void* _context, tiny_gea_packet_t* packet) {
      auto context = static_cast<Context*>(_context);
      memcpy(packet->payload, context->rawPacket->payload, context->rawPacket->payload_length);
    });
}

GEA3::PacketListener GEA3::onPacketReceived(void* context, void (*callback)(void* context, const GEA3::Packet& packet))
{
  auto subscription = new PrivatePacketListener{
    context,
    callback
  };

  tiny_event_subscription_init(
    &subscription->subscription, subscription, +[](void* context, const void* args_) {
      auto subscription = reinterpret_cast<PrivatePacketListener*>(context);
      auto args = reinterpret_cast<const tiny_gea_interface_on_receive_args_t*>(args_);
      auto packet = Packet(args->packet);
      subscription->callback(subscription->context, packet);
    });
  tiny_event_subscribe(tiny_gea_interface_on_receive(&gea3Interface.interface), &subscription->subscription);

  return PacketListener(subscription, &gea3Interface.interface);
}

GEA3::PacketListener GEA3::onPacketReceived(void (*callback)(const GEA3::Packet& packet))
{
  return onPacketReceived(
    reinterpret_cast<void*>(callback), +[](void* context, const GEA3::Packet& packet) {
      reinterpret_cast<void (*)(const GEA3::Packet& packet)>(context)(packet);
    });
}

void GEA3::readERDAsync(uint8_t address, uint16_t erd, void* context, void (*callback)(void* context, ReadStatus status, const void* value, uint8_t valueSize))
{
  tiny_gea3_erd_client_request_id_t requestId;
  tiny_gea3_erd_client_read(&erdClient.interface, &requestId, address, erd);

  auto subscription = new AsyncReadSubscription{ context, callback, requestId, &erdClient };

  tiny_event_subscription_init(
    &subscription->subscription, subscription, +[](void* context, const void* args_) {
      auto subscription = reinterpret_cast<AsyncReadSubscription*>(context);
      auto args = reinterpret_cast<const tiny_gea3_erd_client_on_activity_args_t*>(args_);

      if((args->type == tiny_gea3_erd_client_activity_type_read_completed) && (args->read_completed.request_id == subscription->requestId)) {
        subscription->callback(subscription->context, ReadStatus::success, args->read_completed.data, args->read_completed.data_size);
        tiny_event_unsubscribe(tiny_gea3_erd_client_on_activity(&subscription->erdClient->interface), &subscription->subscription);
        delete subscription;
      }
      else if((args->type == tiny_gea3_erd_client_activity_type_read_failed) && (args->read_failed.request_id == subscription->requestId)) {
        switch(args->read_failed.reason) {
          case tiny_gea3_erd_client_read_failure_reason_retries_exhausted:
            subscription->callback(subscription->context, ReadStatus::retriesExhausted, nullptr, 0);
            break;

          default:
          case tiny_gea3_erd_client_read_failure_reason_not_supported:
            subscription->callback(subscription->context, ReadStatus::notSupported, nullptr, 0);
            break;
        }

        tiny_event_unsubscribe(tiny_gea3_erd_client_on_activity(&subscription->erdClient->interface), &subscription->subscription);
        delete subscription;
      }
    });
  tiny_event_subscribe(tiny_gea3_erd_client_on_activity(&erdClient.interface), &subscription->subscription);
}

void GEA3::writeERDAsync(uint8_t address, uint16_t erd, const void* value, size_t valueSize, void* context, void (*callback)(void* context, WriteStatus status))
{
  tiny_gea3_erd_client_request_id_t requestId;
  tiny_gea3_erd_client_write(&erdClient.interface, &requestId, address, erd, value, valueSize);

  auto subscription = new AsyncWriteSubscription(context, callback, requestId, &erdClient);

  tiny_event_subscription_init(
    &subscription->subscription, subscription, +[](void* context, const void* args_) {
      auto subscription = reinterpret_cast<AsyncWriteSubscription*>(context);
      auto args = reinterpret_cast<const tiny_gea3_erd_client_on_activity_args_t*>(args_);

      if((args->type == tiny_gea3_erd_client_activity_type_write_completed) && (args->write_completed.request_id == subscription->requestId)) {
        subscription->callback(subscription->context, WriteStatus::success);
        tiny_event_unsubscribe(tiny_gea3_erd_client_on_activity(&subscription->erdClient->interface), &subscription->subscription);
        delete subscription;
      }
      else if((args->type == tiny_gea3_erd_client_activity_type_write_failed) && (args->write_failed.request_id == subscription->requestId)) {
        switch(args->write_failed.reason) {
          case tiny_gea3_erd_client_write_failure_reason_retries_exhausted:
            subscription->callback(subscription->context, WriteStatus::retriesExhausted);
            break;

          case tiny_gea3_erd_client_write_failure_reason_not_supported:
            subscription->callback(subscription->context, WriteStatus::notSupported);
            break;

          default:
          case tiny_gea3_erd_client_write_failure_reason_incorrect_size:
            subscription->callback(subscription->context, WriteStatus::incorrectSize);
            break;
        }

        tiny_event_unsubscribe(tiny_gea3_erd_client_on_activity(&subscription->erdClient->interface), &subscription->subscription);
        delete subscription;
      }
    });
  tiny_event_subscribe(tiny_gea3_erd_client_on_activity(&erdClient.interface), &subscription->subscription);
}

GEA3::WriteStatus GEA3::writeERD(uint8_t address, uint16_t erd, const void* value, size_t valueSize)
{
  struct Context {
    bool done;
    WriteStatus status;
  };

  Context context{ false, WriteStatus::success };

  writeERDAsync(
    address, erd, value, valueSize, reinterpret_cast<void*>(&context), +[](void* context_, WriteStatus status) {
      Context* context = reinterpret_cast<Context*>(context_);
      context->done = true;
      context->status = status;
    });

  while(!context.done) {
    loop();
  }

  return context.status;
}

GEA3::ErdSubscription GEA3::subscribe(uint8_t address, void* context, void (*callback)(void* context, uint16_t erd, const void* value, uint8_t valueSize))
{
  auto subscription = new PrivateErdSubscription(address, context, callback, &erdClient.interface, &timerGroup);

  tiny_event_subscription_init(
    &subscription->subscription, subscription, +[](void* context, const void* args_) {
      auto subscription = reinterpret_cast<PrivateErdSubscription*>(context);
      auto args = reinterpret_cast<const tiny_gea3_erd_client_on_activity_args_t*>(args_);

      if(args->address == subscription->address) {
        if(args->type == tiny_gea3_erd_client_activity_type_subscription_publication_received) {
          subscription->callback(
            subscription->context,
            args->subscription_publication_received.erd,
            args->subscription_publication_received.data,
            args->subscription_publication_received.data_size);
        }
        else if(args->type == tiny_gea3_erd_client_activity_type_subscription_host_came_online) {
          tiny_gea3_erd_client_retain_subscription(subscription->erdClient, subscription->address);
        }
      }
    });
  tiny_event_subscribe(tiny_gea3_erd_client_on_activity(&erdClient.interface), &subscription->subscription);

  tiny_timer_start_periodic(
    &timerGroup, &subscription->timer, 60 * 1000, subscription, +[](void* context) {
      auto subscription = reinterpret_cast<PrivateErdSubscription*>(context);
      tiny_gea3_erd_client_retain_subscription(subscription->erdClient, subscription->address);
    });

  tiny_gea3_erd_client_subscribe(&erdClient.interface, address);

  return ErdSubscription(subscription);
}

GEA3::ErdSubscription GEA3::subscribe(uint8_t address, void (*callback)(uint16_t erd, const void* value, uint8_t valueSize))
{
  return subscribe(
    address, reinterpret_cast<void*>(callback), +[](void* context, uint16_t erd, const void* value, uint8_t valueSize) {
      reinterpret_cast<void (*)(uint16_t, const void*, uint8_t)>(context)(erd, value, valueSize);
    });
}
