# ai (ส่วนของผู้สอน)

Gemma ในโครงการนี้ทำหน้าที่เดียว คือ **แปลข้อความของผู้ใช้เป็นคำสั่ง JSON ที่อยู่ใน allowlist** (`gateway/commands.py`) ไม่ได้ควบคุมหลอดไฟหรือเขียนคำตอบ ถ้า AI ช้า ล้มเหลว หรือตอบนอก allowlist gateway ใช้กฎคำสำคัญแทน

| ที่ | ใช้ทำอะไร |
| --- | --- |
| `scripts/setup_gemma_cpu.ps1` | ดาวน์โหลด llama.cpp และ Gemma 4 E2B 4-bit (~3.4 GB) ครั้งเดียว |
| `scripts/start_gemma_cpu.ps1` | เปิด Gemma บนพอร์ต 8090 (เปิดค้างไว้) |
| `notebooks/command_translation_demo.ipynb` | เปรียบเทียบกฎกับ Gemma และทดลองคำตอบนอก allowlist |
| `notebooks/colab_llm_server.ipynb` | รัน Gemma บน Colab GPU ให้ gateway เรียกผ่าน ngrok (ยังไม่ได้ทดสอบบน Colab จริง) |
| `requirements-gemma.txt` | เฉพาะการรันด้วย transformers บน GPU |

```powershell
$env:AIOT_TRANSLATOR='gemma'
python -m gateway.server --lan
```

รายละเอียด: <https://vsupacha.github.io/aiot-for-beginners/content/instructor_ai.html>
