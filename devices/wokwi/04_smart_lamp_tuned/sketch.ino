// Wokwi (Arduino Uno): ขั้นที่ 4 ฉบับปรับแก้ (ไม่มี Wi-Fi)
//   1) Smoothing: เฉลี่ยค่า 16 ครั้งล่าสุด     2) Calibration: ปรับช่วง มืดสุด-สว่างสุด
//   3) Hysteresis: เปิดเมื่อต่ำกว่า 30% ปิดเมื่อสูงกว่า 40%
//   4) millis() แทน delay()                    5) PWM fade: หลอดไฟค่อย ๆ สว่าง/หรี่
// วงจร: potentiometer -> A0, LED + ตัวต้านทาน 220 ohm -> ขา D5 (PWM)
// ต่างจากสเก็ตช์จริง: Uno อ่าน ADC ได้ 10 บิต (0-1023) ไม่มี 14 บิต และไม่มีส่วน Wi-Fi

const int SENSOR_PIN = A0;
const int LED_PIN = 5;

// --- ค่าที่ควรปรับให้เข้ากับเซนเซอร์ของคุณ ---
const long SENSOR_DARK = 0;       // ค่าเมื่อมืดสุด
const long SENSOR_BRIGHT = 1023;  // ค่าเมื่อสว่างสุด
const int ON_BELOW_PCT = 30;
const int OFF_ABOVE_PCT = 40;
const unsigned long SAMPLE_INTERVAL_MS = 100;
const unsigned long REPORT_INTERVAL_MS = 1000;
const int FADE_STEP = 5;
const unsigned long FADE_INTERVAL_MS = 5;
const int WINDOW = 16;

long samples[WINDOW];
int sampleCount = 0;
int sampleIndex = 0;
long sampleSum = 0;
unsigned long lastSample = 0;
unsigned long lastReport = 0;
int fadeLevel = 0;
int fadeTarget = 0;
unsigned long lastFade = 0;
bool lampOn = false;

void addSample(long value) {
  if (sampleCount == WINDOW) sampleSum -= samples[sampleIndex];
  else sampleCount++;
  samples[sampleIndex] = value;
  sampleSum += value;
  sampleIndex = (sampleIndex + 1) % WINDOW;
}

int lightRaw10() {
  if (sampleCount == 0) return 0;
  long average = sampleSum / sampleCount;
  long scaled = (average - SENSOR_DARK) * 1023L / (SENSOR_BRIGHT - SENSOR_DARK);
  return (int)constrain(scaled, 0L, 1023L);
}

int percentOf(int raw10) { return (raw10 * 100L + 511) / 1023; }

void updateSampling() {
  if (millis() - lastSample < SAMPLE_INTERVAL_MS) return;
  lastSample = millis();
  addSample(analogRead(SENSOR_PIN));
}

void updateLamp() {
  int pct = percentOf(lightRaw10());
  if (pct < ON_BELOW_PCT) lampOn = true;
  else if (pct >= OFF_ABOVE_PCT) lampOn = false;
  fadeTarget = lampOn ? 255 : 0;
}

void updateFade() {
  if (millis() - lastFade < FADE_INTERVAL_MS) return;
  lastFade = millis();
  if (fadeLevel < fadeTarget) fadeLevel = min(fadeLevel + FADE_STEP, fadeTarget);
  else if (fadeLevel > fadeTarget) fadeLevel = max(fadeLevel - FADE_STEP, fadeTarget);
  analogWrite(LED_PIN, fadeLevel);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  for (int i = 0; i < WINDOW; i++) addSample(analogRead(SENSOR_PIN));
  Serial.println("Smart lamp tuned (Wokwi)");
}

void loop() {
  updateSampling();
  updateLamp();
  updateFade();
  if (millis() - lastReport >= REPORT_INTERVAL_MS) {
    lastReport = millis();
    Serial.print("avg=");
    Serial.print(lightRaw10());
    Serial.print(" (");
    Serial.print(percentOf(lightRaw10()));
    Serial.print("%) lamp=");
    Serial.print(lampOn ? "ON" : "OFF");
    Serial.print(" pwm=");
    Serial.println(fadeLevel);
  }
}
