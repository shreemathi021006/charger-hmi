#include <Wire.h>
#include <DIYables_LCD_I2C.h>
#include <INA226.h>

#define LCD_SDA_PIN 4
#define LCD_SCL_PIN 13
#define INA_SDA_PIN 21
#define INA_SCL_PIN 22

DIYables_LCD_I2C lcd(0x27, 16, 2, Wire1);
INA226 ina226(0x40, &Wire);

void setup() {
  Serial.begin(115200);

  Wire1.begin(LCD_SDA_PIN, LCD_SCL_PIN);   // LCD bus
  Wire.begin(INA_SDA_PIN, INA_SCL_PIN);    // INA226 bus

  lcd.init();
  lcd.backlight();

  if (!ina226.begin()) {
    lcd.setCursor(0, 0);
    lcd.print("INA226 not found");
    while (1) delay(10);
  }

  int calResult = ina226.setMaxCurrentShunt(0.75, 0.1);  // 750mA max, 0.1 ohm shunt
  if (calResult != 0) {
    lcd.setCursor(0, 0);
    lcd.print("INA226 cal FAIL");
    while (1) delay(10);
  }
}

void loop() {
  float voltageV  = ina226.getBusVoltage();
  float currentMA = ina226.getCurrent() * 1000;   // getCurrent() returns Amps

  lcd.setCursor(0, 0);
  lcd.print("V: ");
  lcd.print(voltageV, 3);
  lcd.print(" V     ");   // trailing spaces erase leftover characters

  lcd.setCursor(0, 1);
  lcd.print("I: ");
  lcd.print(currentMA, 1);
  lcd.print(" mA     ");

  delay(500);
}
