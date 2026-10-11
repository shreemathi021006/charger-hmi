#include <Wire.h>
#include <DIYables_LCD_I2C.h>

const int BUTTON_PIN = 18; // Button connected to GPIO 23 and GND
DIYables_LCD_I2C lcd(0x27, 16, 2);

int pressCount = 0; // Tracks the number of button presses

void setup() {
  Wire.begin(21, 22); // ESP32 SDA = GPIO 21, SCL = GPIO 22
  lcd.init();
  lcd.backlight();
  
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Initial message before any button press
  lcd.setCursor(0, 0);
  lcd.print("Press Button...");
}

void loop() {
  // Check if button is pressed
  if (digitalRead(BUTTON_PIN) == LOW) {
    pressCount++; // Increment count on press
    
    // Reset back to 1 after the 4th press
    if (pressCount > 3) {
      pressCount = 1;
    }

    lcd.clear(); // Clear old text
    
    // Display different lines based on the press count
    if (pressCount == 1) {
      lcd.setCursor(0, 0);
      lcd.print("1: M.Shreemathi");
      lcd.setCursor(0,1);
      lcd.print("EEE-B");
    } 


    else if (pressCount == 2) {
      lcd.setCursor(0, 0);
      lcd.print("3: Yashasvee");
      lcd.setCursor(0,1);
      lcd.print("EEE-B");
    } 
    else if (pressCount == 3){
      lcd.setCursor(0,0);
      lcd.print("SSN");
      lcd.setCursor(0,1);
      lcd.print("INNOVATION DAY");
    }
      
    

    delay(250); // Debounce delay to avoid multiple registers per click
  }
}
