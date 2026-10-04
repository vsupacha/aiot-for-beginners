# เมื่อเปลี่ยนเป็น UNO Q

คง `api/README.md`, `gateway/`, `simulator/`, `jupyter/`, `chat_app/` และใบงานเดิม ส่วนที่ต้องเขียนใหม่คือโปรแกรมบอร์ด เพราะ UNO Q ใช้ Linux MPU + STM32 MCU และสื่อสารกันผ่าน Bridge; sketch `SmartStudyLight.ino` ที่ใช้ `WiFiS3` สำหรับ UNO R4 WiFi **นำไป compile บน UNO Q ตรง ๆ ไม่ได้**

แนวทางย้าย:

1. ให้ STM32 MCU อ่าน potentiometer ที่ A0 แบบ 10-bit และควบคุม LED ที่เข้าถึงได้บน UNO Q; ใช้ระดับ 3.3 V สำหรับวงจรเซ็นเซอร์
2. ให้ Linux MPU ส่ง `POST /v1/decisions` ด้วย payload เดิม และส่ง `action` กลับ MCU ผ่าน Bridge
3. ตรวจ `schema_version`, `device_id`, `seq`, allowlist และ timeout ทั้งสองฝั่ง ฝั่ง Linux ใช้การค้นหา UDP แบบเดียวกัน (`AIOT_DISCOVER 1` → `AIOT_GATEWAY 1 <port>`, ดู `api/README.md`) หรือกำหนด IP ของ gateway เอง
4. ทำ compile/upload และทดสอบ Bridge บนบอร์ด UNO Q รุ่นจริงก่อนแจกนักเรียน

อ้างอิง: [UNO Q user manual](https://docs.arduino.cc/tutorials/uno-q/user-manual/)
