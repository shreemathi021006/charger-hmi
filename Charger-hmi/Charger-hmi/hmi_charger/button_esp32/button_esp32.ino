#define BUTTON1_PIN 16
#define BUTTON2_PIN 17
#define BUTTON3_PIN 18
#define BUTTON4_PIN 19
#define LED_PIN 2   // onboard LED

void setup() {
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  pinMode(BUTTON3_PIN, INPUT_PULLUP);
  pinMode(BUTTON4_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  if (digitalRead(BUTTON1_PIN) == LOW ||
      digitalRead(BUTTON2_PIN) == LOW ||
      digitalRead(BUTTON3_PIN) == LOW ||
      digitalRead(BUTTON4_PIN) == LOW) {
    digitalWrite(LED_PIN, HIGH);   // LED glows
  } else {
    digitalWrite(LED_PIN, LOW);    // LED off
  }
}
