"""ข้อความตอบกลับของหน้าแชต สร้างจากข้อมูลจริงของอุปกรณ์ด้วยแม่แบบ (ไม่ใช้ AI เขียนคำตอบ)"""

MODE_TEXT = {"auto": "อัตโนมัติ (ตามความสว่าง)", "on": "เปิดตลอด (สั่งด้วยมือ)", "off": "ปิดตลอด (สั่งด้วยมือ)"}
HELP = "ลองพิมพ์: เปิดไฟ / ปิดไฟ / อัตโนมัติ / สถานะ"


def status_reply(device):
    lamp = "เปิด" if device["action"] == "LED_ON" else "ปิด"
    return (f"บอร์ด {device['device_id']}: ความสว่าง {device['light_pct']}% "
            f"(A0={device['light_raw']}/1023), ไฟ{lamp}, โหมด {MODE_TEXT[device['mode']]}. "
            f"เหตุผล: {device['reason']}")


def set_mode_reply(mode):
    return (f"รับคำสั่งแล้ว: โหมด {MODE_TEXT[mode]} "
            "บอร์ดจะเปลี่ยนไฟในรอบถัดไป (ไม่เกินประมาณ 3 วินาที)")


def unknown_reply():
    return "ยังไม่เข้าใจคำสั่ง " + HELP


def no_device_reply(device_id):
    return (f"ยังไม่พบข้อมูลจากบอร์ด {device_id} ตรวจสาย USB, Wi-Fi, Serial Monitor "
            "และตรวจว่า gateway กำลังทำงาน")
