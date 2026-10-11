#include <DIYables_LCD_I2C.h>
#include <Wire.h>

DIYables_LCD_I2C lcd(0x27, 16, 2);

const int b1 = 16;   // select
const int b2 = 17;   // enter
const int b3 = 18;   // increment
const int b4 = 19;   // decrement
const int relay = 5;

// ---- Battery/charger specs ----
const float BATTERY_CAPACITY_MAH = 6500.0;
const float CHARGE_CURRENT_MA = 3000.0;   // 5V/3A charger

int page = 0;
int mainChoice = 0;
int manualChoice = 0;

int currentPercent = 0;
int targetPercent = 0;
int targetTime = 0;

int atc = 0;   // target charge % (computed by algorithm)
int et = 0;    // estimated time in minutes (computed by algorithm)

int returnPage = 2;

// ---- Auto cutoff timer ----
unsigned long chargeStartTime = 0;
bool chargingActive = false;

void setup() {
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();

  pinMode(b1, INPUT_PULLUP);
  pinMode(b2, INPUT_PULLUP);
  pinMode(b3, INPUT_PULLUP);
  pinMode(b4, INPUT_PULLUP);
  pinMode(relay, OUTPUT);
  digitalWrite(relay, LOW);

  showMainMenu();
}

void loop() {

  // ---- Auto cutoff check: runs every loop, regardless of which page is open ----
  if (chargingActive && (millis() - chargeStartTime >= (unsigned long)et * 60000UL)) {
    chargingActive = false;
    digitalWrite(relay, LOW);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Completed!");
    lcd.setCursor(0, 1);
    lcd.print("Thank you.");
    delay(3000);
    page = 0;
    showMainMenu();
    return; // skip the rest of loop this cycle
  }

  if (page == 0) {
    if (buttonPressed(b1)) { mainChoice = 1 - mainChoice; showMainMenu(); }
    if (buttonPressed(b2)) {
      currentPercent = 0;
      if (mainChoice == 0) { page = 1; showEnterCurrentDefault(); }
      else { page = 3; showEnterCurrentManual(); }
    }
  }

  else if (page == 1) {
    if (buttonPressed(b3)) { currentPercent = min(currentPercent + 1, 100); showEnterCurrentDefault(); }
    if (buttonPressed(b4)) { currentPercent = max(currentPercent - 1, 0); showEnterCurrentDefault(); }
    if (buttonPressed(b2)) {
      atc = 100;
      et = calcTimeToFull(currentPercent);        // <-- algorithm computes cutoff time
      startCharging();
      page = 2;
      showResult();
    }
  }

  else if (page == 2) {
    if (buttonPressed(b1)) { returnPage = 2; page = 8; showInfo(); }
    if (buttonPressed(b2)) { stopCharging(); page = 0; showMainMenu(); }
  }

  else if (page == 3) {
    if (buttonPressed(b3)) { currentPercent = min(currentPercent + 1, 100); showEnterCurrentManual(); }
    if (buttonPressed(b4)) { currentPercent = max(currentPercent - 1, 0); showEnterCurrentManual(); }
    if (buttonPressed(b2)) { page = 4; showManualMenu(); }
  }

  else if (page == 4) {
    if (buttonPressed(b1)) { manualChoice = 1 - manualChoice; showManualMenu(); }
    if (buttonPressed(b2)) {
      if (manualChoice == 0) { targetPercent = currentPercent; page = 5; showEnterTargetPercent(); }
      else { targetTime = 0; page = 6; showEnterTargetTime(); }
    }
  }

  else if (page == 5) {
    if (buttonPressed(b3)) { targetPercent = min(targetPercent + 1, 100); showEnterTargetPercent(); }
    if (buttonPressed(b4)) { targetPercent = max(targetPercent - 1, 0); showEnterTargetPercent(); }
    if (buttonPressed(b2)) {
      if (targetPercent <= currentPercent) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Target must be");
        lcd.setCursor(0, 1);
        lcd.print("> current %");
        delay(1500);
        showEnterTargetPercent();
      } else {
        atc = targetPercent;
        et = calcTimeBetween(currentPercent, targetPercent);  // <-- algorithm computes cutoff time
        startCharging();
        page = 7;
        showResult();
      }
    }
  }

  else if (page == 6) {
    if (buttonPressed(b3)) { targetTime = min(targetTime + 1, 999); showEnterTargetTime(); }
    if (buttonPressed(b4)) { targetTime = max(targetTime - 1, 0); showEnterTargetTime(); }
    if (buttonPressed(b2)) {
      et = targetTime;                                        // user-given cutoff time
      atc = calcPercentFromTime(currentPercent, targetTime);  // algorithm computes resulting %
      startCharging();
      page = 7;
      showResult();
    }
  }

  else if (page == 7) {
    if (buttonPressed(b1)) { returnPage = 7; page = 8; showInfo(); }
    if (buttonPressed(b2)) { stopCharging(); page = 0; showMainMenu(); }
  }

  else if (page == 8) {
    if (buttonPressed(b2)) {
      page = returnPage;
      showResult();
    }
  }
}

// ---------------- Helpers ----------------
bool buttonPressed(int pin) {
  if (digitalRead(pin) == LOW) {
    delay(200);
    while (digitalRead(pin) == LOW);
    return true;
  }
  return false;
}

void startCharging() {
  digitalWrite(relay, HIGH);
  chargingActive = true;
  chargeStartTime = millis();
}

void stopCharging() {
  digitalWrite(relay, LOW);
  chargingActive = false;
}

// ================= CALCULATION FUNCTIONS =================
int calcTimeToFull(int fromPct) {
  return calcTimeBetween(fromPct, 100);
}

int calcTimeBetween(int fromPct, int toPct) {
  float totalMinutes = 0;
  int ccEnd = min(toPct, 80);

  if (fromPct < ccEnd) {
    float pctChunk = ccEnd - fromPct;
    totalMinutes += (BATTERY_CAPACITY_MAH * (pctChunk / 100.0) / CHARGE_CURRENT_MA) * 60.0;
  }
  if (toPct > 80) {
    int cvStart = max(fromPct, 80);
    float pctChunk = toPct - cvStart;
    totalMinutes += (BATTERY_CAPACITY_MAH * (pctChunk / 100.0) / CHARGE_CURRENT_MA) * 60.0 * 1.8;
  }
  return (int)totalMinutes;
}

int calcPercentFromTime(int fromPct, int minutesGiven) {
  int pct = fromPct;
  float minutesUsed = 0;
  while (pct < 100 && minutesUsed < minutesGiven) {
    float rate = (pct < 80) ? 1.0 : 1.8;
    float minutesPerPercent = (BATTERY_CAPACITY_MAH * 0.01 / CHARGE_CURRENT_MA) * 60.0 * rate;
    minutesUsed += minutesPerPercent;
    if (minutesUsed <= minutesGiven) pct++;
  }
  return pct;
}

// ================= SCREENS =================

void showMainMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(mainChoice == 0 ? "1.Default <-" : "1.Default");
  lcd.setCursor(0, 1);
  lcd.print(mainChoice == 1 ? "2.Manual <-" : "2.Manual");
}

void showEnterCurrentDefault() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enter Current%:");
  lcd.setCursor(0, 1);
  lcd.print(currentPercent);
  lcd.print("%");
}

void showEnterCurrentManual() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enter Current%:");
  lcd.setCursor(0, 1);
  lcd.print(currentPercent);
  lcd.print("%");
}

void showManualMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(manualChoice == 0 ? "1.Enter % <-" : "1.Enter %");
  lcd.setCursor(0, 1);
  lcd.print(manualChoice == 1 ? "2.Enter Time <-" : "2.Enter Time");
}

void showEnterTargetPercent() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Target %:");
  lcd.setCursor(0, 1);
  lcd.print(targetPercent);
  lcd.print("%");
}

void showEnterTargetTime() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Target Time(m):");
  lcd.setCursor(0, 1);
  lcd.print(targetTime);
}

void showResult() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ATC:");
  lcd.print(atc);
  lcd.print(" ET:");
  lcd.print(et);
  lcd.setCursor(0, 1);
  lcd.print("SEL=Info ENT=Exit");
}

void showInfo() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Power");
  lcd.setCursor(0, 1);
  lcd.print("Voltage");
}
