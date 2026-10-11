#include <Wire.h>
#include <INA226.h>
#include <DIYables_LCD_I2C.h>

INA226 ina226(0x44);
DIYables_LCD_I2C lcd(0x19, 16, 2);

const float SHUNT_RESISTANCE = 100;  // ohms - must match your actual shunt

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(4,13);  // SDA, SCL - shared bus for INA226 + LCD

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Starting...");

  Serial.println("Starting INA226 test...");

  if (!ina226.begin()) {
    Serial.println("Couldn't find INA226 chip - check wiring!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("INA226 not");
    lcd.setCursor(0, 1);
    lcd.print("found!");
    while (1) delay(10);
  }

  bool calOK = ina226.setMaxCurrentShunt(0.8, SHUNT_RESISTANCE);
  Serial.print("Calibration success: ");
  Serial.println(calOK ? "YES" : "NO");

  lcd.clear();
  Serial.println("Starting readings...\n");
}

void loop() {
  float busVoltage      = ina226.getBusVoltage();          // V
  float shuntVoltage_mV = ina226.getShuntVoltage_mV();     // mV

  // Manual current calc via Ohm's Law (library's getCurrent() was unreliable)
  float current_mA = shuntVoltage_mV / SHUNT_RESISTANCE;   // mV / ohm = mA
  float power_mW   = busVoltage * (current_mA / 1000.0) * 1000.0;

  // ---- Serial output ----
  Serial.print("Bus V: ");
  Serial.print(busVoltage, 3);
  Serial.print(" V \t");
  Serial.print("Current: ");
  Serial.print(current_mA, 2);
  Serial.print(" mA \t");
  Serial.print("Power: ");
  Serial.print(power_mW, 2);
  Serial.println(" mW");

  // ---- LCD output ----
  // Line 1: Voltage and Current
  lcd.setCursor(0, 0);
  lcd.print("V:");
  lcd.print(busVoltage, 2);
  lcd.print("V I:");
  lcd.print(current_mA, 0);
  lcd.print("mA   ");  // trailing spaces to clear leftover chars from longer numbers

  // Line 2: Power
  lcd.setCursor(0, 1);
  lcd.print("P:");
  lcd.print(power_mW, 1);
  lcd.print("mW        ");  // trailing spaces to clear leftover chars

  delay(1000);
}
