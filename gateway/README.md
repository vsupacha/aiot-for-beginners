# gateway (ส่วนของผู้สอน)

ตัวกลางของระบบ (Python 3.10+, ใช้เฉพาะ standard library) รับค่าแสงจากบอร์ด ตัดสินใจเปิด-ปิดไฟตามโหมด รับคำสั่งจากหน้าแชต และเก็บสถานะ

```powershell
python -m gateway.server                  # เฉพาะเครื่องนี้
python -m gateway.server --lan            # ให้บอร์ดและเครื่องอื่นเชื่อมต่อได้ (เปิด UDP discovery)
python -m gateway.server --on-below 30 --off-above 40   # hysteresis
python -m unittest discover -s gateway/tests -t .         # ชุดทดสอบ
```

| ไฟล์ | หน้าที่ |
| --- | --- |
| `protocol.py` | ตรวจรูปแบบข้อมูลขาเข้า (API v1) |
| `lamp.py` | กฎเปิด-ปิดไฟ + hysteresis |
| `state.py` | สถานะ โหมด และประวัติค่าแสงต่อบอร์ด |
| `commands.py` | allowlist ของคำสั่ง + ตัวแปลงข้อความด้วยกฎ |
| `llm.py` | Gemma (llama.cpp / Colab / transformers) สำหรับแปลข้อความเป็นคำสั่งเท่านั้น |
| `chat.py` | แม่แบบข้อความตอบกลับ |
| `server.py`, `network.py`, `remote.py` | HTTP server, UDP discovery, การตั้งค่า Colab |
| `examples/` | ตัวอย่าง request/response |

สัญญา API และตัวแปรที่ตั้งค่าได้: ดู <https://vsupacha.github.io/aiot-for-beginners/content/instructor_gateway.html> ระบบนี้**ไม่มีการยืนยันตัวตนหรือ TLS** ห้ามเปิดสู่ Internet
