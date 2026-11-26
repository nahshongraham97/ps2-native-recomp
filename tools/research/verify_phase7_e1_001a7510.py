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
XBOX_CAM_SET_VIEW_WINDOW = 0x002BB870
XBOX_CAM_CALL_SITE = 0x002BB8AB
XBOX_FRAME_UPDATE_OBJECTS = 0x002BF2C0
XBOX_FRAME_UPDATE_LEN = 0x5D
XBOX_DIRTY_LIST_HEAD = 0x008353FC


def count_jal_callers(elf: bytes, target: int) -> list[int]:
    e_phoff = struct.unpack_from("<I", elf, 28)[0]
    e_phentsize, e_phnum = struct.unpack_from("<HH", elf, 42)
    jal_word = 0x0C000000 | (target >> 2)
    callers: list[int] = []
    for i in range(e_phnum):
        p_type, p_offset, p_vaddr, _, p_filesz, _, p_flags, _ = struct.unpack_from(
            "<8I", elf, e_phoff + i * e_phentsize
        )
        if p_type == 1 and (p_flags & 1):
            for off in range(0, p_filesz, 4):
                if struct.unpack_from("<I", elf, p_offset + off)[0] == jal_word:
                    callers.append(p_vaddr + off)
    return callers


def read_xbe(xbe: bytes, va: int, size: int) -> bytes:
    base = struct.unpack_from("<I", xbe, 0x104)[0]
    n_secs = struct.unpack_from("<I", xbe, 0x11C)[0]
    sec_hdrs = struct.unpack_from("<I", xbe, 0x120)[0] - base
    for i in range(n_secs):
        off = sec_hdrs + i * 56
        vaddr, vsize, raddr, _ = struct.unpack_from("<4I", xbe, off + 4)
        if vaddr <= va < vaddr + vsize:
            foff = raddr + (va - vaddr)
            return xbe[foff : foff + size]
    raise ValueError(f"xbe_va_not_mapped:{va:08x}")


def verify_xbox_semantic(xbe_reader) -> dict:
    call_bytes = xbe_reader(XBOX_CAM_CALL_SITE, 5)
    if call_bytes[0] != 0xE8:
        raise ValueError(f"xbox_expected_call_at_{XBOX_CAM_CALL_SITE:08x}")
    rel = struct.unpack_from("<i", call_bytes, 1)[0]
    callee = XBOX_CAM_CALL_SITE + 5 + rel
    if callee != XBOX_FRAME_UPDATE_OBJECTS:
        raise ValueError(f"xbox_callee_mismatch:{callee:08x}")
    fn = xbe_reader(XBOX_FRAME_UPDATE_OBJECTS, XBOX_FRAME_UPDATE_LEN)
    # Semantic structural match to RenderWare RwFrameUpdateObjects:
    # +0xa0 root, +0x03 private_flags (&3, |=3, self |=12), +0x08/+0x0c/+0x04 dirty list at 0x008353fc
    required_fragments = (
        bytes.fromhex("8b90a0000000"),  # mov edx, [eax+0xa0] (frame->root)
        bytes.fromhex("0fb64a03f6c103"),  # movzx ecx, byte [edx+3]; test cl, 3
        struct.pack("<I", XBOX_DIRTY_LIST_HEAD),  # dirty list head global
        bytes.fromhex("897208"),  # mov [edx+8], esi (root->dirty_next)
        bytes.fromhex("c7420c") + struct.pack("<I", XBOX_DIRTY_LIST_HEAD),  # mov [edx+0xc], &head
        bytes.fromhex("895604"),  # mov [esi+4], edx (old_head->dirty_prev)
        bytes.fromhex("80c903884a03"),  # or cl, 3; mov [edx+3], cl
        bytes.fromhex("8048030cc3"),  # or byte [eax+3], 0x0c; ret
    )
    for frag in required_fragments:
        if frag not in fn:
            raise ValueError(f"xbox_semantic_fragment_missing:{frag.hex()}")
    return {
        "xbox_caller": f"{XBOX_CAM_SET_VIEW_WINDOW:08x}",
        "xbox_call_site": f"{XBOX_CAM_CALL_SITE:08x}",
        "xbox_callee": f"{XBOX_FRAME_UPDATE_OBJECTS:08x}",
        "xbox_callee_sha256": hashlib.sha256(fn).hexdigest(),
        "dirty_list_head": f"{XBOX_DIRTY_LIST_HEAD:08x}",
    }


def verify_e1(original, listing_rows: dict[int, bytes], elf: bytes | None = None, xbe_reader=None) -> dict:
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
    computed_callers = count_jal_callers(elf, START) if elf is not None else []
    if elf is not None and REQUIRED_CALLER not in computed_callers:
        raise ValueError("required_caller_not_in_elf_scan")
    xbox_sem = verify_xbox_semantic(xbe_reader) if xbe_reader is not None else None
    return {
        "address": "001a7510",
        "end_exclusive": f"{END:08x}",
        "instruction_count": (END - START) // 4,
        "body_sha256": actual_sha,
        "required_caller": f"{REQUIRED_CALLER:08x}",
        "computed_jal_caller_count": len(computed_callers),
        "xbox_semantic_e1": xbox_sem,
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

    xbe_path = P / "private-inputs/xbox-mka-20261121/default.xbe"
    xbe_bytes = xbe_path.read_bytes()
    xbe_reader = lambda va, sz: read_xbe(xbe_bytes, va, sz)

    positive = verify_e1(original, listing_rows, elf=elf, xbe_reader=xbe_reader)

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

    # Planted mutation 3: corrupt Xbox semantic fragment in 0x002bf2c0 (+12 = +3 private_flags check)
    def mutated_xbe(va, size):
        data = bytearray(xbe_reader(va, size))
        if va <= XBOX_FRAME_UPDATE_OBJECTS + 12 < va + size:
            data[XBOX_FRAME_UPDATE_OBJECTS + 12 - va] ^= 0x01
        return bytes(data)

    mutations = []
    for name, fn, xfn in (
        ("planted_body_mutation", mutated_body, xbe_reader),
        ("planted_caller_mutation", mutated_caller, xbe_reader),
        ("planted_xbox_semantic_mutation", original, mutated_xbe),
    ):
        try:
            verify_e1(fn, listing_rows, elf=elf, xbe_reader=xfn)
        except ValueError as e:
            mutations.append({"mutation": name, "status": "REJECTED", "reason": str(e)})
        else:
            raise SystemExit(f"FAIL: {name} was not rejected")

    print(json.dumps({"status": "PASS_E1_001A7510_VERIFIED_AND_PLANTS_REJECTED", "e1": positive, "mutations": mutations}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
