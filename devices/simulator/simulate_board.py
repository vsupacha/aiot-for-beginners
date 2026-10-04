"""จำลองบอร์ดโคมไฟเพื่อทดสอบ gateway และหน้าแชตโดยไม่ต้องมีฮาร์ดแวร์

python devices/simulator/simulate_board.py          # ทดสอบอัตโนมัติ 1 ชุด (ต้องเปิด gateway ก่อน)
python devices/simulator/simulate_board.py --demo   # ส่งค่าแสงที่ขึ้น-ลงเรื่อย ๆ ให้ดูในหน้าแชต
"""

import argparse
import json
import math
import sys
import time
from urllib.request import Request, urlopen

DEVICE_ID = "sim-01"
SEND_INTERVAL = 3.0  # เท่ากับ SEND_INTERVAL_MS ในสเก็ตช์


def call(base, path, payload):
    request = Request(base + path, data=json.dumps(payload).encode(),
                      headers={"Content-Type": "application/json"})
    with urlopen(request, timeout=8) as response:
        return json.load(response)


class Board:
    def __init__(self, base, device_id=DEVICE_ID):
        self.base, self.device_id, self.seq = base, device_id, 0

    def send(self, light_raw):
        self.seq += 1
        reply = call(self.base, "/v1/decisions", {"schema_version": 1, "device_id": self.device_id,
                                                  "seq": self.seq, "light_raw": light_raw})
        assert reply["seq"] == self.seq and reply["device_id"] == self.device_id
        return reply

    def say(self, message):
        return call(self.base, "/v1/chat", {"device_id": self.device_id, "message": message})


def selftest(base):
    board = Board(base)
    failures = 0

    def check(name, got, want):
        nonlocal failures
        ok = got == want
        failures += not ok
        print(f"{'OK  ' if ok else 'FAIL'} {name}: {got}" + ("" if ok else f" (ต้องการ {want})"))

    board.send(500)  # ให้ gateway รู้จักบอร์ดนี้ก่อน แล้วรีเซ็ตโหมดเผื่อค้างจากการทดลองก่อนหน้า
    call(base, "/v1/control", {"device_id": DEVICE_ID, "mode": "auto"})
    check("มืด -> เปิดไฟ", board.send(100)["action"], "LED_ON")
    check("สว่าง -> ปิดไฟ", board.send(800)["action"], "LED_OFF")
    reply = board.say("เปิดไฟ")
    check("แชต 'เปิดไฟ' -> คำสั่ง", reply["command"], {"intent": "set_mode", "mode": "on"})
    check("โหมด on ทั้งที่สว่าง", board.send(800)["action"], "LED_ON")
    board.say("ปิดไฟ")
    check("โหมด off ทั้งที่มืด", board.send(100)["action"], "LED_OFF")
    board.say("อัตโนมัติ")
    check("กลับ auto -> มืดเปิดไฟ", board.send(100)["action"], "LED_ON")
    check("ถามสถานะไม่เปลี่ยนโหมด", (board.say("ทำไมไฟติด")["command"], board.send(100)["mode"]),
          ({"intent": "status"}, "auto"))
    print("ผ่านทั้งหมด" if not failures else f"ไม่ผ่าน {failures} รายการ")
    return 1 if failures else 0


def demo(base):
    """ค่าแสงขึ้น-ลงเป็นคลื่น คาบ 60 วินาที เหมือนกลางวัน-กลางคืนแบบเร่ง"""
    board = Board(base)
    print("ส่งข้อมูลทุก 3 วินาที กด Ctrl+C เพื่อหยุด")
    started = time.monotonic()
    try:
        while True:
            phase = (time.monotonic() - started) / 60 * 2 * math.pi
            raw = int(511 + 450 * math.sin(phase))
            reply = board.send(raw)
            print(f"seq={reply['seq']} light={reply['light_pct']}% -> {reply['action']} ({reply['mode']})")
            time.sleep(SEND_INTERVAL)
    except KeyboardInterrupt:
        return 0


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")  # คอนโซล Windows บางเครื่องไม่ใช่ UTF-8
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8000)
    parser.add_argument("--demo", action="store_true", help="ส่งค่าแสงต่อเนื่องแทนการทดสอบ")
    args = parser.parse_args()
    url = f"http://{args.host}:{args.port}"
    try:
        sys.exit(demo(url) if args.demo else selftest(url))
    except OSError as exc:
        sys.exit(f"ติดต่อ gateway ที่ {url} ไม่ได้: {exc}\nเปิดด้วย: python -m gateway.server")
