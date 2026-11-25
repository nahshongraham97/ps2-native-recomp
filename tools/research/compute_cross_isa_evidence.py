"""Compute cross-ISA signatures (CFG basic blocks, edges, callee sequences, memory offsets)
for all 18 mapped PS2-to-Xbox function pairs.

Outputs JSON signatures matching schema 'cross_isa_pair_signature_v1' to
research/cross_platform/signatures/signature_{ps2_addr}_{xbox_addr}.json.
"""
from __future__ import annotations
import csv
import json
from pathlib import Path
import re
import struct

for _p in [Path(__file__).resolve(), *Path(__file__).resolve().parents]:
    if (_p / "research" / "inputs" / "SLUS_214.10").exists():
        REPO = _p
        break
else:
    REPO = Path(__file__).resolve().parents[2]

ALL_PAIRS = [
    # 18 Candidate mapped pairs (subsystem role correspondence pending dual-platform vectors)
    ("00127120", "0019c680", "mapped-by-role", "Camera task initialization and run"),
    ("00127370", "002330c0", "mapped-by-role", "Game records byte storage"),
    ("0012a170", "0023c720", "mapped-by-role", "Sound catalog table iteration"),
    ("0012a4f0", "0023d8d0", "mapped-by-role", "Sound catalog lookup by ID"),
    ("0012a560", "0023e020", "mapped-by-role", "Sound catalog index to record pointer"),
    ("0012a720", "0023d4d0", "mapped-by-role", "Sound catalog index validation"),
    ("0012a8a0", "0023d720", "mapped-by-role", "Sound bank descriptor lookup"),
    ("0012aa50", "0023c660", "mapped-by-role", "Sound catalog range validator"),
    ("0012abd0", "0023d920", "mapped-by-role", "Sound catalog bank query"),
    ("0012b500", "0023d230", "mapped-by-role", "Sound catalog mode table dispatcher"),
    ("0012c1d0", "0023c700", "mapped-by-role", "Sound reverse-removal helper (modular part 1)"),
    ("0012c1d0", "0023daf0", "mapped-by-role", "Sound reverse-removal helper (modular part 2)"),
    ("0012c300", "0023dc70", "mapped-by-role", "Sound descriptor status query"),
    ("0012ca80", "0023c770", "mapped-by-role", "Sound descriptor initialization"),
    ("00225690", "002c0020", "mapped-by-role", "RenderWare raster host texture upload"),
    ("002ba610", "002ba610", "mapped-by-role", "RenderWare 0407 raster consumer interface transformation"),
    ("003152a0", "0037c170", "mapped-by-role", "Sound job enqueue"),
    ("00317960", "0037c2a0", "mapped-by-role", "Sound job cancel path"),
]


def load_ps2_binary_and_sizes():
    slus_path = REPO / "research" / "inputs" / "SLUS_214.10"
    slus_data = slus_path.read_bytes()

    funcs_tsv = (
        REPO
        / "research"
        / "runs"
        / "ghidra-function-coverage-20261118"
        / "export-20261118-230132-1875-18160"
        / "functions.tsv"
    )
    ps2_sizes = {}
    with funcs_tsv.open(encoding="utf-8") as f:
        for line in f:
            parts = line.strip().split("\t")
            if len(parts) >= 3 and parts[0] != "entry":
                try:
                    ps2_sizes[parts[0]] = int(parts[2])
                except ValueError:
                    pass

    return slus_data, ps2_sizes


def load_xbox_data():
    sb1_path = (
        REPO
        / "research"
        / "runs"
        / "xbox-donor-20261121"
        / "sound-boundary-01"
        / "functions.json"
    )
    sb2_path = (
        REPO
        / "research"
        / "runs"
        / "xbox-donor-20261121"
        / "sound-boundary-02"
        / "functions.json"
    )
    rem_path = REPO / "research" / "runs" / "xbox_remaining_funcs.json"

    funcs = {}
    for p in [sb1_path, sb2_path, rem_path]:
        if p.exists():
            with p.open(encoding="utf-8") as f:
                d = json.load(f)
                for c in d.get("function_candidates", []):
                    funcs[c["entry"]] = c
    return funcs


def load_cross_map(map_path=None):
    map_path = map_path or REPO / "research" / "ledger" / "ps2_xbox_map.csv"
    ps2_to_xb = {}
    if map_path.exists():
        with open(map_path, "r", encoding="utf-8") as f:
            for row in csv.DictReader(f):
                # The CFG/donor comparison uses its independently checked pair
                # signatures. Heuristic ownership candidates are not callee proof.
                if not row.get("map_evidence", "").startswith("research/cross_platform/signatures/"):
                    continue
                xb = row.get("xbox_address", "none")
                if xb != "none":
                    ps2_to_xb[row["ps2_address"]] = xb.replace("0x", "").lower().zfill(8)
    return ps2_to_xb


def canonicalize_cfg(blocks):
    succs = {b["id"]: [] for b in blocks}
    preds = {b["id"]: [] for b in blocks}
    va_to_id = {b["start"]: b["id"] for b in blocks}

    for b in blocks:
        bid = b["id"]
        for s_va in b.get("successors", []):
            if s_va in va_to_id:
                sid = va_to_id[s_va]
                succs[bid].append(sid)
                preds[sid].append(bid)

    active_nodes = set(succs.keys())
    changed = True
    while changed:
        changed = False
        for u in list(active_nodes):
            if u not in active_nodes:
                continue
            if len(succs[u]) == 1:
                v = succs[u][0]
                if v in active_nodes and v != u and len(preds[v]) == 1 and preds[v][0] == u:
                    succs[u] = succs[v]
                    for w in succs[v]:
                        preds[w] = [u if p == v else p for p in preds[w]]
                    active_nodes.remove(v)
                    del succs[v]
                    del preds[v]
                    changed = True
                    break

    decision_blocks = sum(1 for u in active_nodes if len(succs[u]) > 1)
    v_norm = len(active_nodes)
    e_norm = sum(len(s) for s in succs.values())
    cc = e_norm - v_norm + 2 if v_norm > 0 else 1
    return v_norm, e_norm, cc, decision_blocks


def norm_addr(a):
    clean = a.replace("0x", "").lower()
    return clean.zfill(8) if clean.isalnum() and len(clean) <= 8 else clean


def analyze_mips(vaddr_str, slus_data, ps2_sizes):
    vaddr = int(vaddr_str, 16)
    size = ps2_sizes.get(vaddr_str, 0)
    if size == 0:
        raise ValueError(f"Unknown PS2 function size for {vaddr_str}")

    seg_off = 0x100
    seg_vaddr = 0x00100000
    start_off = seg_off + (vaddr - seg_vaddr)
    inst_count = size // 4

    leaders = {vaddr}
    edges = set()
    callees = []
    offsets = set()

    insts = []
    for i in range(inst_count):
        va = vaddr + i * 4
        off = start_off + i * 4
        w = struct.unpack_from("<I", slus_data, off)[0]
        op = (w >> 26) & 0x3F
        rs = (w >> 21) & 0x1F
        rt = (w >> 16) & 0x1F
        imm = w & 0xFFFF
        simm = imm if imm < 0x8000 else imm - 0x10000
        funct = w & 0x3F
        insts.append((va, op, rs, rt, simm, funct, w))

    for i, (va, op, rs, rt, simm, funct, w) in enumerate(insts):
        # memory offsets
        if op in (
            0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
            0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x31,
            0x35, 0x37, 0x39, 0x3D, 0x3F,
        ):
            offsets.add(simm)

        # control flow
        if op == 3:  # jal
            target = (va & 0xF0000000) | ((w & 0x03FFFFFF) << 2)
            callees.append(f"{target:08x}")
        elif op == 0 and funct == 9:  # jalr
            callees.append(f"indirect_r{rs}")
        elif op == 2:  # j
            target = (va & 0xF0000000) | ((w & 0x03FFFFFF) << 2)
            if vaddr <= target < vaddr + size:
                leaders.add(target)
                edges.add((va, target))
            if i + 2 < inst_count:
                leaders.add(insts[i + 2][0])
        elif op in (4, 5, 6, 7, 1, 20, 21, 22, 23):  # branches
            target = va + 4 + (simm << 2)
            if vaddr <= target < vaddr + size:
                leaders.add(target)
                edges.add((va, target))
            if i + 2 < inst_count:
                next_va = insts[i + 2][0]
                leaders.add(next_va)
                edges.add((va, next_va))

    sorted_leaders = sorted(leaders)
    blocks = []
    for bi, lva in enumerate(sorted_leaders):
        end_va = (
            sorted_leaders[bi + 1] if bi + 1 < len(sorted_leaders) else vaddr + size
        )
        b_inst_count = (end_va - lva) // 4
        succs = [f"{t:08x}" for (f, t) in edges if lva <= f < end_va]
        blocks.append({
            "id": bi,
            "start": f"{lva:08x}",
            "end": f"{end_va:08x}",
            "instruction_count": b_inst_count,
            "successors": sorted(set(succs)),
        })

    return {
        "arch": "mips_r5900",
        "entry": vaddr_str,
        "size_bytes": size,
        "instruction_count": inst_count,
        "basic_block_count": len(blocks),
        "edge_count": len(edges),
        "callees": callees,
        "memory_offsets": sorted(offsets),
        "cfg_blocks": blocks,
    }


def analyze_x86(xb_addr_str, xbox_funcs):
    if xb_addr_str == "002ba610":
        c = xbox_funcs["002ba0b0"]
        t_start = 0x002BA610
        t_end = 0x002BA680
        insts = [
            i for i in c["instructions"] if t_start <= int(i["address"], 16) <= t_end
        ]
    else:
        c = xbox_funcs[xb_addr_str]
        insts = c["instructions"]

    entry_va = int(insts[0]["address"], 16)
    last_va = int(insts[-1]["address"], 16)

    leaders = {entry_va}
    edges = set()
    callees = []
    offsets = set()

    JUMP_MNEMS = {
        "JMP", "JZ", "JNZ", "JE", "JNE", "JG", "JGE", "JL", "JLE",
        "JA", "JAE", "JB", "JBE", "JS", "JNS", "JO", "JNO", "JP", "JNP",
    }

    for i, inst in enumerate(insts):
        va = int(inst["address"], 16)
        text = inst["instruction"]
        parts = text.split(None, 1)
        mnem = parts[0].upper()
        args = parts[1] if len(parts) > 1 else ""

        for m in re.finditer(r"\[([^\]]+)\]", args):
            bracket = m.group(1)
            disp_m = re.findall(
                r"([+-]?\s*0x[0-9a-fA-F]+|[+-]?\s*\b\d+\b)", bracket
            )
            for d in disp_m:
                d_clean = d.replace(" ", "")
                try:
                    val = int(d_clean, 0)
                    offsets.add(val)
                except ValueError:
                    pass

        if mnem == "CALL":
            callees.append(args.strip())

        if mnem in JUMP_MNEMS:
            tgt_m = re.search(r"0x([0-9a-fA-F]+)", args)
            if tgt_m:
                tgt = int(tgt_m.group(1), 16)
                if entry_va <= tgt <= last_va:
                    leaders.add(tgt)
                    edges.add((va, tgt))
            if mnem != "JMP":
                if i + 1 < len(insts):
                    next_va = int(insts[i + 1]["address"], 16)
                    leaders.add(next_va)
                    edges.add((va, next_va))
            else:
                if i + 1 < len(insts):
                    next_va = int(insts[i + 1]["address"], 16)
                    leaders.add(next_va)

        if mnem.startswith("RET"):
            if i + 1 < len(insts):
                next_va = int(insts[i + 1]["address"], 16)
                leaders.add(next_va)

    sorted_leaders = sorted(leaders)
    blocks = []
    for bi, lva in enumerate(sorted_leaders):
        end_va = sorted_leaders[bi + 1] if bi + 1 < len(sorted_leaders) else last_va + 1
        b_insts = [it for it in insts if lva <= int(it["address"], 16) < end_va]
        succs = [f"{t:08x}" for (f, t) in edges if lva <= f < end_va]
        blocks.append({
            "id": bi,
            "start": f"{lva:08x}",
            "end": f"{end_va:08x}",
            "instruction_count": len(b_insts),
            "successors": sorted(set(succs)),
        })

    last_inst_len = len(bytes.fromhex(insts[-1]["bytes_hex"]))
    size_bytes = (last_va + last_inst_len) - entry_va

    return {
        "arch": "x86_32",
        "entry": xb_addr_str,
        "size_bytes": size_bytes,
        "instruction_count": len(insts),
        "basic_block_count": len(blocks),
        "edge_count": len(edges),
        "callees": callees,
        "memory_offsets": sorted(offsets),
        "cfg_blocks": blocks,
    }


def compute_alignment(
    ps2_info,
    xb_info,
    is_candidate=None,
    default_confidence=None,
    notes="",
    is_composite=False,
    ps2_to_xb=None,
):
    if ps2_to_xb is None:
        ps2_to_xb = {}

    ps2_entry = norm_addr(ps2_info.get("entry", ""))
    xb_entry = norm_addr(xb_info.get("entry", ""))

    # Evaluate whether pair is an intentional candidate mapping from the cross-platform ledger
    if is_candidate is None:
        mapped_xb = ps2_to_xb.get(ps2_entry)
        if isinstance(mapped_xb, list):
            is_candidate = xb_entry in [norm_addr(x) for x in mapped_xb]
        elif isinstance(mapped_xb, str):
            is_candidate = (norm_addr(mapped_xb) == xb_entry)
        else:
            is_candidate = False

    # 1. Normalized CFG comparison
    ps2_v, ps2_e, ps2_cc, ps2_d = canonicalize_cfg(ps2_info["cfg_blocks"])
    xb_v, xb_e, xb_cc, xb_d = canonicalize_cfg(xb_info["cfg_blocks"])
    cfg_isomorphic = (
        ps2_v == xb_v and ps2_e == xb_e and ps2_cc == xb_cc and ps2_d == xb_d
    )

    # Non-vacuous CFG requires multi-block structure and at least one decision block
    non_vacuous_cfg = (
        cfg_isomorphic
        and ps2_v > 1
        and ps2_d > 0
        and ps2_info["basic_block_count"] > 1
        and xb_info["basic_block_count"] > 1
    )

    # 2. Memory offsets (disregarded for classification per measurement rule)
    ps2_nonstack = {o for o in ps2_info["memory_offsets"] if o >= 0}
    xb_nonstack = {o for o in xb_info["memory_offsets"] if o >= 0}
    shared_offsets = sorted(ps2_nonstack.intersection(xb_nonstack))
    nontrivial_offsets = [o for o in shared_offsets if o > 64]

    # 3. Callees compared through address translation map
    ps2_c = [norm_addr(c) for c in ps2_info.get("callees", [])]
    xb_c = [norm_addr(c) for c in xb_info.get("callees", [])]
    mapped_ps2_c = [norm_addr(ps2_to_xb.get(c, c)) for c in ps2_c]
    common_callees = sorted(set(mapped_ps2_c).intersection(set(xb_c)))
    callees_exact = (
        len(ps2_c) == len(xb_c)
        and len(ps2_c) > 0
        and all(mapped_ps2_c[i] == xb_c[i] for i in range(len(ps2_c)))
    )

    # 4. Determine match confidence strictly from binary measurements:
    # - "exact": non-vacuous isomorphic CFGs AND mapped callees match position-by-position
    # - "structural": non-vacuous isomorphic CFGs AND verified mapped callees
    # - "mapped-by-role": registered candidate pair in cross-platform map pending dual-platform vectors
    # - "unrelated": non-candidate pair, negative control, or arbitrary pair without alignment
    if non_vacuous_cfg and callees_exact:
        confidence = "exact"
        evidence_type = "isomorphic_cfg_and_exact_callees"
    elif non_vacuous_cfg and len(common_callees) > 0:
        confidence = "structural"
        evidence_type = "isomorphic_cfg_and_common_mapped_callees"
    elif is_candidate and default_confidence != "unrelated":
        confidence = "mapped-by-role"
        evidence_type = (
            "subsystem_role_correspondence_pending_dual_platform_vectors"
        )
    else:
        confidence = "unrelated"
        evidence_type = "zero_structural_or_callee_alignment"

    return {
        "match_confidence": confidence,
        "evidence_type": evidence_type,
        "notes": notes,
        "shared_memory_offsets": shared_offsets,
        "nontrivial_offsets": nontrivial_offsets,
        "cfg_topology_match": cfg_isomorphic,
        "raw_ps2_block_count": ps2_info["basic_block_count"],
        "raw_xbox_block_count": xb_info["basic_block_count"],
        "raw_ps2_edge_count": ps2_info["edge_count"],
        "raw_xbox_edge_count": xb_info["edge_count"],
        "normalized_ps2": {
            "vertices": ps2_v,
            "edges": ps2_e,
            "cyclomatic_complexity": ps2_cc,
            "decision_blocks": ps2_d,
        },
        "normalized_xbox": {
            "vertices": xb_v,
            "edges": xb_e,
            "cyclomatic_complexity": xb_cc,
            "decision_blocks": xb_d,
        },
        "mapped_callee_match": callees_exact,
        "common_mapped_callees": common_callees,
    }


def main():
    slus_data, ps2_sizes = load_ps2_binary_and_sizes()
    xbox_funcs = load_xbox_data()
    ps2_to_xb = load_cross_map()

    sig_dir = REPO / "research" / "cross_platform" / "signatures"
    sig_dir.mkdir(parents=True, exist_ok=True)

    generated = []
    for ps2_addr, xb_addr, req_confidence, notes in ALL_PAIRS:
        is_composite = (req_confidence == "composite")
        ps2_info = analyze_mips(ps2_addr, slus_data, ps2_sizes)
        xb_info = analyze_x86(xb_addr, xbox_funcs)
        alignment = compute_alignment(
            ps2_info,
            xb_info,
            is_candidate=True,
            default_confidence=req_confidence,
            notes=notes,
            is_composite=is_composite,
            ps2_to_xb=ps2_to_xb,
        )
        computed_confidence = alignment["match_confidence"]

        sig_data = {
            "schema": "cross_isa_pair_signature_v1",
            "ps2_address": ps2_addr,
            "xbox_address": xb_addr,
            "match_confidence": computed_confidence,
            "ps2": ps2_info,
            "xbox": xb_info,
            "semantic_alignment": alignment,
        }

        filename = f"signature_{ps2_addr}_{xb_addr}.json"
        out_path = sig_dir / filename
        with out_path.open("w", encoding="utf-8", newline="\n") as f:
            json.dump(sig_data, f, indent=2)
            f.write("\n")
        generated.append(out_path)
        print(f"Generated {filename}: {computed_confidence} (evidence={alignment['evidence_type']})")

    print(f"\nSuccessfully generated {len(generated)} cross-ISA signature files.")


if __name__ == "__main__":
    main()
