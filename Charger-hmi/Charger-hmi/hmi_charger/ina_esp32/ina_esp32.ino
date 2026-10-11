#include <Wire.h>
#include <INA226.h>

INA226 ina226(0x40);

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(21, 22);  // SDA, SCL

  Serial.println("Starting INA226 test...");

  if (!ina226.begin()) {
    Serial.println("Couldn't find INA226 chip - check wiring!");
    while (1) delay(10);
  }

  Serial.println("INA226 found!");

  // Shunt voltage ADC on INA226 maxes out at ~81.92mV.
  // With a 0.1 ohm shunt, max measurable current = 81.92mV / 0.1 ohm = ~819mA.
  // So maxCurrent here MUST be <= ~0.8A for this shunt value, or calibration breaks.
  bool calOK = ina226.setMaxCurrentShunt(0.8, 0.1);  // 800mA max, 0.1 ohm shunt
  Serial.print("Calibration success: ");
  Serial.println(calOK ? "YES" : "NO");

  if (!calOK) {
    Serial.println("Calibration FAILED - check shunt/current values!");
  }

  Serial.println("Starting readings...\n");
}

void loop() {
  float busVoltage      = ina226.getBusVoltage();
  float shuntVoltage_mV = ina226.getShuntVoltage_mV();
  float current_mA      = ina226.getCurrent() * 1000;
  float power_mW        = ina226.getPower() * 1000;

  Serial.print("Bus V: ");
  Serial.print(busVoltage, 3);
  Serial.print(" V \t");

  Serial.print("Shunt V: ");
  Serial.print(shuntVoltage_mV, 4);
  Serial.print(" mV \t");

  Serial.print("Current: ");
  Serial.print(current_mA, 4);
  Serial.print(" mA \t");

  Serial.print("Power: ");
  Serial.print(power_mW, 3);
  Serial.println(" mW");

  delay(1000);
}
