#include <DIYables_LCD_I2C.h>
#include <Wire.h>

DIYables_LCD_I2C lcd(0x27, 16, 2);

const int b1 = 16;   // select button
const int b2 = 17;   // enter button
const int relay = 5;

int page = 0;        // 0 = main menu, 1 = manual menu, 2 = default run, 3 = enter %, 4 = enter time
int mainChoice = 0;    // 0 = default, 1 = manual
int manualChoice = 0;  // 0 = by %, 1 = by time

void setup() {
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();

  pinMode(b1, INPUT_PULLUP);
  pinMode(b2, INPUT_PULLUP);
  pinMode(relay, OUTPUT);
  digitalWrite(relay, LOW);

  showMainMenu();
}

void loop() {

  // ---- Page 0: Main Menu (Default / Manual) ----
  if (page == 0) {
    if (buttonPressed(b1)) {
      mainChoice = 1 - mainChoice;   // flips between 0 and 1
      showMainMenu();
    }
    if (buttonPressed(b2)) {
      if (mainChoice == 0) {
        page = 2;              // go to Default run page
        showDefaultRun();
      } else {
        page = 1;              // go to Manual menu
        showManualMenu();
      }
    }
  }

  // ---- Page 1: Manual Menu (Enter % / Enter Time) ----
  else if (page == 1) {
    if (buttonPressed(b1)) {
      manualChoice = 1 - manualChoice;
      showManualMenu();
    }
    if (buttonPressed(b2)) {
      if (manualChoice == 0) {
        page = 3;              // Enter % page
        showEnterPercent();
      } else {
        page = 4;              // Enter time page
        showEnterTime();
      }
    }
  }

  // Pages 2, 3, 4 are end screens for now (nothing happens on button press)
}

// ---------------- Helper: detects a single press (not held) ----------------
bool buttonPressed(int pin) {
  if (digitalRead(pin) == LOW) {
    delay(200);              // simple debounce, waits out the press
    while (digitalRead(pin) == LOW);  // wait until button is released
    return true;
  }
  return false;
}

// ---------------- Screens ----------------

void showMainMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  if (mainChoice == 0) lcd.print("1.Default <-");
  else lcd.print("1.Default");

  lcd.setCursor(0, 1);
  if (mainChoice == 1) lcd.print("2.Manual <-");
  else lcd.print("2.Manual");
}

void showManualMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  if (manualChoice == 0) lcd.print("1.Enter % <-");
  else lcd.print("1.Enter %");

  lcd.setCursor(0, 1);
  if (manualChoice == 1) lcd.print("2.Enter Time <-");
  else lcd.print("2.Enter Time");
}

void showDefaultRun() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("CHARGE 80%");
  lcd.setCursor(0, 1);
  lcd.print("EST TIME: --");
  digitalWrite(relay, HIGH);
}

void showEnterPercent() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enter percentage");
  digitalWrite(relay, HIGH);
}

void showEnterTime() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enter time");
  digitalWrite(relay, HIGH);
}
