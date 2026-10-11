#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32_MIT_APP");
  Serial.println("Bluetooth started. Pair with 'ESP32_MIT_APP'");
}

void loop() {
  if (SerialBT.available()) {
    String data = SerialBT.readStringUntil('\n'); // read one full message
    data.trim(); // remove any trailing \r or whitespace

    if (data.length() > 0) {
      int batteryLevel = data.toInt();
      Serial.print("Battery Level: ");
      Serial.println(batteryLevel);
    }
  }
}
