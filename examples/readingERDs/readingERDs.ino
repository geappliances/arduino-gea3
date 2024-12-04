#include <Arduino.h>
#include <GEA3.h>

static GEA3 gea3;

void setup()
{
  Serial.begin(115200);

  Serial1.begin(GEA3::baud);

  gea3.begin(Serial1);

  gea3.readERDAsync(
    0x0035, +[](GEA3::ReadStatus status, GEA3::U32 value) {
      if(status == GEA3::ReadStatus::success) {
        Serial.printf("Successfully read ERD 0x0035 asynchronously: 0x%08X\n", value.read());
      }
      else {
        Serial.printf("Failed to read ERD 0x0035 asynchronously\n");
      }
    });

  auto result = gea3.readERD<GEA3::U32>(0x0035);
  if(result.status == GEA3::ReadStatus::success) {
    Serial.printf("Successfully read ERD 0x0035: 0x%08X\n", value.read());
  }
  else {
    Serial.printf("Failed to read ERD 0x0035\n");
  }
}

void loop()
{
  gea3.loop();
}
