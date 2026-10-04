// Wokwi (Arduino Uno): ขั้นที่ 3 จำลองการคุยกับ gateway ผ่าน Serial
// Wokwi เชื่อมกับ gateway ในห้องเรียนไม่ได้ สเก็ตช์นี้จึงทำหน้าที่ของ gateway ในตัวเอง
// เพื่อให้เห็นข้อมูลที่บอร์ดจริงส่ง (->) และคำตอบที่ได้รับ (<-) ทุก 3 วินาที
// พิมพ์ใน Serial Monitor (ช่องล่าง แล้วกด Enter) แทนการพิมพ์ในหน้าแชต:
//   on = เปิดไฟตลอด, off = ปิดไฟตลอด, auto = ตามความสว่าง, ? = ดูสถานะ

const char DEVICE_ID[] = "desk-01";
const int SENSOR_PIN = A0;
const int LED_PIN = LED_BUILTIN;
const int ON_BELOW = 35;    // ความสว่าง (%) ต่ำกว่านี้ -> เปิด  (เหมือน gateway ค่าเริ่มต้น)
const int OFF_ABOVE = 35;   // ความสว่าง (%) ตั้งแต่นี้ขึ้นไป -> ปิด (ตั้ง 30/40 เพื่อลองทำ hysteresis)
const unsigned long SEND_INTERVAL_MS = 3000;

enum Mode { AUTO, FORCE_ON, FORCE_OFF };
Mode mode = AUTO;
bool lampOn = false;
uint32_t seq = 0;
unsigned long lastSend = 0;

int percentOf(int raw) { return (raw * 100L + 511) / 1023; }

const char *modeName() { return mode == AUTO ? "auto" : (mode == FORCE_ON ? "on" : "off"); }

// กฎเดียวกับ gateway/lamp.py (LampPolicy.decide)
bool decide(int pct) {
  if (mode == FORCE_ON) return true;
  if (mode == FORCE_OFF) return false;
  if (pct < ON_BELOW) return true;
  if (pct >= OFF_ABOVE) return false;
  return lampOn;  // ช่วงกลางของ hysteresis: คงสถานะเดิม
}

void readCommands() {
  if (Serial.available() == 0) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  line.toLowerCase();
  if (line == "on") mode = FORCE_ON;
  else if (line == "off") mode = FORCE_OFF;
  else if (line == "auto") mode = AUTO;
  else if (line != "?") {
    if (line.length() > 0) Serial.println("commands: on / off / auto / ?");
    return;
  }
  Serial.print("mode=");
  Serial.println(modeName());
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  Serial.println("Smart lamp (Wokwi). Type: on / off / auto / ?");
}

void loop() {
  readCommands();
  if (millis() - lastSend < SEND_INTERVAL_MS) return;
  lastSend = millis();

  int raw = analogRead(SENSOR_PIN);
  int pct = percentOf(raw);

  // สิ่งที่บอร์ดจริงส่งไปที่ gateway
  Serial.print("-> {\"schema_version\":1,\"device_id\":\"");
  Serial.print(DEVICE_ID);
  Serial.print("\",\"seq\":");
  Serial.print(seq);
  Serial.print(",\"light_raw\":");
  Serial.print(raw);
  Serial.println("}");

  // สิ่งที่ gateway ตัดสินใจและตอบกลับ
  lampOn = decide(pct);
  Serial.print("<- {\"schema_version\":1,\"device_id\":\"");
  Serial.print(DEVICE_ID);
  Serial.print("\",\"seq\":");
  Serial.print(seq);
  Serial.print(",\"action\":\"");
  Serial.print(lampOn ? "LED_ON" : "LED_OFF");
  Serial.print("\",\"mode\":\"");
  Serial.print(modeName());
  Serial.print("\",\"light_pct\":");
  Serial.print(pct);
  Serial.println("}");

  digitalWrite(LED_PIN, lampOn ? HIGH : LOW);
  seq++;
}
