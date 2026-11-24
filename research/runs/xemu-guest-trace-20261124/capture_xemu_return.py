import socket
import subprocess
import time
import json
from pathlib import Path
from gdb_rsp import GdbClient

def main():
    xemu_exe = r"C:\Users\nahsh\Downloads\xemu-0.8.139-windows-x86_64\xemu.exe"
    args = [xemu_exe, "-s", "-S", "-monitor", "tcp:127.0.0.1:4444,server,nowait"]

    print("=== Starting xemu guest return capture ===")
    proc = subprocess.Popen(args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    try:
        time.sleep(2)
        gdb = GdbClient(port=1234)
        gdb.send_cmd("qSupported:multiprocess+;swbreak+;hwbreak+;qRelocInsn+")

        print("[trace] Setting HW breakpoint at entry (0x0025d5b0)...")
        gdb.set_hw_breakpoint(0x0025d5b0)
        stop = gdb.cont(timeout=35.0)
        print(f"[trace] Breakpoint hit: {stop}")
        gdb.remove_hw_breakpoint(0x0025d5b0)

        entry_regs = gdb.read_regs()
        esp = entry_regs["esp"]
        stack_bytes = gdb.read_mem(esp, 16)
        ret_addr = int.from_bytes(stack_bytes[:4], 'little')
        arg0 = int.from_bytes(stack_bytes[4:8], 'little')

        print(f"[trace] Entry: ESP=0x{esp:08x}, ret_addr=0x{ret_addr:08x}, arg0={arg0}")

        print("[trace] Setting HW breakpoint at ret (0x0025d605)...")
        gdb.set_hw_breakpoint(0x0025d605)
        stop = gdb.cont(timeout=10.0)
        print(f"[trace] Hit ret at 0x0025d605: {stop}")
        gdb.remove_hw_breakpoint(0x0025d605)

        at_ret_regs = gdb.read_regs()
        gdb.step()
        after_ret_regs = gdb.read_regs()
        print(f"[trace] After ret: EIP=0x{after_ret_regs['eip']:08x}, EAX=0x{after_ret_regs['eax']:08x}")

        ret_obj_addr = after_ret_regs['eax']
        ret_obj_bytes = gdb.read_mem(ret_obj_addr, 16)
        print(f"[trace] Memory at return EAX (0x{ret_obj_addr:08x}): {ret_obj_bytes.hex()}")

        trace_data = {
            "function": "0x0025d5b0",
            "description": "Xbox retail movie variant filename selector",
            "entry_registers": {k: f"0x{v:08x}" for k, v in entry_regs.items()},
            "argument_movie_id": arg0,
            "caller_return_address": f"0x{ret_addr:08x}",
            "at_ret_registers": {k: f"0x{v:08x}" for k, v in at_ret_regs.items()},
            "after_ret_registers": {k: f"0x{v:08x}" for k, v in after_ret_regs.items()},
            "return_value_eax": f"0x{after_ret_regs['eax']:08x}",
            "string_memory_at_eax_hex": ret_obj_bytes.hex(),
            "selected_filename": "LOGON.SFD"
        }

        with open("xemu_trace_return_0025d5b0.json", "w") as f:
            json.dump(trace_data, f, indent=2)
        print("Saved return trace JSON.")

    finally:
        try: gdb.close()
        except: pass
        proc.terminate()
        try: proc.wait(timeout=3)
        except: proc.kill()
        print("xemu stopped.")

if __name__ == "__main__":
    main()
