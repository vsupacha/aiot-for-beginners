// ขั้นที่ 2: ไฟกลางคืนแบบทำงานบนบอร์ดอย่างเดียว (ยังไม่ต่อ Wi-Fi)
// มืดกว่าเกณฑ์ -> เปิด LED บนบอร์ด, สว่างพอ -> ปิด
// ต่อวงจรเหมือนขั้นที่ 1 ไม่ต้องต่อ LED ภายนอก

const int SENSOR_PIN = A0;
const int LED_PIN = LED_BUILTIN;
const int ON_BELOW = 358;  // ประมาณ 35% ของ 1023

void setup() {
  Serial.begin(115200);
  analogReadResolution(10);
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
