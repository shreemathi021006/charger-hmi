#include <Wire.h>
#include <DIYables_LCD_I2C.h>
#include <INA226.h>   // RobTillaart INA226 library — matches your tested reference sketch

// ---- Two SEPARATE I2C buses (ESP32 has two hardware I2C peripherals) ----
// LCD  -> Wire1 on SDA=4,  SCL=13
// INA226 -> Wire  on SDA=21, SCL=22
#define LCD_SDA_PIN 4
#define LCD_SCL_PIN 13
#define INA_SDA_PIN 21
#define INA_SCL_PIN 22
#define I2C_ADDRESS_INA226 0x44   // matches your tested reference sketch

// DIYables_LCD_I2C supports passing a specific TwoWire bus as the 4th arg
DIYables_LCD_I2C lcd(0x27, 16, 2, Wire1);

// RobTillaart's INA226 constructor: INA226(address, wire) — bound to the Wire bus (21/22)
INA226 ina226(I2C_ADDRESS_INA226, &Wire);

const int b1 = 16;   // select
const int b2 = 17;   // enter
const int b3 = 18;   // increment
const int b4 = 19;   // decrement
const int relay = 5;

// ---- Battery spec (still needed to convert measured mAh -> %) ----
const float BATTERY_CAPACITY_MAH = 6500.0;

int page = 0;
int mainChoice = 0;
int manualChoice = 0;

int currentPercent = 0;
int targetPercent = 0;
int targetTime = 0;

int atc = 0;   // target charge % (for percent-target modes)
int et = 0;    // target time in minutes (only used in time-target mode)

int returnPage = 2;

// ---- Charging state ----
unsigned long chargeStartTime = 0;
bool chargingActive = false;
bool timeBasedMode = false;   // true only when user explicitly entered a target TIME

// ---- Real coulomb counting via INA226 (replaces the old fixed-current assumption) ----
float accumulatedMah = 0.0;
unsigned long lastSampleMillis = 0;
float liveCurrentMA = 0.0;
float liveVoltageV = 0.0;
float estimatedPercent = 0.0;

unsigned long lastLcdUpdateMillis = 0;

void setup() {
  Wire1.begin(LCD_SDA_PIN, LCD_SCL_PIN);   // LCD's own bus
  Wire.begin(INA_SDA_PIN, INA_SCL_PIN);    // INA226's own bus

  lcd.init();
  lcd.backlight();

  if (!ina226.begin()) {
    lcd.setCursor(0, 0);
    lcd.print("INA226 not found");
    while (1) delay(10);
  }

  // Shunt voltage ADC on INA226 maxes out at ~81.92mV.
  // With a 0.1 ohm shunt, max measurable current = 81.92mV / 0.1 ohm = ~819mA.
  // NOTE: 0.8*0.1 = exactly 0.080V, right at the library's 80mV limit — due to
  // floating point rounding this can read as slightly OVER 0.080 and fail
  // calibration. Back off slightly (0.75A) to stay safely under the limit.
  // IMPORTANT: if your actual charging current can exceed this, raise it
  // (and/or use a smaller shunt) to match — otherwise readings clip and
  // coulomb counting below will be wrong.
  // NOTE: setMaxCurrentShunt() returns an ERROR CODE (int), not a plain
  // bool — 0 means success, nonzero means a specific error. Assigning it
  // straight into a bool is backwards (0 becomes false = looks like
  // failure even when calibration actually succeeded). Check against 0.
  int calResult = ina226.setMaxCurrentShunt(0.75, 0.1);  // 750mA max, 0.1 ohm shunt
  if (calResult != 0) {
    lcd.setCursor(0, 0);
    lcd.print("INA226 cal FAIL");
    lcd.setCursor(0, 1);
    lcd.print("Err code: ");
    lcd.print(calResult);
    while (1) delay(10);
  }

  pinMode(b1, INPUT_PULLUP);
  pinMode(b2, INPUT_PULLUP);
  pinMode(b3, INPUT_PULLUP);
  pinMode(b4, INPUT_PULLUP);
  pinMode(relay, OUTPUT);
  digitalWrite(relay, LOW);

  showMainMenu();
}

void loop() {

  // ---- Continuous INA226 sampling + coulomb counting (runs every loop) ----
  sampleINA226();

  // ---- Auto cutoff, now driven by REAL measured charge instead of a static formula ----
  if (chargingActive) {
    if (!timeBasedMode) {
      // Default-full and manual-target-% modes: stop once real accumulated
      // charge (from INA226 current integrated over time) reaches target
      if (estimatedPercent >= atc) {
        finishCharging();
        return;
      }
    } else {
      // Manual target-TIME mode: user asked for a duration directly, so
      // that stays time-based — but the screen still shows live current/%
      if (millis() - chargeStartTime >= (unsigned long)et * 60000UL) {
        finishCharging();
        return;
      }
    }

    // Refresh the live charging screen ~once a second without a full clear
    if (millis() - lastLcdUpdateMillis >= 1000) {
      lastLcdUpdateMillis = millis();
      showChargingStatus();
    }
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
      timeBasedMode = false;
      startCharging();
      page = 2;
      lcd.clear();
      showChargingStatus();
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
        timeBasedMode = false;
        startCharging();
        page = 7;
        lcd.clear();
        showChargingStatus();
      }
    }
  }

  else if (page == 6) {
    if (buttonPressed(b3)) { targetTime = min(targetTime + 1, 999); showEnterTargetTime(); }
    if (buttonPressed(b4)) { targetTime = max(targetTime - 1, 0); showEnterTargetTime(); }
    if (buttonPressed(b2)) {
      et = targetTime;
      timeBasedMode = true;
      startCharging();
      page = 7;
      lcd.clear();
      showChargingStatus();
    }
  }

  else if (page == 7) {
    if (buttonPressed(b1)) { returnPage = 7; page = 8; showInfo(); }
    if (buttonPressed(b2)) { stopCharging(); page = 0; showMainMenu(); }
  }

  else if (page == 8) {
    if (buttonPressed(b2)) {
      page = returnPage;   // returnPage is always 2 or 7 (a charging page)
      lcd.clear();
      showChargingStatus();
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

// Reads the INA226 and, while charging, integrates current over time
// (coulomb counting) to get real mAh delivered -> real % gained.
void sampleINA226() {
  unsigned long now = millis();
  liveVoltageV = ina226.getBusVoltage();
  liveCurrentMA = fabs(ina226.getCurrent() * 1000);   // getCurrent() returns Amps

  if (chargingActive && lastSampleMillis != 0) {
    float dtHours = (now - lastSampleMillis) / 3600000.0;
    accumulatedMah += liveCurrentMA * dtHours;
    estimatedPercent = currentPercent + (accumulatedMah / BATTERY_CAPACITY_MAH) * 100.0;
    if (estimatedPercent > 100) estimatedPercent = 100;
  }
  lastSampleMillis = now;
}

void startCharging() {
  digitalWrite(relay, HIGH);
  chargingActive = true;
  chargeStartTime = millis();
  accumulatedMah = 0.0;
  estimatedPercent = currentPercent;
  lastSampleMillis = millis();
}

void stopCharging() {
  digitalWrite(relay, LOW);
  chargingActive = false;
}

void finishCharging() {
  stopCharging();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Completed!");
  lcd.setCursor(0, 1);
  lcd.print("Thank you.");
  delay(3000);
  page = 0;
  showMainMenu();
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

// Live charging screen, replaces the old static showResult().
// Line 1: real-time estimated SoC (from INA226 coulomb counting) vs target
// Line 2: live current, plus either elapsed/target time or live voltage
void showChargingStatus() {
  lcd.setCursor(0, 0);
  lcd.print("SoC:");
  lcd.print((int)estimatedPercent);
  lcd.print("% Tgt:");
  lcd.print(atc);
  lcd.print("%   ");

  lcd.setCursor(0, 1);
  lcd.print(liveCurrentMA, 0);
  lcd.print("mA ");
  if (timeBasedMode) {
    unsigned long elapsedMin = (millis() - chargeStartTime) / 60000UL;
    lcd.print(elapsedMin);
    lcd.print("/");
    lcd.print(et);
    lcd.print("m   ");
  } else {
    lcd.print(liveVoltageV, 1);
    lcd.print("V   ");
  }
}

void showInfo() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("V:");
  lcd.print(liveVoltageV, 2);
  lcd.print("V");
  lcd.setCursor(0, 1);
  lcd.print("I:");
  lcd.print(liveCurrentMA, 1);
  lcd.print("mA");
}
