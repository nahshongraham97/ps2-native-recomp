import socket
import subprocess
import time
import json
from pathlib import Path
from gdb_rsp import GdbClient

class MonitorClient:
    def __init__(self, host="127.0.0.1", port=4444):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((host, port))
        time.sleep(0.2)
        self.sock.recv(4096)

    def send_command(self, cmd: str) -> str:
        self.sock.sendall(f"{cmd}\n".encode('utf-8'))
        time.sleep(0.5)
        res = b""
        while True:
            try:
                self.sock.settimeout(1.0)
                chunk = self.sock.recv(4096)
                if not chunk:
                    break
                res += chunk
                if b"(qemu)" in res:
                    break
            except socket.timeout:
                break
        self.sock.settimeout(None)
        return res.decode('utf-8', 'replace')

    def close(self):
        try: self.sock.close()
        except: pass

def main():
    xemu_exe = r"C:\Users\nahsh\Downloads\xemu-0.8.139-windows-x86_64\xemu.exe"
    args = [xemu_exe, "-s", "-S", "-monitor", "tcp:127.0.0.1:4444,server,nowait"]

    print("=== Starting xemu guest debug trace capture ===")
    proc = subprocess.Popen(args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    try:
        time.sleep(2)
        gdb = GdbClient(port=1234)
        gdb.send_cmd("qSupported:multiprocess+;swbreak+;hwbreak+;qRelocInsn+")
        mon = MonitorClient(port=4444)

        # Monitor smoke test
        mon_regs = mon.send_command("info registers")
        print("[monitor smoke] info registers:\n", mon_regs[:200])

        # Breakpoint at 0x0025d5b0 (movie selector entry)
        print("[trace] Setting HW breakpoint at 0x0025d5b0...")
        gdb.set_hw_breakpoint(0x0025d5b0)
        stop = gdb.cont(timeout=35.0)
        print(f"[trace] Breakpoint hit response: {stop}")
        gdb.remove_hw_breakpoint(0x0025d5b0)

        entry_regs = gdb.read_regs()
        print(f"[trace] Hit 0x0025d5b0: EIP=0x{entry_regs['eip']:08x}, ESP=0x{entry_regs['esp']:08x}, EBP=0x{entry_regs['ebp']:08x}")

        # Step 40 instructions
        steps = []
        for i in range(40):
            regs_before = gdb.read_regs()
            eip = regs_before['eip']
            gdb.step()
            regs_after = gdb.read_regs()
            steps.append({
                "step": i,
                "eip": f"0x{eip:08x}",
                "registers_before": {k: f"0x{v:08x}" for k, v in regs_before.items()},
                "registers_after": {k: f"0x{v:08x}" for k, v in regs_after.items()}
            })
            if regs_after['eip'] == 0x0025d605 or regs_after['eip'] == 0x0025d40e:
                print(f"[trace] Step {i} reached target {hex(regs_after['eip'])}")
                break

        out_data = {
            "tool": "xemu 0.8.139",
            "qemu_version": "10.2.0",
            "target": "Xbox default.xbe",
            "function_name": "movie_selector_0025d5b0",
            "function_entry": "0x0025d5b0",
            "steps": steps
        }
        with open("xemu_trace_0025d5b0.json", "w") as f:
            json.dump(out_data, f, indent=2)
        print("Trace steps saved to xemu_trace_0025d5b0.json")

    finally:
        try: gdb.close()
        except: pass
        try: mon.close()
        except: pass
        proc.terminate()
        try: proc.wait(timeout=3)
        except: proc.kill()
        print("xemu stopped.")

if __name__ == "__main__":
    main()
