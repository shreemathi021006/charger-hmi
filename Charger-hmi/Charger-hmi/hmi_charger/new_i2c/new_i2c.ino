#include <Wire.h>

void scan(TwoWire &bus, const char *name) {
  Serial.print("Scanning "); Serial.println(name);
  int found = 0;
  for (byte a = 1; a < 127; a++) {
    bus.beginTransmission(a);
    if (bus.endTransmission() == 0) {
      Serial.print("  Found 0x");
      if (a < 16) Serial.print("0");
      Serial.println(a, HEX);
      found++;
    }
  }
  if (!found) Serial.println("  Nothing found");
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);    // INA219 pins
  Wire1.begin(4, 13);    // LCD pins
}

void loop() {
  scan(Wire,  "Wire  (SDA 21, SCL 22)");
  scan(Wire1, "Wire1 (SDA 4, SCL 13)");
  Serial.println("---");
  delay(3000);
}
