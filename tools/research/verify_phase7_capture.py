"""Verify Phase 7 PS2CAP01 entry/exit state captures and reject planted corruption."""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path


def parse_capture(raw: bytes) -> dict:
    if len(raw) < 40 + 32:
        raise ValueError("capture_truncated_header")
    payload, trailer = raw[:-32], raw[-32:]
    if hashlib.sha256(payload).digest() != trailer:
        raise ValueError("capture_sha256_mismatch")
    magic = raw[:8]
    if magic != b"PS2CAP01":
        raise ValueError(f"bad_magic:{magic!r}")
    pc, entry, frame, cpu_sz, ram_sz, spr_sz = struct.unpack_from("<IIQIII", raw, 8)
    if cpu_sz != 1456 or ram_sz != (32 << 20) or spr_sz != (16 << 10):
        raise ValueError(f"unexpected_sizes:{cpu_sz},{ram_sz},{spr_sz}")
    pos = 36
    entry_cpu = raw[pos:pos + cpu_sz]; pos += cpu_sz
    entry_ram = raw[pos:pos + ram_sz]; pos += ram_sz
    entry_spr = raw[pos:pos + spr_sz]; pos += spr_sz
    exit_cpu = raw[pos:pos + cpu_sz]; pos += cpu_sz
    exit_ram = raw[pos:pos + ram_sz]; pos += ram_sz
    exit_spr = raw[pos:pos + spr_sz]; pos += spr_sz
    (scount,) = struct.unpack_from("<I", raw, pos); pos += 4
    expected_len = pos + scount * 8 + 32
    if len(raw) != expected_len:
        raise ValueError(f"store_table_length_mismatch:{len(raw)}!={expected_len}")
    stores = [struct.unpack_from("<II", raw, pos + i * 8) for i in range(scount)]

    def reg32(cpu_bytes: bytes, idx: int) -> int:
        return struct.unpack_from("<I", cpu_bytes, idx * 16)[0]

    entry_pc = struct.unpack_from("<I", entry_cpu, 548)[0]
    exit_pc = struct.unpack_from("<I", exit_cpu, 548)[0]
    return {
        "pc": f"0x{pc:08x}",
        "entry": entry,
        "frame": frame,
        "entry_pc": f"0x{entry_pc:08x}",
        "exit_pc": f"0x{exit_pc:08x}",
        "entry_a0": f"0x{reg32(entry_cpu, 4):08x}",
        "entry_a1": f"0x{reg32(entry_cpu, 5):08x}",
        "entry_sp": f"0x{reg32(entry_cpu, 29):08x}",
        "entry_ra": f"0x{reg32(entry_cpu, 31):08x}",
        "exit_v0": f"0x{reg32(exit_cpu, 2):08x}",
        "exit_sp": f"0x{reg32(exit_cpu, 29):08x}",
        "store_count": scount,
        "stores_sample": [(f"0x{a:08x}", f"0x{v:08x}") for a, v in stores[:12]],
        "ram_bytes_changed": sum(1 for i in range(0, ram_sz, 4) if entry_ram[i:i+4] != exit_ram[i:i+4]) * 4,
    }


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--capture-dir", type=Path, required=True)
    args = ap.parse_args()

    files = sorted(args.capture_dir.glob("cap_*.bin"))
    if not files:
        raise SystemExit("no capture files found")

    results = []
    for p in files:
        raw = p.read_bytes()
        info = parse_capture(raw)
        info["file"] = p.name
        results.append(info)

    # Planted corruption test: flip 1 bit in exit_ram and verify rejection
    corrupted = bytearray(files[0].read_bytes())
    corrupted[36 + 1456 + (32 << 20) + (16 << 10) + 1456 + 16] ^= 0x01
    planted_rejected = False
    try:
        parse_capture(bytes(corrupted))
    except ValueError as e:
        planted_rejected = (str(e) == "capture_sha256_mismatch")

    if not planted_rejected:
        raise SystemExit("FAIL: planted capture corruption was NOT rejected")

    print(json.dumps({
        "status": "PASS_CAPTURE_VERIFIED_AND_PLANT_REJECTED",
        "captures_verified": len(results),
        "planted_corruption_rejected": planted_rejected,
        "samples": results,
    }, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
