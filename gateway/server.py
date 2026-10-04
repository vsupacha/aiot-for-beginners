"""Gateway ของโคมไฟอัจฉริยะ: รับค่าจากบอร์ด สั่งหลอดไฟ และรับคำสั่งจากหน้าแชต

  POST /v1/decisions  บอร์ดส่งค่าแสง -> ได้คำสั่ง LED_ON/LED_OFF กลับ
  POST /v1/control    หน้าแชตเลือกโหมด auto/on/off (ปุ่ม)
  POST /v1/chat       ข้อความของผู้ใช้ -> แปลเป็นคำสั่ง -> ทำงานและตอบกลับ
  GET  /v1/devices    สถานะล่าสุดของทุกบอร์ด
"""

import argparse
import json
import os
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

from . import chat
from .lamp import LampPolicy
from .llm import compose_messages, translator_from_env
from .network import DiscoveryResponder, lan_ipv4_addresses
from .protocol import PayloadError, validate_chat, validate_control, validate_request
from .state import DeviceState

MAX_BYTES = 2048


class LampServer(ThreadingHTTPServer):
    daemon_threads = True
    request_queue_size = 64

    def __init__(self, address, translator, policy=None):
        super().__init__(address, LampHandler)
        self.translator = translator
        self.devices = DeviceState(policy or LampPolicy())


class LampHandler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *args):
        pass  # ไม่แสดงทุกคำขอ เพราะบอร์ดส่งทุก 3 วินาที

    def _json(self, status, payload):
        body = json.dumps(payload, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(body)
        self.close_connection = True

    def do_GET(self):
        if self.path == "/health":
            self._json(200, {"status": "ok", "schema_version": 1})
        elif self.path == "/v1/devices":
            self._json(200, {"devices": self.server.devices.list()})
        else:
            self._json(404, {"error": "not_found"})

    def do_POST(self):
        if self.path not in ("/v1/decisions", "/v1/chat", "/v1/control"):
            self._json(404, {"error": "not_found"})
            return
        try:
            length = int(self.headers.get("Content-Length", "-1"))
        except ValueError:
            length = -1
        if length < 0:
            self._json(400, {"error": "invalid_payload", "detail": "Content-Length required"})
            return
        if length > MAX_BYTES:
            self._json(413, {"error": "payload_too_large"})
            return
        if not self.headers.get("Content-Type", "").lower().startswith("application/json"):
            self._json(400, {"error": "invalid_payload",
                             "detail": "Content-Type must be application/json"})
            return
        try:
            payload = json.loads(self.rfile.read(length).decode("utf-8"))
            if self.path == "/v1/decisions":
                payload = validate_request(payload)
            elif self.path == "/v1/chat":
                payload = validate_chat(payload)
            else:
                payload = validate_control(payload)
        except (UnicodeDecodeError, json.JSONDecodeError, PayloadError) as exc:
            self._json(400, {"error": "invalid_payload", "detail": str(exc)})
            return
        try:
            status, result = getattr(self, "_" + self.path.split("/")[-1])(payload)
        except Exception:
            self._json(503, {"error": "backend_unavailable"})
            return
        self._json(status, result)

    def _decisions(self, request):
        return 200, self.server.devices.update(request)

    def _control(self, payload):
        device_id, mode = payload
        if not self.server.devices.set_mode(device_id, mode):
            return 404, {"error": "unknown_device"}
        return 200, {"device_id": device_id, "mode": mode}

    def _chat(self, payload):
        device_id, message, prompt = payload
        devices = self.server.devices
        device = devices.get(device_id)
        sent = compose_messages(message, prompt)  # แสดงให้ผู้ใช้เห็นว่าประกอบข้อความอย่างไร
        if device is None:
            return 200, {"device_id": device_id, "reply": chat.no_device_reply(device_id),
                         "command": {"intent": "none"}, "source": "rule", "engine": None,
                         "sent": sent}
        command, source, engine = self.server.translator.translate(message, prompt)
        if command["intent"] == "set_mode":
            devices.set_mode(device_id, command["mode"])
            reply = chat.set_mode_reply(command["mode"])
        elif command["intent"] == "status":
            reply = chat.status_reply(device)
        else:
            reply = chat.unknown_reply()
        return 200, {"device_id": device_id, "reply": reply, "command": command,
                     "source": source, "engine": engine, "sent": sent}


def main():
    parser = argparse.ArgumentParser(description="Smart lamp gateway")
    parser.add_argument("--lan", action="store_true",
                        help="ให้เครื่องอื่นใน Wi-Fi เดียวกันเชื่อมต่อได้ และเปิดการค้นหา gateway อัตโนมัติ")
    parser.add_argument("--host", default=os.getenv("AIOT_HOST"),
                        help="ที่อยู่ที่ใช้ฟัง (ค่าเริ่มต้น 127.0.0.1 หรือ 0.0.0.0 เมื่อใช้ --lan)")
    parser.add_argument("--port", type=int, default=int(os.getenv("AIOT_PORT", "8000")))
    parser.add_argument("--on-below", type=int, default=int(os.getenv("AIOT_ON_BELOW", "35")),
                        help="ความสว่าง (%%) ต่ำกว่านี้ -> เปิดไฟ")
    parser.add_argument("--off-above", type=int, default=int(os.getenv("AIOT_OFF_ABOVE", "35")),
                        help="ความสว่าง (%%) ตั้งแต่นี้ขึ้นไป -> ปิดไฟ (ตั้งให้มากกว่า --on-below เพื่อทำ hysteresis)")
    args = parser.parse_args()
    host = args.host or ("0.0.0.0" if args.lan else "127.0.0.1")
    try:
        policy = LampPolicy(args.on_below, args.off_above)
        translator = translator_from_env()
    except (ValueError, ImportError) as exc:
        parser.error(str(exc))
    server = LampServer((host, args.port), translator, policy)
    port = server.server_port
    print(f"Gateway ready on {host}:{port} (command translator: {translator.name})", flush=True)
    print(f"  Lamp rule: ON below {policy.on_below}%, OFF from {policy.off_above}%", flush=True)
    engine = getattr(translator, "engine", None)
    if engine is not None:
        state = "ready" if engine.ready() else "NOT reachable yet - chat uses keyword rules"
        print(f"  Gemma: {engine.describe()} {state}", flush=True)

    discovery = None
    if not host.startswith("127."):
        try:
            discovery = DiscoveryResponder(host, port).start()
            print(f"  Board auto-discovery: UDP {port}", flush=True)
        except OSError as exc:
            print(f"  Board auto-discovery off ({exc}); set GATEWAY_HOST in the sketch", flush=True)
        addresses = lan_ipv4_addresses() if host == "0.0.0.0" else [host]
        if addresses:
            print(f"  >>> Gateway IP: {addresses[0]}   chat page: http://{addresses[0]}:8501", flush=True)
            if len(addresses) > 1:  # มักเป็น adapter ของ VPN, WSL หรือ Hyper-V
                print(f"      other adapters: {', '.join(addresses[1:])}", flush=True)
        else:
            print("  No LAN IPv4 found - check Wi-Fi/Ethernet", flush=True)
    else:
        print("  Local only. Add --lan so boards can connect.", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
        if discovery:
            discovery.close()


if __name__ == "__main__":
    main()
