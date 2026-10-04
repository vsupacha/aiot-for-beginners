"""คำสั่งที่ระบบรับรู้ และตัวแปลงข้อความของผู้ใช้เป็นคำสั่ง (ด้วยกฎ)

คำสั่งมีแค่ 3 แบบ ซึ่งเป็น allowlist ที่ gateway ตรวจเองทุกครั้ง ไม่ว่าคำสั่งจะมาจากกฎหรือ AI:
  {"intent": "set_mode", "mode": "auto" | "on" | "off"}
  {"intent": "status"}
  {"intent": "none"}   (ไม่เข้าใจ หรือไม่ใช่คำสั่ง)
"""

import json

from .protocol import MODES

NONE = {"intent": "none"}
STATUS = {"intent": "status"}

ON_WORDS = ("เปิดไฟ", "ติดไฟ", "เปิดโคม", "เปิดหลอด", "turn on", "switch on", "light on", "lights on")
OFF_WORDS = ("ปิดไฟ", "ดับไฟ", "ดับโคม", "ปิดโคม", "ปิดหลอด", "turn off", "switch off", "light off", "lights off")
AUTO_WORDS = ("อัตโนมัติ", "ออโต้", "ตามแสง", "auto")
STATUS_WORDS = ("สถานะ", "status")
LAMP_WORDS = ("ไฟ", "สว่าง", "แสง", "มืด", "โคม", "หลอด", "lamp", "light", "bright", "dark")
QUESTION_WORDS = ("ตอนนี้", "เท่าไร", "เท่าไหร่", "กี่")
# ประโยคคำถาม/ปฏิเสธ/เงื่อนไข ไม่ใช่คำสั่ง: ให้ระบบอธิบายสถานะแทนการสั่งงาน
NOT_A_COMMAND = ("ทำไม", "อย่า", "ไม่", "ไม่ต้อง", "ไม่ให้", "หรือเปล่า", "ไหม", "มั้ย", "หรือไม่", "ถ้า", "?")


def validate_command(command):
    """ตรวจคำสั่งตาม allowlist (ใช้กับคำตอบของ AI ด้วย); ผิดรูปแบบให้ ValueError"""
    if not isinstance(command, dict):
        raise ValueError("command must be an object")
    intent = command.get("intent")
    if intent == "set_mode" and set(command) == {"intent", "mode"} and command["mode"] in MODES:
        return {"intent": "set_mode", "mode": command["mode"]}
    if intent in ("status", "none") and set(command) == {"intent"}:
        return {"intent": intent}
    raise ValueError("command is not in the allowlist")


def rule_translate(message):
    """แปลข้อความเป็นคำสั่งด้วยกฎคำสำคัญ: ใช้เป็นตัวสำรองเมื่อไม่มี AI หรือ AI ล้มเหลว"""
    text = message.lower().strip()
    if any(word in text for word in STATUS_WORDS):
        return dict(STATUS)
    if any(word in text for word in NOT_A_COMMAND + QUESTION_WORDS):
        # คำถาม/ปฏิเสธ/เงื่อนไข: อธิบายสถานะถ้าเกี่ยวกับโคมไฟ ไม่เช่นนั้นไม่ใช่เรื่องของระบบนี้
        return dict(STATUS) if any(word in text for word in LAMP_WORDS) else dict(NONE)
    # "ปิดไฟ" เป็นส่วนหนึ่งของ "เปิดไฟ" จึงตัดคำเปิดออกก่อนค้นหาคำปิด
    on_found = any(word in text for word in ON_WORDS)
    rest = text
    for word in ON_WORDS:
        rest = rest.replace(word, " ")
    found = set()
    if on_found:
        found.add("on")
    if any(word in rest for word in OFF_WORDS):
        found.add("off")
    if any(word in text for word in AUTO_WORDS):
        found.add("auto")
    if len(found) == 1:  # ถ้าพบหลายแบบพร้อมกัน (เช่น เปิดไฟแล้วปิดไฟ) ถือว่ากำกวม ไม่สั่ง
        return {"intent": "set_mode", "mode": found.pop()}
    return dict(NONE)


def parse_model_answer(text):
    """หา JSON object แรกในคำตอบของโมเดลที่ผ่าน allowlist; ข้อความอื่นถูกทิ้ง"""
    decoder = json.JSONDecoder()
    for index, char in enumerate(text):
        if char != "{":
            continue
        try:
            obj, _ = decoder.raw_decode(text[index:])
            return validate_command(obj)
        except (json.JSONDecodeError, ValueError):
            continue
    raise ValueError("model did not return an allowed command")
