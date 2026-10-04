"""Remote LLM on Google Colab (Ollama behind an ngrok tunnel), reached with REST + JSON.

ngrok's free URL changes every time the Colab notebook starts, so the lecturer pastes the
connection string the notebook prints into `python -m gateway.remote "<url>#<token>"`. That
writes a small file the running gateway re-reads, so the gateway never needs a restart.
"""

import json
import os
import sys
import threading
import time
from pathlib import Path
from urllib.parse import urlsplit
from urllib.request import Request, urlopen

# Colab is a convenience, not a dependency: after a failure skip it for a while so boards
# and chat go straight to the local backup instead of each waiting for a dead tunnel.
COOLDOWN_SECONDS = 20.0
# ngrok shows a browser warning page to unknown clients unless this header is present.
HEADERS = {"Content-Type": "application/json; charset=utf-8",
           "ngrok-skip-browser-warning": "1", "User-Agent": "aiot-workshop-gateway"}


def config_path(env=os.environ):
    if env.get("AIOT_REMOTE_FILE"):
        return Path(env["AIOT_REMOTE_FILE"])
    base = env.get("AIOT_MODELS") or Path.home() / ".cache" / "aiot-workshop"
    return Path(base) / "remote_llm.json"


def parse_connection(text):
    """'https://xxxx.ngrok-free.dev#token' -> (url, token)."""
    url, _, token = text.strip().partition("#")
    parts = urlsplit(url)
    if parts.scheme not in ("http", "https") or not parts.netloc or not token:
        raise ValueError('connection string must look like "https://<host>#<token>"')
    return url.rstrip("/"), token


class RemoteEngine:
    """Calls the Colab server: POST /v1/chat {system, history, message, ...} -> {reply}."""

    name = "colab"

    def __init__(self, url=None, token=None, file=None):
        self.fixed = (url.rstrip("/"), token) if url and token else None
        self.file = Path(file) if file else config_path()
        self._mtime = None
        self._config = None
        self._down_until = 0.0
        self._lock = threading.Lock()

    @classmethod
    def from_env(cls, env=os.environ):
        return cls(env.get("AIOT_REMOTE_LLM_URL"), env.get("AIOT_REMOTE_LLM_TOKEN"),
                   config_path(env))

    def config(self):
        """Current (url, token) or None; a new URL from the file clears the cooldown."""
        if self.fixed:
            return self.fixed
        try:
            mtime = self.file.stat().st_mtime_ns
        except OSError:
            self._config, self._mtime = None, None
            return None
        with self._lock:
            if mtime != self._mtime:
                try:
                    data = json.loads(self.file.read_text(encoding="utf-8"))
                    self._config = (data["url"].rstrip("/"), data["token"])
                except (OSError, ValueError, KeyError, AttributeError):
                    self._config = None
                self._mtime = mtime
                self._down_until = 0.0
            return self._config

    def describe(self):
        config = self.config()
        if config is None:
            return 'Colab (not set: run python -m gateway.remote "<url>#<token>")'
        return f"Colab at {config[0]}"

    def last_name(self):
        return self.name

    def ready(self):
        config = self.config()
        if config is None:
            return False
        try:
            with urlopen(Request(config[0] + "/health", headers=HEADERS), timeout=4) as response:
                return json.load(response).get("status") == "ok"
        except (OSError, ValueError):
            return False

    def generate(self, prompt, max_tokens, timeout):
        return self.generate_messages([{"role": "user", "content": prompt}], max_tokens, timeout)

    def generate_messages(self, messages, max_tokens, timeout):
        config = self.config()
        if config is None:
            raise RuntimeError("Colab server not configured")
        if time.monotonic() < self._down_until:
            raise RuntimeError("Colab server cooling down")
        system = "\n\n".join(m["content"] for m in messages if m["role"] == "system")
        rest = [m for m in messages if m["role"] != "system"]
        if not rest or rest[-1]["role"] != "user":
            raise ValueError("last message must be from the user")
        body = json.dumps({
            "system": system, "history": rest[:-1], "message": rest[-1]["content"],
            "max_tokens": max_tokens, "temperature": 0,
        }, ensure_ascii=False).encode("utf-8")
        headers = dict(HEADERS, Authorization=f"Bearer {config[1]}")
        try:
            with urlopen(Request(config[0] + "/v1/chat", data=body, headers=headers),
                         timeout=timeout) as response:
                return json.load(response)["reply"]
        except Exception:
            self._down_until = time.monotonic() + COOLDOWN_SECONDS
            raise


def main():
    args = sys.argv[1:]
    path = config_path()
    if args == ["--clear"]:
        path.unlink(missing_ok=True)
        print("Colab LLM setting removed; gateway uses the local backup only.")
        return 0
    if len(args) != 1:
        print('usage: python -m gateway.remote "<url>#<token>"   |   --clear', file=sys.stderr)
        return 2
    try:
        url, token = parse_connection(args[0])
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps({"url": url, "token": token}), encoding="utf-8")
    print(f"Saved to {path} (running gateways pick it up within seconds)")
    engine = RemoteEngine(file=path)
    if engine.ready():
        print(f"Colab server reachable: {url}")
        return 0
    print("Saved, but the Colab server did not answer /health. Is the notebook still running?",
          file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
