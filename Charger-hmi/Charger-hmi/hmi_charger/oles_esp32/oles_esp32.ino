#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SDA_PIN 4
#define SCL_PIN 13

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64   // change to 32 if yours is a 128x32 module
#define OLED_RESET -1      // no dedicated reset pin on most I2C OLEDs
#define OLED_ADDR 0x3C     // common default; some modules use 0x3D

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  Serial.begin(115200);
  delay(300);

  Wire.begin(SDA_PIN, SCL_PIN);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED not found — check wiring/address (try 0x3D if 0x3C fails)");
    while (1) delay(10);
  }

  Serial.println("OLED found and initialized.");

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println("OLED OK");

  display.setTextSize(1);
  display.setCursor(0, 20);
  display.println("SDA=4  SCL=13");
  display.setCursor(0, 32);
  display.println("ESP32 I2C test");

  display.display();
}

void loop() {
  // Simple visible heartbeat so you can confirm the display keeps updating,
  // not just that it drew once at boot.
  static unsigned long lastUpdate = 0;
  static int counter = 0;

  if (millis() - lastUpdate >= 1000) {
    lastUpdate = millis();
    counter++;

    display.fillRect(0, 48, SCREEN_WIDTH, 16, SSD1306_BLACK); // clear bottom line only
    display.setCursor(0, 50);
    display.setTextSize(1);
    display.print("Uptime: ");
    display.print(counter);
    display.print("s");
    display.display();
  }
}
