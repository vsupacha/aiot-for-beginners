// Wokwi (Arduino Uno): ขั้นที่ 1 อ่านค่า analog จาก potentiometer ที่ A0
// หมุน potentiometer ในหน้าจำลองแทนการเปลี่ยนความสว่าง (หมุนขวา = ค่ามาก = สว่างมาก)
// ต่างจากสเก็ตช์จริง: Arduino Uno มี ADC 10 บิตเสมอ จึงไม่ต้องเรียก analogReadResolution()

const int SENSOR_PIN = A0;

void setup() {
  Serial.begin(115200);
}

void loop() {
  int raw = analogRead(SENSOR_PIN);  // 0-1023
  Serial.print("light:");
  Serial.println(raw);
  delay(200);
}
