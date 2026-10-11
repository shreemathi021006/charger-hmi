#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <INA226.h>          // RobTillaart INA226 library
#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Enable it in "Tools -> Board -> Enable Bluetooth"
#endif

BluetoothSerial SerialBT;

// ---- Two SEPARATE I2C buses (ESP32 has two hardware I2C peripherals) ----
// OLED   -> Wire1 on SDA=4,  SCL=13   (confirmed addr 0x3C)
// INA226 -> Wire  on SDA=21, SCL=22
#define OLED_SDA_PIN 4
#define OLED_SCL_PIN 13
#define INA_SDA_PIN 21
#define INA_SCL_PIN 22
#define I2C_ADDRESS_INA226 0x44

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C   // confirmed working address

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, OLED_RESET);
INA226 ina226(I2C_ADDRESS_INA226, &Wire);

// ---- Thin wrapper so the rest of the file can keep using lcd.clear() /
// lcd.setCursor(col,row) / lcd.print(...) exactly as before, now drawing to
// the OLED. Rows 0/1 map to a two-line layout; textSize(1) gives 21 columns.
struct OledLCD {
  void init() {}       // display.begin() is called explicitly in setup()
  void backlight() {}  // no-op, OLED has no backlight pin
  void clear() {
    display.clearDisplay();
    display.display();
  }
  void setCursor(int col, int row) {
    display.setCursor(col * 6, row * 16);
  }
  template <typename T>
  void print(T val) {
    display.print(val);
    display.display();
  }
  // Supports lcd.print(floatValue, decimals)
  void print(float val, int digits) {
    display.print(val, digits);
    display.display();
  }
} lcd;

const int b1 = 16;   // select
const int b2 = 17;   // enter
const int b3 = 18;   // increment
const int b4 = 19;   // decrement
const int relay = 5;

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

// ---- INA226 readings: used ONLY for the mA / V display ----
float liveCurrentMA = 0.0;
float liveVoltageV = 0.0;

// ---- Battery % from the phone over Bluetooth (one integer per line) ----
int phoneBatteryPercent = -1;         // -1 = no reading received yet
unsigned long lastBtDataMillis = 0;

unsigned long lastLcdUpdateMillis = 0;

void setup() {
  Serial.begin(115200);

  Wire1.begin(OLED_SDA_PIN, OLED_SCL_PIN);   // OLED's own bus
  Wire.begin(INA_SDA_PIN, INA_SCL_PIN);      // INA226's own bus

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED not found - check wiring/address");
    while (1) delay(10);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.display();

  lcd.init();
  lcd.backlight();

  if (!ina226.begin()) {
    lcd.setCursor(0, 0);
    lcd.print("INA226 not found");
    while (1) delay(10);
  }

  // 750mA max, 0.1 ohm shunt
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
  sampleINA226();              // keep mA / V fresh for the display, always

  // ---- Auto cutoff, driven by the phone's battery % over BT ----
  if (chargingActive) {
    if (!timeBasedMode) {
      if (phoneBatteryPercent >= 0 && phoneBatteryPercent >= atc) {
        finishCharging();
        return;
      }
    } else {
      if (millis() - chargeStartTime >= (unsigned long)et * 60000UL) {
        finishCharging();
        return;
      }
    }

    // Refresh the live charging screen ~once a second, ONLY on charging pages
    // (so the Info screen on page 8 is not overwritten)
    if ((page == 2 || page == 7) && millis() - lastLcdUpdateMillis >= 1000) {
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
      page = returnPage;   // always 2 or 7 (a charging page)
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

// Reads one line ("<percent>\n") from the phone over Bluetooth
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

// Reads the INA226 for live current/voltage (display only)
void sampleINA226() {
  liveVoltageV = ina226.getBusVoltage();
  liveCurrentMA = fabs(ina226.getCurrent() * 1000);   // getCurrent() returns Amps

  // Uncomment to debug voltage readings in the Serial Monitor:
  // Serial.printf("Bus: %.3f V | Current: %.1f mA\n", liveVoltageV, liveCurrentMA);
}

void startCharging() {
  digitalWrite(relay, HIGH);
  chargingActive = true;
  chargeStartTime = millis();
  lastLcdUpdateMillis = millis();
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
// Line 2: live current (mA), live voltage (V), and elapsed/target time in time mode
void showChargingStatus() {
  // Blank both rows' pixel bands first so stale digits don't linger
  display.fillRect(0, 0, SCREEN_WIDTH, 16, SSD1306_BLACK);
  display.fillRect(0, 16, SCREEN_WIDTH, 16, SSD1306_BLACK);

  lcd.setCursor(0, 0);
  lcd.print("Bat:");
  if (phoneBatteryPercent >= 0) {
    lcd.print(phoneBatteryPercent);
  } else {
    lcd.print("--");
  }
  lcd.print("% Tgt:");
  lcd.print(atc);
  lcd.print("%");

  lcd.setCursor(0, 1);
  lcd.print(liveCurrentMA, 0);
  lcd.print("mA ");
  lcd.print(liveVoltageV, 1);
  lcd.print("V");
  if (timeBasedMode) {
    unsigned long elapsedMin = (millis() - chargeStartTime) / 60000UL;
    lcd.print(" ");
    lcd.print(elapsedMin);
    lcd.print("/");
    lcd.print(et);
    lcd.print("m");
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
