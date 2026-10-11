#include <Wire.h>
#include <DIYables_LCD_I2C.h>
#include <INA226.h>          // RobTillaart INA226 library
#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Enable it in "Tools -> Board -> Enable Bluetooth"
#endif

BluetoothSerial SerialBT;

// ---- Two SEPARATE I2C buses (ESP32 has two hardware I2C peripherals) ----
// LCD    -> Wire1 on SDA=4,  SCL=13
// INA226 -> Wire  on SDA=21, SCL=22
#define LCD_SDA_PIN 4
#define LCD_SCL_PIN 13
#define INA_SDA_PIN 21
#define INA_SCL_PIN 22
#define I2C_ADDRESS_INA226 0x44

DIYables_LCD_I2C lcd(0x27, 16, 2, Wire1);
INA226 ina226(I2C_ADDRESS_INA226, &Wire);

const int b1 = 16;   // select
const int b2 = 17;   // enter
const int b3 = 18;   // increment
const int b4 = 19;   // decrement
const int relay = 5;

// Fixed target used by "Default" mode (previously the code implied ~80%,
// so that's kept as the hard-coded default target here).
const int DEFAULT_TARGET_PERCENT = 80;

int page = 0;
int mainChoice = 0;
int manualChoice = 0;

int currentPercent = 0;  // user-entered starting % (Default and Manual flows)
int targetPercent = 0;   // used only in Manual -> Enter % flow
int targetTime = 0;      // used only in Manual -> Enter Time flow

int atc = 0;   // active target charge % (percent-target modes)
int et = 0;    // active target time in minutes (time-target mode)

int returnPage = 2;

// ---- Charging state ----
unsigned long chargeStartTime = 0;
bool chargingActive = false;
bool timeBasedMode = false;   // true only when user explicitly entered a target TIME

// ---- INA226 readings — now used ONLY for the mA / V display, not for
//      deciding when to stop charging ----
float liveCurrentMA = 0.0;
float liveVoltageV = 0.0;

// ---- Battery % now comes from the phone over Bluetooth (MIT App Inventor
//      sends the phone's battery level, one integer per line, e.g. "57\n") ----
int phoneBatteryPercent = -1;         // -1 = no reading received yet
unsigned long lastBtDataMillis = 0;

unsigned long lastLcdUpdateMillis = 0;

void setup() {
  Serial.begin(115200);

  Wire1.begin(LCD_SDA_PIN, LCD_SCL_PIN);   // LCD's own bus
  Wire.begin(INA_SDA_PIN, INA_SCL_PIN);    // INA226's own bus

  lcd.init();
  lcd.backlight();

  if (!ina226.begin()) {
    lcd.setCursor(0, 0);
    lcd.print("INA226 not found");
    while (1) delay(10);
  }

  // 750mA max, 0.1 ohm shunt (see original notes on the 80mV ADC ceiling)
  int calResult = ina226.setMaxCurrentShunt(0.75, 0.1);
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

  SerialBT.begin("ESP32_MIT_APP");
  Serial.println("Bluetooth started. Pair with 'ESP32_MIT_APP'");

  showMainMenu();
}

void loop() {

  readPhoneBatteryPercent();   // keep phoneBatteryPercent fresh, always
  sampleINA226();              // keep mA / V fresh for the LCD, always

  // ---- Auto cutoff, now driven by the phone's real battery % over BT ----
  if (chargingActive) {
    if (!timeBasedMode) {
      // Default mode and Manual->Enter% mode: stop once the phone reports
      // it has reached (or passed) the target percentage.
      if (phoneBatteryPercent >= 0 && phoneBatteryPercent >= atc) {
        finishCharging();
        return;
      }
    } else {
      // Manual -> Enter Time mode: unchanged, still purely time-based.
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
      // ---- Default: fixed target %, checked live via Bluetooth ----
      atc = DEFAULT_TARGET_PERCENT;
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

// Reads one line ("<percent>\n") from the phone over Bluetooth and updates
// phoneBatteryPercent. Runs every loop iteration regardless of page/state,
// so the value is always as fresh as possible.
void readPhoneBatteryPercent() {
  if (SerialBT.available()) {
    String data = SerialBT.readStringUntil('\n');
    data.trim();   // strip stray \r or whitespace
    if (data.length() > 0) {
      int val = data.toInt();
      if (val >= 0 && val <= 100) {
        phoneBatteryPercent = val;
        lastBtDataMillis = millis();
        Serial.print("Phone battery %: ");
        Serial.println(phoneBatteryPercent);
      }
    }
  }
}

// Reads the INA226 for live current/voltage. Used purely for the LCD
// display now — no longer used to estimate charge percentage.
void sampleINA226() {
  liveVoltageV = ina226.getBusVoltage();
  liveCurrentMA = fabs(ina226.getCurrent() * 1000);   // getCurrent() returns Amps
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

// Live charging screen.
// Line 1: phone-reported battery % (via Bluetooth) vs target
// Line 2: live current (mA), plus either elapsed/target time or live voltage
void showChargingStatus() {
  lcd.setCursor(0, 0);
  lcd.print("Bat:");
  if (phoneBatteryPercent >= 0) {
    lcd.print(phoneBatteryPercent);
  } else {
    lcd.print("--");
  }
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
