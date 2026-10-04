# devices: สเก็ตช์สำหรับ Arduino UNO R4 WiFi

โฟลเดอร์ `01_`–`04_` สำหรับนักเรียน ส่วน `simulator/` สำหรับผู้สอน

| โฟลเดอร์ | ทำอะไร | ต้องใช้ |
| --- | --- | --- |
| `01_read_light/` | อ่านค่า analog ที่ A0 แสดงใน Serial Monitor/Plotter | — |
| `02_local_nightlight/` | ไฟกลางคืนที่ทำงานบนบอร์ดอย่างเดียว | — |
| `03_smart_lamp/` | ส่งค่าแสงให้ gateway และทำตามคำสั่ง (auto/on/off จากหน้าแชต) | Wi-Fi, ArduinoJson v7 |
| `04_smart_lamp_tuned/` | ฉบับปรับแก้: 14 บิต, moving average, calibration, hysteresis offline, PWM fade, `millis()` | LED + 220 Ω ที่ D5 |
| `wokwi/` | **ไม่มีบอร์ด**: สเก็ตช์ + `diagram.json` สำหรับ Arduino Uno ใน [Wokwi](https://wokwi.com) (ขั้น 3 จำลอง gateway ด้วย Serial) | เบราว์เซอร์ |
| `simulator/` | จำลองบอร์ดเพื่อทดสอบ gateway/แชตโดยไม่ต้องมีฮาร์ดแวร์ | Python |
| `uno_q/` | แนวทางย้ายไป Arduino UNO Q (ยังไม่ได้ทดสอบ) | — |

`03_` และ `04_` ต้องมีไฟล์ `arduino_secrets.h` (คัดลอกจาก `arduino_secrets.h.example` แล้วใส่รหัส Wi-Fi 2.4 GHz) **ห้ามขึ้น GitHub**

เอกสารประกอบ: [สร้างโคมไฟ](https://vsupacha.github.io/aiot-for-beginners/content/02_build_lamp.html) และ [ปรับแก้โค้ด](https://vsupacha.github.io/aiot-for-beginners/content/04_tuning.html)

> สเก็ตช์ทั้งหมดเขียนสำหรับ UNO R4 WiFi (WiFiS3) แต่ยังไม่ได้ compile/ทดสอบบนบอร์ดจริงใน repo นี้ ให้อัปโหลดและตรวจ Serial Monitor ตามเอกสารก่อนใช้สอน ส่วน gateway/simulator มีชุดทดสอบอัตโนมัติ
