// Wokwi (Arduino Uno): ขั้นที่ 2 ไฟกลางคืนที่ทำงานบนบอร์ดอย่างเดียว
// มืดกว่าเกณฑ์ (หมุน potentiometer ไปทางซ้าย) -> LED บนบอร์ด (ขา 13, ตัวอักษร L) ติด

const int SENSOR_PIN = A0;
const int LED_PIN = LED_BUILTIN;
const int ON_BELOW = 358;  // ประมาณ 35% ของ 1023

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  int raw = analogRead(SENSOR_PIN);
  bool lampOn = raw < ON_BELOW;
  digitalWrite(LED_PIN, lampOn ? HIGH : LOW);
  Serial.print("light:");
  Serial.print(raw);
  Serial.print(" lamp:");
  Serial.println(lampOn ? 1 : 0);
  delay(200);
}
