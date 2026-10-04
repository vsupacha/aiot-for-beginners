"""Find this computer's LAN address and let boards discover the gateway by UDP broadcast."""

import socket
import threading

DISCOVER_REQUEST = b"AIOT_DISCOVER 1"
DISCOVER_REPLY = "AIOT_GATEWAY 1 {port}"


def lan_ipv4_addresses():
    """Best-effort list of IPv4 addresses other devices on the LAN can reach."""
    found = []
    probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        # UDP connect sends no packet; it only asks the OS which interface has the default route.
        probe.connect(("10.255.255.255", 1))
        found.append(probe.getsockname()[0])
    except OSError:
        pass
    finally:
        probe.close()
    try:
        found += socket.gethostbyname_ex(socket.gethostname())[2]
    except OSError:
        pass
    unique = []
    for address in found:
        if not address.startswith(("127.", "169.254.", "0.")) and address not in unique:
            unique.append(address)
    return unique


class DiscoveryResponder:
    """Answer `AIOT_DISCOVER 1` broadcasts so boards need no hard-coded gateway IP."""

    def __init__(self, bind_host, http_port, udp_port=None):
        self.http_port = http_port
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind((bind_host, http_port if udp_port is None else udp_port))
        self.port = self.sock.getsockname()[1]
        self.thread = threading.Thread(target=self._serve, daemon=True)

    def start(self):
        self.thread.start()
        return self

    def _serve(self):
        reply = DISCOVER_REPLY.format(port=self.http_port).encode()
        while True:
            try:
                data, sender = self.sock.recvfrom(64)
            except OSError:
                return  # Socket closed.
            if data.strip() == DISCOVER_REQUEST:
                try:
                    self.sock.sendto(reply, sender)
                except OSError:
                    pass

    def close(self):
        self.sock.close()
