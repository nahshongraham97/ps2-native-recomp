"""E1 ownership verification and planted-failure mutation test for 0x001a7510 (RwFrameUpdateObjects)."""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import struct
from pathlib import Path

from classify_stack_call_boundaries import reader

EXPECTED_SHA = "624ddc6f40f31d001aa72a3b386aa2ce2754f08918b32646261f254348ac5906"
START = 0x001A7510
END = 0x001A7578
REQUIRED_CALLER = 0x001A644C


def verify_e1(original, listing_rows: dict[int, bytes]) -> dict:
    body = original(START, END - START)
    actual_sha = hashlib.sha256(body).hexdigest()
    if actual_sha != EXPECTED_SHA:
        raise ValueError(f"001a7510_body_sha_mismatch:{actual_sha}")
    for pc in range(START, END, 4):
        if pc not in listing_rows:
            raise ValueError(f"missing_listing_pc:{pc:08x}")
        if original(pc, 4) != listing_rows[pc]:
            raise ValueError(f"listing_word_mismatch:{pc:08x}")
    jal_target = 0x0C000000 | (START >> 2)
    caller_word = struct.unpack("<I", original(REQUIRED_CALLER, 4))[0]
    if caller_word != jal_target:
        raise ValueError(f"missing_required_caller_001a644c:{caller_word:08x}")
    return {
        "address": "001a7510",
        "end_exclusive": f"{END:08x}",
        "instruction_count": (END - START) // 4,
        "body_sha256": actual_sha,
        "required_caller": f"{REQUIRED_CALLER:08x}",
    }


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--parent-root", type=Path, required=True)
    args = ap.parse_args()
    P = args.parent_root.resolve()
    elf = (P / "research/inputs/SLUS_214.10").read_bytes()
    original = reader(elf)
    listing_path = P / "research/runs/ghidra-function-coverage-20261118/export-20261118-230132-1875-18160/instructions.tsv"
    listing_rows = {}
    with listing_path.open(encoding="utf-8-sig", newline="") as f:
        for row in csv.DictReader(f, delimiter="\t"):
            pc = int(row["address"], 16)
            if START <= pc < END:
                listing_rows[pc] = bytes.fromhex(row["bytes"])

    positive = verify_e1(original, listing_rows)

    # Planted mutation 1: corrupt body word at 0x001a7510
    def mutated_body(va, size):
        data = bytearray(original(va, size))
        if va <= START < va + size:
            data[START - va] ^= 0x01
        return bytes(data)

    # Planted mutation 2: corrupt caller JAL at 0x001a644c
    def mutated_caller(va, size):
        data = bytearray(original(va, size))
        if va <= REQUIRED_CALLER < va + size:
            data[REQUIRED_CALLER - va] ^= 0x01
        return bytes(data)

    mutations = []
    for name, fn in (("planted_body_mutation", mutated_body), ("planted_caller_mutation", mutated_caller)):
        try:
            verify_e1(fn, listing_rows)
        except ValueError as e:
            mutations.append({"mutation": name, "status": "REJECTED", "reason": str(e)})
        else:
            raise SystemExit(f"FAIL: {name} was not rejected")

    print(json.dumps({"status": "PASS_E1_001A7510_VERIFIED_AND_PLANTS_REJECTED", "e1": positive, "mutations": mutations}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
