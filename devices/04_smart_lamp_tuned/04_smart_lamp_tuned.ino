// ขั้นที่ 4: Smart lamp ฉบับปรับแก้ (ต่อยอดจาก 03_smart_lamp)
// ส่วนเครือข่ายเหมือนขั้นที่ 3 ทุกประการ สิ่งที่เปลี่ยนคือการอ่านเซนเซอร์และการขับหลอดไฟ:
//   1) analogReadResolution(14): อ่านละเอียดขึ้น แล้วส่ง gateway เป็น 10 บิตตามสัญญา API เดิม
//   2) Smoothing: เฉลี่ยค่า 16 ครั้งล่าสุด ลดสัญญาณรบกวนที่ทำให้ไฟกะพริบ
//   3) Calibration: ปรับช่วง "มืดสุด-สว่างสุด" ของเซนเซอร์จริงให้เป็น 0-1023
//   4) Hysteresis ในโหมด offline: เปิดเมื่อต่ำกว่า 30% ปิดเมื่อสูงกว่า 40%
//   5) millis() แทน delay() ในส่วนอ่านเซนเซอร์และปรับความสว่าง ไม่หยุดรอ
//   6) PWM: หลอดไฟค่อย ๆ สว่างขึ้น/มืดลง (fade) แทนการกระโดดทันที
// วงจรเพิ่ม: LED ภายนอก -> ตัวต้านทาน 220 ohm -> ขา D5 (PWM) -> GND (ขายาวของ LED ต่อฝั่ง D5)
// หมายเหตุ: ระหว่างติดต่อ gateway (สูงสุดราว 5 วินาทีเมื่อเครือข่ายมีปัญหา) การ fade จะหยุดชั่วคราว
#include <WiFiS3.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include "arduino_secrets.h"

const char GATEWAY_HOST[] = "";
const uint16_t GATEWAY_PORT = 8000;
const char DEVICE_ID[] = "desk-01";
const int SENSOR_PIN = A0;
const int LED_PIN = 5;                     // ขา PWM (D3, D5, D6, D9, D10, D11)
const unsigned long SEND_INTERVAL_MS = 3000;
const unsigned long RESPONSE_TIMEOUT_MS = 5000;
const int FAILURES_BEFORE_REDISCOVERY = 3;
const int FAILURES_BEFORE_WIFI_RESET = 6;

// --- ค่าที่ควรปรับให้เข้ากับเซนเซอร์ของคุณ ---
const int ADC_BITS = 14;
const long ADC_MAX = (1L << ADC_BITS) - 1;  // 16383
// ค่า 14 บิตเมื่อเซนเซอร์ "มืดสุด" และ "สว่างสุด" ในห้องที่ใช้งานจริง (วัดจาก Serial Monitor)
// ถ้าเซนเซอร์เป็นแบบกลับด้าน (มืด = ค่ามาก) ให้สลับสองค่านี้
const long SENSOR_DARK = 0;
const long SENSOR_BRIGHT = ADC_MAX;
const int OFFLINE_ON_BELOW_PCT = 30;       // hysteresis ของโหมด offline
const int OFFLINE_OFF_ABOVE_PCT = 40;
const unsigned long SAMPLE_INTERVAL_MS = 100;
const int FADE_STEP = 5;                   // ความสว่าง PWM (0-255) ที่เปลี่ยนต่อ 5 ms
const unsigned long FADE_INTERVAL_MS = 5;
const int WINDOW = 16;

long samples[WINDOW];
int sampleCount = 0;
int sampleIndex = 0;
long sampleSum = 0;
unsigned long lastSample = 0;
int fadeLevel = 0;         // ความสว่างปัจจุบันของหลอด (0-255)
int fadeTarget = 0;        // ความสว่างเป้าหมาย
unsigned long lastFade = 0;
bool offlineLampOn = false;

uint32_t seq = 0;
unsigned long lastSend = 0;
IPAddress gatewayIP;
uint16_t gatewayPort = GATEWAY_PORT;
bool haveGateway = false;
int failures = 0;
int failuresInRow = 0;
WiFiUDP udp;

// เก็บตัวอย่างล่าสุด WINDOW ค่า และคืนค่าเฉลี่ย (ใช้ผลรวมที่คงไว้ ไม่ต้องบวกใหม่ทุกครั้ง)
void addSample(long value) {
  if (sampleCount == WINDOW) sampleSum -= samples[sampleIndex];
  else sampleCount++;
  samples[sampleIndex] = value;
  sampleSum += value;
  sampleIndex = (sampleIndex + 1) % WINDOW;
}

// ค่าเฉลี่ยที่ผ่านการ calibrate แล้ว ในช่วง 0-1023 (ตรงกับ light_raw ของ API)
int lightRaw10() {
  if (sampleCount == 0) return 0;
  long average = sampleSum / sampleCount;
  long scaled = (average - SENSOR_DARK) * 1023L / (SENSOR_BRIGHT - SENSOR_DARK);
  return (int)constrain(scaled, 0L, 1023L);
}

int percentOf(int raw10) { return (raw10 * 100 + 511) / 1023; }

void updateSampling() {
  if (millis() - lastSample < SAMPLE_INTERVAL_MS) return;
  lastSample = millis();
  addSample(analogRead(SENSOR_PIN));
}

void setLamp(bool on) { fadeTarget = on ? 255 : 0; }

void updateFade() {
  if (millis() - lastFade < FADE_INTERVAL_MS) return;
  lastFade = millis();
  if (fadeLevel < fadeTarget) fadeLevel = min(fadeLevel + FADE_STEP, fadeTarget);
  else if (fadeLevel > fadeTarget) fadeLevel = max(fadeLevel - FADE_STEP, fadeTarget);
  analogWrite(LED_PIN, fadeLevel);
}

// กฎ offline แบบ hysteresis: ช่วง 30-40% คงสถานะเดิม
bool offlineDecision(int raw10) {
  int pct = percentOf(raw10);
  if (pct < OFFLINE_ON_BELOW_PCT) offlineLampOn = true;
  else if (pct >= OFFLINE_OFF_ABOVE_PCT) offlineLampOn = false;
  return offlineLampOn;
}

bool connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  Serial.print("Connecting Wi-Fi");
  WiFi.begin(SECRET_SSID, SECRET_PASS);
  for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi unavailable");
    return false;
  }
  // WiFiS3 อาจรายงาน 0.0.0.0 จนกว่า DHCP จะเสร็จ
  for (int i = 0; i < 20 && WiFi.localIP() == IPAddress(0, 0, 0, 0); i++) delay(250);
  Serial.print("Board IP: ");
  Serial.println(WiFi.localIP());
  return true;
}

bool discoverGateway() {
  if (GATEWAY_HOST[0] != '\0') {
    haveGateway = gatewayIP.fromString(GATEWAY_HOST);
    if (!haveGateway) Serial.println("GATEWAY_HOST is not a valid IPv4 address");
    return haveGateway;
  }
  Serial.println("Searching for gateway...");
  IPAddress local = WiFi.localIP();
  IPAddress mask = WiFi.subnetMask();
  IPAddress subnetBroadcast(local[0] | ~mask[0], local[1] | ~mask[1],
                            local[2] | ~mask[2], local[3] | ~mask[3]);
  udp.begin(GATEWAY_PORT + 1);
  for (int attempt = 0; attempt < 3 && !haveGateway; attempt++) {
    IPAddress targets[] = {subnetBroadcast, IPAddress(255, 255, 255, 255)};
    for (IPAddress target : targets) {
      udp.beginPacket(target, GATEWAY_PORT);
      udp.print("AIOT_DISCOVER 1");
      udp.endPacket();
    }
    unsigned long started = millis();
    while (millis() - started < 1000) {
      if (udp.parsePacket() > 0) {
        char reply[32] = {0};
        udp.read(reply, sizeof(reply) - 1);
        if (strncmp(reply, "AIOT_GATEWAY 1 ", 15) == 0) {
          gatewayIP = udp.remoteIP();
          gatewayPort = atoi(reply + 15);
          haveGateway = gatewayPort > 0;
          break;
        }
      }
      delay(10);
    }
  }
  udp.stop();
  if (haveGateway) {
    Serial.print("Gateway found: ");
    Serial.print(gatewayIP);
    Serial.print(':');
    Serial.println(gatewayPort);
  } else {
    Serial.println("Gateway not found; run it with --lan, or set GATEWAY_HOST");
  }
  return haveGateway;
}

// ส่งค่าแสงให้ gateway แล้วอ่านคำสั่งที่ตอบกลับ คืน true เมื่อได้คำตอบที่ถูกต้องครบ
bool requestDecision(int lightRaw, uint32_t requestSeq, bool &turnOn) {
  JsonDocument request;
  request["schema_version"] = 1;
  request["device_id"] = DEVICE_ID;
  request["seq"] = requestSeq;
  request["light_raw"] = lightRaw;
  String body;
  serializeJson(request, body);

  WiFiClient client;
  client.setTimeout(RESPONSE_TIMEOUT_MS);
  if (!client.connect(gatewayIP, gatewayPort)) {
    client.stop();  // คืน socket ของโมดูล Wi-Fi ไม่เช่นนั้นล้มเหลวซ้ำ ๆ แล้ว socket หมด
    Serial.println("Gateway connection failed");
    return false;
  }
  client.print("POST /v1/decisions HTTP/1.1\r\nHost: ");
  client.print(gatewayIP);
  client.print("\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: ");
  client.print(body.length());
  client.print("\r\n\r\n");
  client.print(body);

  String statusLine = client.readStringUntil('\n');
  statusLine.trim();
  if (!statusLine.startsWith("HTTP/1.1 200 ") && !statusLine.startsWith("HTTP/1.0 200 ")) {
    Serial.print("Bad HTTP status: ");
    Serial.println(statusLine);
    client.stop();
    return false;
  }
  int contentLength = -1;
  while (true) {
    String header = client.readStringUntil('\n');
    header.trim();
    if (header.length() == 0) break;
    if (header.startsWith("Content-Length:")) contentLength = header.substring(15).toInt();
  }
  if (contentLength <= 0 || contentLength >= 1024) {
    Serial.println("Bad response length");
    client.stop();
    return false;
  }
  char response[1024];
  size_t received = client.readBytes(response, contentLength);
  client.stop();
  if (received != (size_t)contentLength) {
    Serial.println("Incomplete response");
    return false;
  }
  response[received] = '\0';
  JsonDocument reply;
  if (deserializeJson(reply, response) != DeserializationError::Ok) {
    Serial.println("Invalid response JSON");
    return false;
  }
  // ตรวจคำตอบก่อนเชื่อ: ต้องเป็นของบอร์ดนี้ และตรงกับคำขอรอบนี้
  const char *replyDevice = reply["device_id"] | "";
  const char *action = reply["action"] | "";
  if (reply["schema_version"].as<int>() != 1 ||
      strcmp(replyDevice, DEVICE_ID) != 0 ||
      reply["seq"].isNull() ||
      reply["seq"].as<uint32_t>() != requestSeq ||
      (strcmp(action, "LED_ON") != 0 && strcmp(action, "LED_OFF") != 0)) {
    Serial.println("Response contract mismatch");
    return false;
  }
  turnOn = strcmp(action, "LED_ON") == 0;
  Serial.print("HTTP 200 seq="); Serial.print(requestSeq);
  Serial.print(" light="); Serial.print(lightRaw);
  Serial.print(" mode="); Serial.print(reply["mode"].as<const char *>());
  Serial.print(" action="); Serial.println(action);
  return true;
}

void printStatus() {
  Serial.print("device="); Serial.print(DEVICE_ID);
  Serial.print(" gateway=");
  if (haveGateway) Serial.print(gatewayIP); else Serial.print("(not found)");
  Serial.print(" light="); Serial.print(lightRaw10());
  Serial.print(" ("); Serial.print(percentOf(lightRaw10())); Serial.println("%)");
}

void readSerialCommands() {
  while (Serial.available() > 0) {
    if (Serial.read() == '?') printStatus();
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  analogWrite(LED_PIN, 0);
  analogReadResolution(ADC_BITS);
  Serial.begin(115200);
  delay(1000);
  Serial.println("Smart lamp (tuned). Type ? for status");
  for (int i = 0; i < WINDOW; i++) addSample(analogRead(SENSOR_PIN));  // เติมหน้าต่างเฉลี่ยก่อนส่งครั้งแรก
  if (connectWiFi()) discoverGateway();
}

void loop() {
  readSerialCommands();
  updateSampling();
  updateFade();
  if (millis() - lastSend < SEND_INTERVAL_MS) return;
  lastSend = millis();

  int light = lightRaw10();
  bool turnOn = false;
  bool ok = connectWiFi() && (haveGateway || discoverGateway()) &&
            requestDecision(light, seq, turnOn);
  if (!ok) {
    turnOn = offlineDecision(light);
    Serial.print("Offline rule: light="); Serial.print(light);
    Serial.println(turnOn ? " -> LED_ON" : " -> LED_OFF");
  }
  setLamp(turnOn);

  if (ok) {
    failures = 0;
    failuresInRow = 0;
  } else {
    if (++failures >= FAILURES_BEFORE_REDISCOVERY && GATEWAY_HOST[0] == '\0') {
      haveGateway = false;
      failures = 0;
    }
    if (++failuresInRow >= FAILURES_BEFORE_WIFI_RESET) {
      Serial.println("Still failing; reconnecting Wi-Fi");
      WiFi.disconnect();
      failuresInRow = 0;
    }
  }
  if (seq >= 2147483647UL) seq = 0;
  else seq++;
}
