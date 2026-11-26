"""Replay pinned Track A source witnesses and the frozen A2/A3 226-table census.

Read-only on original ELF, listing and ledgers. Writes private evidence only;
never updates ownership, class names or native credit from pointer heuristics.
"""
from __future__ import annotations
import argparse
from collections import defaultdict
import csv
import hashlib
import json
from pathlib import Path
import struct

import audit_ps2_rtti as rtti
import bounded_ownership as bounded
import installed_vtable_members as installed
from verify_phase7_capture import parse_capture

TARGETS = {0x16ae10: bytes.fromhex("14008290bc160200fe1702000800e0032b100200"),
           0x2e1710: bytes.fromhex("0800e00300000000")}


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def prove(fork, output, capture_root=None):
    parent = fork.parents[2]
    state_path = fork / "research/runs/renderer-scope.json"
    state = json.loads(state_path.read_bytes())
    context = state["bounded_inputs"]
    elf_path = parent / "research/inputs/SLUS_214.10"
    raw = elf_path.read_bytes()
    if sha(raw) != rtti.ELF_SHA:
        raise ValueError("original ELF identity differs")
    loads = rtti.segments(raw)
    def original(va, length):
        for segment in loads:
            delta = va - segment["vaddr"]
            if 0 <= delta and delta + length <= segment["filesz"]:
                begin = segment["offset"] + delta
                return raw[begin:begin + length]
        raise ValueError(f"unmapped original file-backed range {va:08x}+{length}")
    listing = parent / context["ghidra_reproduction"]["private_listing"]["path"]
    if sha(listing.read_bytes()) != context["ghidra_reproduction"]["private_listing"]["sha256"]:
        raise ValueError("original EE listing pin differs")
    directory = listing.parent
    bodies_path = directory / "function-bodies.tsv"
    body_pin = context["nis_class_frontier"]["input_pins"][str(bodies_path.relative_to(parent)).replace("\\", "/")]
    if sha(bodies_path.read_bytes()) != body_pin:
        raise ValueError("original function-body manifest differs")
    ledger_path = fork / "research/ledger/functions.csv"
    rows = list(csv.DictReader(ledger_path.read_text("utf-8-sig").splitlines()))
    catalogue = {int(r["address"], 16): r["symbol"] for r in rows
                 if r["module"] == "SLUS_214.10" and r["catalogue_kind"] == "callable"}
    candidates = defaultdict(list)
    for item in context["nis_class_frontier"]["global_abi_audit"]["constructor_like_table_stores"]:
        candidates[item["candidate_vptr_va"]].append(item)
    candidates = [{"vtable_va": va, "stores": stores} for va, stores in sorted(candidates.items())]
    # Every instruction used as a method-code address is joined to original bytes.
    code = set()
    target_listing = defaultdict(list)
    for row in bounded.csv_rows(listing):
        pc = int(row["address"], 16)
        expected = bytes.fromhex(row["bytes"])
        if original(pc, len(expected)) != expected:
            raise ValueError(f"original EE listing word differs {pc:08x}")
        code.add(pc)
        if int(row["function_entry"] or "0", 16) in TARGETS:
            target_listing[int(row["function_entry"] or "0", 16)].append(row)
    text_ranges = [(s["vaddr"], s["vaddr"] + s["filesz"]) for s in loads if s["flags"] & 1]
    census = installed.classify_candidates(original, candidates, text_ranges, code, catalogue)
    census["self_test"] = installed.self_test(original, candidates, text_ranges, code, catalogue)
    audit = rtti.audit(raw, catalogue)
    audit["self_test"] = rtti.self_test()
    graph = bounded.direct_calls(directory, original, set(catalogue))
    owner = bounded.code_ranges(directory)
    target_reports = []
    for target, expected in TARGETS.items():
        if original(target, len(expected)) != expected:
            raise ValueError(f"target original instructions differ {target:08x}")
        joined = {int(r["address"], 16) for r in target_listing[target]}
        if joined != set(range(target, target + len(expected), 4)):
            raise ValueError("target complete function body/listing coverage differs")
        calls = [{"caller": f"{caller:08x}", "callsite": f"{site:08x}",
                  "jal_word_le": original(site, 4).hex(),
                  "delay_word_le": original(site + 4, 4).hex()}
                 for (caller, callee), sites in graph.items() if callee == target for site in sites]
        tables = [{"table": t["vtable_va"], "classification": t["classification"], "slot": m["slot_va"]}
                  for t in census["candidates"] for m in t["possible_method_pointer_words"] if int(m["method"], 16) == target]
        status = next(r["ownership_status"] for r in rows if r["module"] == "SLUS_214.10" and int(r["address"], 16) == target)
        target_reports.append({"method": f"{target:08x}", "original_bytes_le": expected.hex(),
            "original_body_sha256": sha(expected), "listing": target_listing[target],
            "direct_callers": calls, "candidate_table_references": tables,
            "admitted_ownership_status": status, "promotable": False, "native_credit": False,
            "ownership_limit": "No independently proved game resource factory/constructor/vtable or library identity chain for this function. Original callers and pointer references are observations; graph propagation alone supplies no verified ownership."})
    captures = []
    if capture_root:
        for path in sorted(capture_root.resolve().rglob("cap_*.bin")):
            data = path.read_bytes()
            info = parse_capture(data)
            pc = int(info["pc"], 16)
            entry = data[36:36 + 1456]
            after = 36 + 1456 + (32 << 20) + (16 << 10)
            exit_state = data[after:after + 1456]
            ra, = struct.unpack_from("<I", entry, 31 * 16)
            exit_pc, = struct.unpack_from("<I", exit_state, 548)
            entry_sp = entry[29 * 16:30 * 16]
            exit_sp = exit_state[29 * 16:30 * 16]
            site = (ra - 8) & 0xffffffff
            word = int.from_bytes(original(site, 4), "little")
            direct_target = ((site + 4) & 0xf0000000) | ((word & 0x3ffffff) << 2) if word >> 26 == 3 else None
            if exit_pc != ra or entry_sp != exit_sp or direct_target != pc:
                raise ValueError(f"partial/unproved ordinary-return capture {path}")
            captures.append({"path": str(path.relative_to(fork)), "sha256": sha(data),
                "pc": info["pc"], "frame": info["frame"], "whole_ordinary_return": True,
                "caller": f"{owner(site):08x}", "source_callsite": f"{site:08x}",
                "source_jal_le": original(site, 4).hex(), "source_delay_le": original(site + 4, 4).hex(),
                "entry_cpu_pc": info["entry_pc"], "exit_pc": info["exit_pc"], "ra": f"0x{ra:08x}",
                "source_entry_pc_limit": "Producer captures wrapper/block entry pc, not target entry; target identity is joined independently through original JAL at saved RA-8.",
                "cpu_changed_word_offsets": [i for i in range(0, 1456, 4) if entry[i:i+4] != exit_state[i:i+4]]})
    promotable = sorted({m["method"] for m in census["game_members"] if m["promotion_eligible"]})
    withheld = [{"method": m["method"], "table": m["table"], "outside": m["shared_outside_confirmed_game_tables"]}
                for m in census["game_members"] if not m["promotion_eligible"]]
    result = {"schema": "mkar.track-a-original-ownership-and-census.v1", "scope": "source-derived; no game run or adoption/credit",
        "input_pins": {"elf": sha(raw), "listing": sha(listing.read_bytes()), "bodies": body_pin,
                       "ledger": sha(ledger_path.read_bytes()), "scope": sha(state_path.read_bytes())},
        "targets": target_reports, "census": census, "rtti_audit": audit,
        "promotable_game_methods": promotable, "withheld_confirmed_slots": withheld,
        "failed_or_unresolved_candidates": [{"table": t["vtable_va"], "status": t["classification"],
            "reason": t["classification_reason"], "unknowns": t["unknowns"]} for t in census["candidates"] if t["classification"] != "confirmed-vtable"],
        "capture_receipts": captures,
        "implementation_credit": 0, "new_ownership_admissions": 0}
    output = output.resolve()
    if not output.is_relative_to((fork / "build/oracle-telemetry").resolve()) or output.exists():
        raise ValueError("output must be new private telemetry path")
    output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps({"output": str(output), "sha256": sha(output.read_bytes()),
        "targets": [{k: r[k] for k in ["method", "admitted_ownership_status", "promotable"]} for r in target_reports],
        "candidate_count": census["candidate_count"], "classifications": census["classification_counts"],
        "promotable_game_methods": promotable, "capture_count": len(captures), "self_test": census["self_test"]["status"]}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fork", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--capture-root", type=Path)
    args = parser.parse_args()
    prove(args.fork.resolve(), args.output, args.capture_root)
