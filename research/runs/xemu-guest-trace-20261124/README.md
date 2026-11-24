# Authenticated xemu Guest Debug Trace (Xbox default.xbe 0x0025d5b0)

## Overview
This run artifact captures genuine guest execution of the Mortal Kombat: Armageddon retail Xbox XBE (`default.xbe`, SHA-256 `87abec079a45b5b603f1310e45e011951d629c86b8451bfad97d1a7f8088f17e`) under xemu 0.8.139 (QEMU 10.2.0).

## Tooling & Protocol
- **xemu Invocation**: Launched with `-s -S -monitor tcp:127.0.0.1:4444,server,nowait`.
- **GDB Remote Serial Protocol (RSP)**: `gdb_rsp.py` connects to `127.0.0.1:1234`.
- **Breakpoint**: Hardware execution breakpoint (`Z1,25d5b0,1`) set at `0x0025d5b0` (movie filename variant selector).

## Execution Facts Verified
1. **Entry**: Breakpoint triggered at ~16.73s VM execution time during title boot.
   - Registers: `EIP = 0x0025d5b0`, `ESP = 0x00ecb204`, `EBP = 0x00ecb328`.
   - Caller Return Address: `0x0025d40e`.
   - Argument: `movie_id = 0` (first attract movie: `LOGON` / `LOGOW`).
2. **Subroutine Call Flow**:
   - `0x0025d5b0`: clamps `movie_id` to range 0..32.
   - `0x0025d5c7`: calls `0x0027c790` (widescreen query).
   - In `0x0027c790`: calls `0x002a37cb`, which executes `call 0x002eac82` (`jmp dword ptr [0x004d77f8]`, Xbox Kernel Ordinal 24 / `0x18`, `ExQueryNonVolatileSetting` with setting index 8 `XC_VIDEO_FLAGS`).
   - Under xemu's default video setting, widescreen query returned 0.
   - At `0x0025d5ce`: `je 0x25d5f0` falls back to normal 4:3 descriptor entry at `0x00579828 + movie_id * 40`.
   - At `0x0025d5f6`: loads pointer `0x00504b7c` (`"LOGON.SFD\0"`).
3. **Return**:
   - Instruction `0x0025d605`: `ret` returning to caller `0x0025d40e`.
   - `EAX`: `0x00504b7c`.
   - Caller formatting: Caller at `0x0025d508` pushes `0x00504cb8` (`"/movies/%s"`) and `EAX`, formatting `"/movies/LOGON.SFD"`, and dispatches `call 0x0027c4b0` to stream the movie.

## Artifact Contents
- `gdb_rsp.py`: Pure Python GDB Remote Serial Protocol client.
- `capture_xemu_trace.py`: Trace driver recording 40 single-step instruction states.
- `capture_xemu_return.py`: Breakpoint driver recording pre-call and return register snapshots.
- `xemu_trace_0025d5b0.json`: 40-step instruction execution log.
- `xemu_trace_return_0025d5b0.json`: Return register and memory writeback facts.
