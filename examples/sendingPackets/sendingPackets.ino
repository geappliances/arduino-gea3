#include <Arduino.h>
#include <GEA3.h>

static GEA3 gea3;

void setup()
{
  Serial.begin(115200);

  Serial1.begin(GEA3::baud);

  gea3.begin(Serial1);

  gea3.sendPacket(GEA3::Packet(0xE4, 0xFF, { 0x01 }));
}

void loop()
{
  gea3.loop();
}
