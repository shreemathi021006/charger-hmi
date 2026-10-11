#define RELAY_PIN 5   // D5 = GPIO5

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);  // OFF (active-low module)
}

void loop() {
  digitalWrite(RELAY_PIN, LOW);   // Relay ON
  Serial.println("on");
  delay(3000);

  digitalWrite(RELAY_PIN, HIGH);  // Relay OFF
  Serial.println("off");
  delay(3000);
}
