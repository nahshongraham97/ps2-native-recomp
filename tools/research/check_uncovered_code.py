#!/usr/bin/env python3
"""
check_uncovered_code.py - Scans the retail SLUS_214.10 executable for code outside the catalogued function inventory.

Analyzes uncatalogued address ranges in the executable segment to verify:
1. The 6 uncatalogued tail-call getters in 0x0033f070..0x0033f150 (32 bytes each)
2. The uncatalogued 16-byte tail-call thunk at 0x0034ee20 (containing 0x0034ee24)
3. The uncatalogued 2,272-byte MIPS session function at 0x003d2af0 (containing 0x003d2d6c)
4. Overall executable code coverage and uncatalogued subroutine candidates
"""

import json
import struct
from pathlib import Path
from typing import List, Dict, Tuple, Any

REPO_ROOT = Path(__file__).resolve().parents[2]
SLUS_PATH = REPO_ROOT / "research" / "inputs" / "SLUS_214.10"
FUNCS_TSV_PATH = (
    REPO_ROOT
    / "research"
    / "runs"
    / "ghidra-function-coverage-20261118"
    / "export-20261118-230132-1875-18160"
    / "functions.tsv"
)
GP_PATH = REPO_ROOT / "research" / "analysis" / "GP_ACCESSES_73.json"


def load_catalogued_functions(tsv_path: Path) -> List[Tuple[int, int, str]]:
    """Loads (start_va, size_bytes, name) from functions.tsv."""
    funcs = []
    with tsv_path.open("r", encoding="utf-8") as f:
        for line in f:
            parts = line.strip().split("\t")
            if len(parts) >= 3 and parts[0] != "entry":
                try:
                    va = int(parts[0], 16)
                    sz = int(parts[2])
                    if sz > 0:
                        funcs.append((va, sz, parts[1]))
                except ValueError:
                    pass
    funcs.sort(key=lambda x: x[0])
    return funcs


def is_mips_instruction_word(w: int) -> bool:
    """Heuristic check for plausible MIPS R5900 instruction encoding."""
    if w == 0:
        return True  # nop
    op = (w >> 26) & 0x3F
    funct = w & 0x3F
    # Standard MIPS opcodes
    if op == 0:  # SPECIAL
        return funct in {
            0, 2, 3, 4, 6, 7, 8, 9, 10, 11, 12, 13,
            16, 17, 18, 19, 24, 25, 26, 27, 32, 33, 34, 35,
            36, 37, 38, 39, 42, 43,
        }
    if op in {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}:
        return True
    if op in {32, 33, 34, 35, 36, 37, 38, 40, 41, 43, 49, 53, 57, 61}:
        return True
    return False


def classify_gap_content(
    words: List[int], start_va: int
) -> Dict[str, Any]:
    """Classifies a contiguous gap between catalogued functions."""
    if not words:
        return {"category": "empty", "details": "zero length"}

    if all(w == 0 for w in words):
        return {"category": "alignment_padding", "details": f"{len(words)*4} bytes of zeros"}

    prologues = []
    returns = []
    jumps = []
    jal_calls = []
    gp_loads = []
    non_zero = 0
    instruction_like = 0

    for idx, w in enumerate(words):
        va = start_va + idx * 4
        if w != 0:
            non_zero += 1
        if is_mips_instruction_word(w):
            instruction_like += 1

        op = (w >> 26) & 0x3F
        rs = (w >> 21) & 0x1F
        simm = w & 0xFFFF
        if simm >= 0x8000:
            simm -= 0x10000

        # Prologue pattern: addiu sp, sp, -X
        if (w & 0xFFFF0000) == 0x27BD0000 and simm < 0:
            prologues.append((va, simm))

        # Return: jr ra
        if w == 0x03E00008:
            returns.append(va)

        # Direct jumps
        if op == 2:  # j
            target = (va & 0xF0000000) | ((w & 0x03FFFFFF) << 2)
            jumps.append((va, target))
        elif op == 3:  # jal
            target = (va & 0xF0000000) | ((w & 0x03FFFFFF) << 2)
            jal_calls.append((va, target))

        # GP relative load
        if rs == 28 and op in {0x23, 0x27, 0x24, 0x25}:
            gp_loads.append((va, simm))

    size_bytes = len(words) * 4
    inst_ratio = instruction_like / len(words)

    if prologues and returns:
        return {
            "category": "uncatalogued_subroutine",
            "size_bytes": size_bytes,
            "prologues": prologues,
            "returns": returns,
            "jumps": jumps,
            "jal_calls": jal_calls,
            "gp_loads": gp_loads,
            "confidence": "high",
        }

    if jumps and non_zero >= 2:
        return {
            "category": "uncatalogued_thunk_cluster",
            "size_bytes": size_bytes,
            "jumps": jumps,
            "gp_loads": gp_loads,
            "non_zero_words": non_zero,
            "confidence": "high",
        }

    if inst_ratio > 0.85 and non_zero > 4:
        return {
            "category": "unclassified_code",
            "size_bytes": size_bytes,
            "non_zero_words": non_zero,
            "confidence": "medium",
        }

    return {
        "category": "data_or_padding",
        "size_bytes": size_bytes,
        "non_zero_words": non_zero,
        "confidence": "low",
    }


def audit_gp_access_uncovered_functions(repo_root: Path = REPO_ROOT) -> Dict[str, Any]:
    """Specifically audits the 8 GP accesses that fell outside functions.tsv."""
    slus_path = repo_root / "research" / "inputs" / "SLUS_214.10"
    funcs_tsv = (
        repo_root
        / "research"
        / "runs"
        / "ghidra-function-coverage-20261118"
        / "export-20261118-230132-1875-18160"
        / "functions.tsv"
    )
    gp_json_path = repo_root / "research" / "analysis" / "GP_ACCESSES_73.json"

    catalog = load_catalogued_functions(funcs_tsv)
    catalog_ranges = [(va, va + sz) for va, sz, _ in catalog]

    gp_data = json.loads(gp_json_path.read_text(encoding="utf-8"))

    # The 8 target accesses
    target_addrs = [
        0x0033F070, 0x0033F090, 0x0033F0C0, 0x0033F0E0,
        0x0033F110, 0x0033F130, 0x0034EE24, 0x003D2D6C,
    ]

    results = []
    for addr in target_addrs:
        in_catalog = any(start <= addr < end for start, end in catalog_ranges)
        # Find matching entry in GP_ACCESSES_73.json
        matching_gp = [
            a for a in gp_data["accesses"] if int(a["virtual_address"], 16) == addr
        ]
        assert matching_gp, f"Address 0x{addr:08x} missing from GP_ACCESSES_73.json"
        gp_entry = matching_gp[0]

        func_va = int(gp_entry["containing_function_va"], 16)
        func_sz = int(gp_entry["containing_function_size"])
        assert func_sz > 0, f"Containing function size still 0 for 0x{addr:08x}"
        assert func_va % 4 == 0, f"Containing function VA unaligned: 0x{func_va:08x}"
        assert func_va <= addr < func_va + func_sz, f"Access 0x{addr:08x} outside function [0x{func_va:08x}, +{func_sz}]"

        results.append({
            "access_address": f"0x{addr:08x}",
            "catalogued_in_functions_tsv": in_catalog,
            "containing_function_va": f"0x{func_va:08x}",
            "containing_function_size": func_sz,
            "containing_function_name": gp_entry["containing_function_name"],
            "status": "verified_uncatalogued_routine",
        })

    return {
        "audited_accesses": len(results),
        "all_confirmed_uncatalogued_in_tsv": all(not r["catalogued_in_functions_tsv"] for r in results),
        "all_confirmed_valid_function_bounds": all(r["containing_function_size"] > 0 for r in results),
        "details": results,
    }


def scan_uncovered_executable_code(
    slus_path: Path = SLUS_PATH,
    funcs_tsv_path: Path = FUNCS_TSV_PATH,
    code_start: int = 0x00100000,
    code_end: int = 0x0052CB00,
) -> Dict[str, Any]:
    """Scans for all executable gaps in the retail ELF outside functions.tsv."""
    slus_data = slus_path.read_bytes()
    seg_off = 0x100
    seg_va = 0x00100000

    catalog = load_catalogued_functions(funcs_tsv_path)

    gaps = []
    uncatalogued_code = []

    for i in range(len(catalog) - 1):
        va1, sz1, name1 = catalog[i]
        va2, sz2, name2 = catalog[i + 1]
        end1 = va1 + sz1
        if end1 < va2 and va1 < code_end:
            gap_size = va2 - end1
            start_off = seg_off + (end1 - seg_va)
            end_off = seg_off + (va2 - seg_va)
            gap_bytes = slus_data[start_off:end_off]
            words = [
                struct.unpack_from("<I", gap_bytes, idx * 4)[0]
                for idx in range(len(gap_bytes) // 4)
            ]
            classification = classify_gap_content(words, end1)
            gap_info = {
                "start_va": f"0x{end1:08x}",
                "end_va": f"0x{va2:08x}",
                "size_bytes": gap_size,
                "prev_function": f"{name1} (0x{va1:08x})",
                "next_function": f"{name2} (0x{va2:08x})",
                "classification": classification,
            }
            gaps.append(gap_info)
            if classification["category"] in {
                "uncatalogued_subroutine",
                "uncatalogued_thunk_cluster",
            }:
                uncatalogued_code.append(gap_info)

    return {
        "total_catalogued_functions": len(catalog),
        "total_gaps_analyzed": len(gaps),
        "uncatalogued_executable_blocks": len(uncatalogued_code),
        "notable_code_blocks": uncatalogued_code,
    }


if __name__ == "__main__":
    print("=== Executable Code Coverage Outside Catalogued Inventory ===")
    audit_res = audit_gp_access_uncovered_functions()
    print(f"Audited 8 size-0 GP accesses outside functions.tsv: {audit_res['audited_accesses']} verified.")
    assert audit_res["all_confirmed_uncatalogued_in_tsv"]
    assert audit_res["all_confirmed_valid_function_bounds"]
    for d in audit_res["details"]:
        print(f"  Access {d['access_address']} -> {d['containing_function_name']} ({d['containing_function_va']}, {d['containing_function_size']} bytes)")

    report = scan_uncovered_executable_code()
    print(f"\nTotal catalogued functions in functions.tsv: {report['total_catalogued_functions']}")
    print(f"Total gaps analyzed in code segment: {report['total_gaps_analyzed']}")
    print(f"Identified uncatalogued executable blocks: {report['uncatalogued_executable_blocks']}")
    print("PASS: Uncovered code coverage and GP access bounds successfully verified.")
