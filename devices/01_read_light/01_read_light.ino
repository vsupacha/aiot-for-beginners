// ขั้นที่ 1: อ่านเซนเซอร์แบบ analog ที่ขา A0 แล้วแสดงค่าใน Serial Monitor / Serial Plotter
// ต่อ potentiometer (ขากลาง -> A0, ขานอก -> 3.3V และ GND) หรือเซนเซอร์แสงแบบ analog
// ค่ามาก = สว่างมาก (ถ้าใช้ LDR แล้วค่าตรงกันข้าม ดูหมายเหตุในเอกสาร)

const int SENSOR_PIN = A0;

void setup() {
  Serial.begin(115200);
  analogReadResolution(10);  // ได้ค่า 0-1023 (UNO R4 รองรับสูงสุด 14 บิต)
}

void loop() {
  int raw = analogRead(SENSOR_PIN);
  Serial.print("light:");  // รูปแบบ "ชื่อ:ค่า" ทำให้ Serial Plotter วาดกราฟได้ทันที
  Serial.println(raw);
  delay(200);
}
