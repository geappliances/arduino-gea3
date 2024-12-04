#include <Arduino.h>
#include "GEA3.h"

static GEA3 gea3;

void setup()
{
  Serial.begin(115200);

  Serial1.begin(GEA3::baud);

  gea3.begin(Serial1);

  gea3.subscribe(
    +[](uint16_t erd, const void* value, uint8_t valueSize) {
      switch(erd) {
        case 0x0001: {
          auto model_number = reinterpret_cast<const char*>(value);
          Serial.printf("Model Number: %.32s\n", model_number);
        } break;

        case 0x0002: {
          auto serial_number = reinterpret_cast<const char*>(value);
          Serial.printf("Serial Number: %.32s\n", serial_number);
        } break;

        case 0x0008: {
          auto appliance_type = reinterpret_cast<const GEA3::U8*>(value);
          Serial.printf("Appliance Type: %d\n", appliance_type->read());
        } break;

        case 0x0035: {
          auto personality = reinterpret_cast<const GEA3::U32*>(value);
          Serial.printf("Appliance Personality: %d\n", personality->read());
        } break;
      }
    });
}

void loop()
{
  gea3.loop();
}
