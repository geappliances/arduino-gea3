#include <Arduino.h>
#include <GEA3.h>

static GEA3 gea3;

void setup()
{
  Serial.begin(115200);

  Serial1.begin(GEA3::baud);

  gea3.begin(Serial1);

  gea3.writeERDAsync(
    0x0035, GEA3::U32(0x01ABCDEF), +[](GEA3::WriteStatus status) {
      if(status == GEA3::WriteStatus::success) {
        Serial.println("Wrote ERD 0x0035 asynchronously");
      }
      else {
        Serial.println("Failed to write ERD 0x0035 asynchronously");
      }
    });

  if(gea3.writeERD(0x0035, GEA3::U32(0x01ABCDEF)) == GEA3::WriteStatus::success) {
    Serial.println("Wrote ERD 0x0035");
  }
  else {
    Serial.println("Failed to write ERD 0x0035");
  }
}

void loop()
{
  gea3.loop();
}
