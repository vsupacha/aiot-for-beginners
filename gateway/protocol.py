"""ข้อตกลงข้อมูล (API v1) ระหว่างบอร์ด, gateway และหน้าแชต: ตรวจรูปแบบก่อนใช้งานเสมอ"""

import re

SCHEMA_VERSION = 1
DEVICE_ID = re.compile(r"^[A-Za-z0-9_-]{1,32}$")
MODES = ("auto", "on", "off")
ACTIONS = ("LED_ON", "LED_OFF")
MAX_MESSAGE = 300
MAX_PROMPT = 300


class PayloadError(ValueError):
    pass


def validate_request(payload):
    """คำขอจากบอร์ด: POST /v1/decisions"""
    if not isinstance(payload, dict):
        raise PayloadError("request must be a JSON object")
    if set(payload) != {"schema_version", "device_id", "seq", "light_raw"}:
        raise PayloadError("request fields must match API v1")
    if type(payload["schema_version"]) is not int or payload["schema_version"] != SCHEMA_VERSION:
        raise PayloadError("schema_version must be 1")
    check_device_id(payload["device_id"])
    if type(payload["seq"]) is not int or not 0 <= payload["seq"] <= 2147483647:
        raise PayloadError("seq must be an integer 0-2147483647")
    if type(payload["light_raw"]) is not int or not 0 <= payload["light_raw"] <= 1023:
        raise PayloadError("light_raw must be an integer 0-1023")
    return payload


def check_device_id(value):
    if not isinstance(value, str) or not DEVICE_ID.fullmatch(value):
        raise PayloadError("device_id must be 1-32 safe characters (A-Z a-z 0-9 _ -)")
    return value


def validate_chat(payload):
    """ข้อความจากหน้าแชต: POST /v1/chat -> (device_id, message, prompt)

    prompt คือ system prompt ของผู้ใช้เอง (ไม่บังคับ) ที่ถูกนำไปต่อก่อนข้อความจริงตอนส่งให้ AI
    """
    if (not isinstance(payload, dict) or not {"device_id", "message"} <= set(payload)
            or set(payload) - {"device_id", "message", "prompt"}):
        raise PayloadError("chat takes device_id, message and optional prompt")
    check_device_id(payload["device_id"])
    message = payload["message"]
    if not isinstance(message, str) or not 1 <= len(message.strip()) <= MAX_MESSAGE:
        raise PayloadError(f"message must be 1-{MAX_MESSAGE} characters")
    prompt = payload.get("prompt", "")
    if not isinstance(prompt, str) or len(prompt.strip()) > MAX_PROMPT:
        raise PayloadError(f"prompt must be text of at most {MAX_PROMPT} characters")
    return payload["device_id"], message.strip(), prompt.strip()


def validate_control(payload):
    """ปุ่มควบคุมจากหน้าแชต: POST /v1/control -> (device_id, mode)"""
    if not isinstance(payload, dict) or set(payload) != {"device_id", "mode"}:
        raise PayloadError("control takes exactly device_id and mode")
    check_device_id(payload["device_id"])
    if payload["mode"] not in MODES:
        raise PayloadError("mode must be auto, on or off")
    return payload["device_id"], payload["mode"]


def light_percent(raw):
    """0-1023 -> 0-100 (ปัดครึ่งขึ้นด้วยจำนวนเต็ม ให้ผลเหมือนกันทุกภาษา)"""
    return (raw * 100 + 511) // 1023
