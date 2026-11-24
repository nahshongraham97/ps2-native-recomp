import socket
import time

def checksum(data: str) -> str:
    return f"{sum(data.encode('latin1')) % 256:02x}"

class GdbClient:
    """Minimal, self-contained GDB Remote Serial Protocol (RSP) client for xemu/QEMU."""
    def __init__(self, host="127.0.0.1", port=1234):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((host, port))
        self.sock.sendall(b"+")

    def close(self):
        try:
            self.sock.close()
        except Exception:
            pass

    def send_cmd(self, payload: str) -> str:
        pkt = f"${payload}#{checksum(payload)}"
        self.sock.sendall(pkt.encode('latin1'))
        ack = self.sock.recv(1)
        while ack != b'+':
            ack = self.sock.recv(1)
        res = b""
        while b"#" not in res:
            chunk = self.sock.recv(4096)
            if not chunk:
                break
            res += chunk
        self.sock.sendall(b"+")
        if b"$" in res and b"#" in res:
            start = res.index(b"$") + 1
            end = res.index(b"#")
            return res[start:end].decode('latin1', 'replace')
        return res.decode('latin1', 'replace')

    def read_mem(self, addr: int, length: int) -> bytes:
        resp = self.send_cmd(f"m{addr:x},{length:x}")
        if resp.startswith("E"):
            return b""
        try:
            return bytes.fromhex(resp)
        except Exception:
            return b""

    def write_mem(self, addr: int, data: bytes) -> bool:
        resp = self.send_cmd(f"M{addr:x},{len(data):x}:{data.hex()}")
        return resp == "OK"

    def read_regs(self) -> dict:
        resp = self.send_cmd("g")
        raw = bytes.fromhex(resp)
        names = ["eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi", "eip", "eflags", "cs", "ss", "ds", "es", "fs", "gs"]
        regs = {}
        for i, n in enumerate(names):
            if (i+1)*4 <= len(raw):
                val = int.from_bytes(raw[i*4:(i+1)*4], 'little')
                regs[n] = val
        return regs

    def write_reg(self, reg_index: int, value: int) -> bool:
        val_bytes = value.to_bytes(4, 'little').hex()
        resp = self.send_cmd(f"P{reg_index:x}={val_bytes}")
        return resp == "OK"

    def set_hw_breakpoint(self, addr: int):
        return self.send_cmd(f"Z1,{addr:x},1")

    def remove_hw_breakpoint(self, addr: int):
        return self.send_cmd(f"z1,{addr:x},1")

    def step(self):
        return self.send_cmd("s")

    def cont(self, timeout=30.0):
        pkt = f"$c#{checksum('c')}"
        self.sock.sendall(pkt.encode('latin1'))
        ack = self.sock.recv(1)
        while ack != b'+':
            ack = self.sock.recv(1)
        self.sock.settimeout(timeout)
        try:
            res = b""
            while b"#" not in res:
                chunk = self.sock.recv(1024)
                if not chunk:
                    break
                res += chunk
            self.sock.settimeout(None)
            self.sock.sendall(b"+")
            return res.decode('latin1', 'replace')
        except socket.timeout:
            self.sock.settimeout(None)
            self.sock.sendall(b"\x03")
            return "TIMEOUT"
