"""สถานะอุปกรณ์ในหน่วยความจำ: ค่าล่าสุด, โหมดที่ผู้ใช้เลือก และประวัติค่าแสงสั้น ๆ"""

from collections import deque
from datetime import datetime, timezone
from threading import Lock

from .protocol import SCHEMA_VERSION, light_percent

HISTORY_LENGTH = 30


class DeviceState:
    def __init__(self, policy):
        self.policy = policy
        self._lock = Lock()
        self._devices = {}

    def update(self, request):
        """บันทึกค่าจากบอร์ด ตัดสินใจ และคืนคำตอบสำหรับบอร์ด"""
        pct = light_percent(request["light_raw"])
        with self._lock:
            item = self._devices.setdefault(request["device_id"], {
                "mode": "auto", "action": None, "history": deque(maxlen=HISTORY_LENGTH)})
            action, reason = self.policy.decide(pct, item["mode"], item["action"])
            item.update(seq=request["seq"], light_raw=request["light_raw"], light_pct=pct,
                        action=action, reason=reason,
                        last_seen_utc=datetime.now(timezone.utc).isoformat())
            item["history"].append({"seq": request["seq"], "light_pct": pct, "action": action})
            mode = item["mode"]
        return {"schema_version": SCHEMA_VERSION, "device_id": request["device_id"],
                "seq": request["seq"], "action": action, "mode": mode,
                "reason": reason[:120], "light_pct": pct}

    def set_mode(self, device_id, mode):
        """คืน False ถ้ายังไม่เคยได้ข้อมูลจากอุปกรณ์นี้"""
        with self._lock:
            if device_id not in self._devices:
                return False
            self._devices[device_id]["mode"] = mode
            return True

    def get(self, device_id):
        with self._lock:
            item = self._devices.get(device_id)
            return self._snapshot(device_id, item) if item else None

    def list(self):
        with self._lock:
            return [self._snapshot(key, self._devices[key]) for key in sorted(self._devices)]

    @staticmethod
    def _snapshot(device_id, item):
        return {"device_id": device_id, **item, "history": list(item["history"])}
