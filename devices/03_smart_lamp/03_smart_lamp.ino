// ขั้นที่ 3: Smart lamp บน UNO R4 WiFi (WiFiS3 + ArduinoJson v7)
//   A0   : เซนเซอร์แสงแบบ analog (หรือ potentiometer จำลองแสง) ค่ามาก = สว่าง
//   LED  : LED บนบอร์ด (LED_BUILTIN) แสดงสถานะหลอดไฟ
// ทุก 3 วินาทีบอร์ดส่งค่าแสงให้ gateway และทำตามคำสั่ง LED_ON / LED_OFF ที่ตอบกลับมา
// gateway จะรู้โหมดที่ผู้ใช้เลือกจากหน้าแชต (auto/on/off) จึงไม่ต้องมีโค้ดรับคำสั่งบนบอร์ด
// ถ้าติดต่อ gateway ไม่ได้ บอร์ดใช้เกณฑ์แสงของตัวเอง (โหมด offline) หลอดไฟจึงยังใช้งานได้
// Serial Monitor (115200): พิมพ์ ? เพื่อดูสถานะ
#include <WiFiS3.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include "arduino_secrets.h"

// เว้นว่างไว้เพื่อให้ค้นหา gateway เอง (UDP broadcast ใน Wi-Fi เดียวกัน)
// ถ้าเครือข่ายบล็อก broadcast ให้ใส่ IP ที่ gateway แสดง เช่น "192.168.1.20"
const char GATEWAY_HOST[] = "";
const uint16_t GATEWAY_PORT = 8000;
const char DEVICE_ID[] = "desk-01";        // ทุกบอร์ดต้องไม่ซ้ำกัน ห้ามใช้ชื่อจริง
const int SENSOR_PIN = A0;
const int LED_PIN = LED_BUILTIN;
const int OFFLINE_ON_BELOW = 358;          // ประมาณ 35% ของ 1023 ใช้เมื่อไม่มี gateway
const unsigned long SEND_INTERVAL_MS = 3000;
const unsigned long RESPONSE_TIMEOUT_MS = 5000;
const int FAILURES_BEFORE_REDISCOVERY = 3;
const int FAILURES_BEFORE_WIFI_RESET = 6;

uint32_t seq = 0;
unsigned long lastSend = 0;
IPAddress gatewayIP;
uint16_t gatewayPort = GATEWAY_PORT;
bool haveGateway = false;
int failures = 0;
int failuresInRow = 0;
WiFiUDP udp;

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
  if (haveGateway) Serial.println(gatewayIP); else Serial.println("(not found)");
}

void readSerialCommands() {
  while (Serial.available() > 0) {
    if (Serial.read() == '?') printStatus();
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  analogReadResolution(10);
  Serial.begin(115200);
  delay(1000);
  Serial.println("Smart lamp. Type ? for status");
  if (connectWiFi()) discoverGateway();
}

void loop() {
  readSerialCommands();
  if (millis() - lastSend < SEND_INTERVAL_MS) return;
  lastSend = millis();

  int lightRaw = analogRead(SENSOR_PIN);
  bool turnOn = false;
  bool ok = connectWiFi() && (haveGateway || discoverGateway()) &&
            requestDecision(lightRaw, seq, turnOn);
  if (!ok) {
    turnOn = lightRaw < OFFLINE_ON_BELOW;  // โหมด offline: ตัดสินใจเองจากค่าแสง
    Serial.print("Offline rule: light="); Serial.print(lightRaw);
    Serial.println(turnOn ? " -> LED_ON" : " -> LED_OFF");
  }
  digitalWrite(LED_PIN, turnOn ? HIGH : LOW);

  if (ok) {
    failures = 0;
    failuresInRow = 0;
  } else {
    if (++failures >= FAILURES_BEFORE_REDISCOVERY && GATEWAY_HOST[0] == '\0') {
      haveGateway = false;  // gateway อาจเปิดใหม่และได้ IP ใหม่
      failures = 0;
    }
    if (++failuresInRow >= FAILURES_BEFORE_WIFI_RESET) {
      Serial.println("Still failing; reconnecting Wi-Fi");
      WiFi.disconnect();  // connectWiFi() เริ่มเชื่อมต่อใหม่ในรอบถัดไป
      failuresInRow = 0;
    }
  }
  if (seq >= 2147483647UL) seq = 0;
  else seq++;
}
