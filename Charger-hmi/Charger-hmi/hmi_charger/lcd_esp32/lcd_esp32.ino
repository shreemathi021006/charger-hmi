#include <DIYables_LCD_I2C.h>
#include <Wire.h>

//pinout lcd_vcc = esp32_vin ; lcd_gnd = esp32_gnd; lcd_sda = esp32_d21; lcd_scl = esp32_d22;

DIYables_LCD_I2C lcd (0x27, 16, 2);

void setup(){
  Wire.begin(4,13);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("Yashasvee");
  lcd.setCursor(0,1);
  lcd.print("EEE-B");
  
  
}
void loop(){
  
}
