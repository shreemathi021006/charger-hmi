const int but1 = 16;
const int relay = 5 ;
void setup(){
  Serial.begin(115200);
  pinMode(but1,INPUT_PULLUP);
  pinMode(relay,OUTPUT);
  digitalWrite(relay,LOW);
}

void loop(){
  if (digitalRead(but1) == LOW){
    digitalWrite(relay,LOW);
    Serial.println("worked");
  }
  else {
    digitalWrite(relay,HIGH);
  }
  delay(250);
}
