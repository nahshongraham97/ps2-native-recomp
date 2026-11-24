import subprocess
import time
import json
from pathlib import Path
from gdb_rsp import GdbClient

def main():
    xemu_exe = r"C:\Users\nahsh\Downloads\xemu-0.8.139-windows-x86_64\xemu.exe"
    args = [xemu_exe, "-s", "-S", "-monitor", "tcp:127.0.0.1:4444,server,nowait"]

    print("=== Capturing Xemu Widescreen Guest Trace ===")
    proc = subprocess.Popen(args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    try:
        time.sleep(2)
        gdb = GdbClient(port=1234)
        gdb.send_cmd("qSupported:multiprocess+;swbreak+;hwbreak+;qRelocInsn+")

        print("Setting HW breakpoint at entry (0x0025d5b0)...")
        gdb.set_hw_breakpoint(0x0025d5b0)
        stop = gdb.cont(timeout=35.0)
        print(f"Hit entry 0x0025d5b0: {stop}")
        gdb.remove_hw_breakpoint(0x0025d5b0)

        # Set widescreen active flag at 0x005bfa64 = 1
        gdb.write_mem(0x005bfa64, (1).to_bytes(4, 'little'))

        entry_regs = gdb.read_regs()
        esp = entry_regs["esp"]
        stack_bytes = gdb.read_mem(esp, 16)
        ret_addr = int.from_bytes(stack_bytes[:4], 'little')
        arg0 = int.from_bytes(stack_bytes[4:8], 'little')

        print(f"Entry: ESP=0x{esp:08x}, ret_addr=0x{ret_addr:08x}, arg0={arg0}")

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
                print(f"Step {i} reached target {hex(regs_after['eip'])}")
                break

        # Break at ret 0x0025d605 to observe returned string
        gdb.set_hw_breakpoint(0x0025d605)
        stop = gdb.cont(timeout=10.0)
        gdb.remove_hw_breakpoint(0x0025d605)

        ret_regs = gdb.read_regs()
        eax = ret_regs['eax']
        ret_str_raw = gdb.read_mem(eax, 16)
        ret_str = ret_str_raw.split(b'\0')[0].decode('ascii', 'replace')
        print(f"At ret: EAX=0x{eax:08x} ('{ret_str}')")

        # Step into caller and run until sprintf returns at 0x0025d519
        gdb.set_hw_breakpoint(0x0025d519)
        stop = gdb.cont(timeout=10.0)
        gdb.remove_hw_breakpoint(0x0025d519)

        caller_regs = gdb.read_regs()
        ebp = caller_regs['ebp']
        buf_raw = gdb.read_mem(ebp - 0x108, 64)
        formatted_str = buf_raw.split(b'\0')[0].decode('ascii', 'replace')
        print(f"Caller sprintf formatted string: '{formatted_str}'")

        trace_data = {
            "tool": "xemu 0.8.139",
            "qemu_version": "10.2.0",
            "target": "Xbox default.xbe",
            "widescreen_flag_address": "0x005bfa64",
            "widescreen_flag_value": 1,
            "function_name": "movie_selector_0025d5b0",
            "function_entry": "0x0025d5b0",
            "entry_registers": {k: f"0x{v:08x}" for k, v in entry_regs.items()},
            "argument_movie_id": arg0,
            "caller_return_address": f"0x{ret_addr:08x}",
            "return_value_eax": f"0x{eax:08x}",
            "returned_filename": ret_str,
            "caller_sprintf_instruction": "0x0025d514",
            "caller_formatted_path": formatted_str,
            "steps": steps
        }

        out_path = Path(r"c:\Users\nahsh\mk port\Armageddon-backup\armageddon-reimplementation\external\forks\retail-main-22\research\runs\xemu-guest-trace-20261124\xemu_trace_widescreen_0025d5b0.json")
        out_path.write_text(json.dumps(trace_data, indent=2))
        print(f"Saved widescreen trace to {out_path}")

    finally:
        try: gdb.close()
        except: pass
        proc.terminate()
        try: proc.wait(timeout=3)
        except: proc.kill()
        print("xemu stopped.")

if __name__ == "__main__":
    main()
