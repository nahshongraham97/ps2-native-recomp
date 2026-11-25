"""Offline, conservative engine-family census; not a RenderWare symbol classifier.

Original core callbacks, diagnostic-named plugin roots and their registry callbacks,
and individually authenticated PS2 helper bodies are admitted. No range,
SDK status, Xbox address, conventional API name, or native filename admits code.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import re
import struct
import subprocess

from aot_inventory import measure_entries, verified_nonfunction_catalogue_addresses

ELF_SHA = "a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f"
RECORDS = (
    "parallel-renderer-engine-init-20261122",
    "parallel-renderer-plugin-0401-20261122",
    "parallel-renderer-plugin-0402-20261122",
)


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def signed_immediate(word):
    immediate = word & 0xffff
    return immediate - 0x10000 if immediate & 0x8000 else immediate


def registry_calls(original, begin, end):
    """Recover a0..a3 across an authenticated straight-line registration prefix."""
    registers: list[int | None] = [None] * 32
    registers[0] = 0

    def signed_word(value: int) -> int:
        value &= 0xFFFFFFFF
        return (value if value < 0x80000000 else value - 0x100000000) & 0xFFFFFFFFFFFFFFFF

    registrations = []
    for address in range(begin, end, 4):
        word = struct.unpack("<I", original(address, 4))[0]
        op, rs, rt, imm = word >> 26, (word >> 21) & 31, (word >> 16) & 31, word & 0xFFFF
        if op == 0 and (word & 63) == 8 and (word != 0x03e00008 or address + 8 != end):
            raise AssertionError(f"Early indirect exit in registration prefix at {address:08x}")
        if op == 15:
            registers[rt] = signed_word(imm << 16)
        elif op == 9:
            registers[rt] = (None if registers[rs] is None else signed_word(
                registers[rs] + signed_immediate(word)))
        elif op == 0x0d:  # ORI zero-extends its immediate.
            registers[rt] = None if registers[rs] is None else registers[rs] | imm
        elif word == 0:  # NOP
            pass
        elif op == 0 and (word & 63) in (0x25, 0x2D):
            rd = (word >> 11) & 31
            if (word & 63) == 0x2D:
                # Only the source-proved move-with-zero DADDU subset is needed.
                # Do not pretend bitwise OR implements unrestricted addition.
                if rs != 0 and rt != 0:
                    raise AssertionError(f"DADDU is not a verified move at {address:08x}")
                registers[rd] = registers[rt if rs == 0 else rs]
            else:
                registers[rd] = (None if registers[rs] is None or registers[rt] is None
                                 else registers[rs] | registers[rt])
        else:
            # Exhaust the OTHER instruction forms actually present in this
            # pinned straight-line span. Any write to a0..a3 must be handled
            # above; no stale value may silently survive an unknown writer.
            if op == 0 and (word & 63) == 0x2A:  # SLT rd
                destination = (word >> 11) & 31
            elif op in (0x0E, 0x1E, 0x37):  # XORI / LQ / LD rt
                destination = rt
            elif op in (3, 0x1F, 0x2B, 0x3F) or (op == 0 and (word & 63) == 8):
                destination = None  # JAL, SQ, SW, SD, JR: no argument write
            else:
                raise AssertionError(f"Unhandled registration instruction at {address:08x}: {word:08x}")
            if destination in (4, 5, 6, 7):
                raise AssertionError(f"Unhandled argument writer at {address:08x}")
            if destination is not None:
                registers[destination] = None
        registers[0] = 0  # Architectural zero is never writable.
        if address > begin:
            previous = struct.unpack("<I", original(address - 4, 4))[0]
            if previous >> 26 == 3 and (previous & 0x3FFFFFF) << 2 == 0x1A6A20:
                if any(registers[index] is None for index in (4, 5, 6, 7)):
                    raise AssertionError(f"Registration argument not freshly established at {address:08x}")
                row = {"plugin_id": f"{registers[5]:04x}", "source_extent": registers[4],
                       "constructor": f"{registers[6]:08x}", "destructor": f"{registers[7]:08x}",
                       "call_address": f"{address - 4:08x}", "delay_word_le": original(address, 4).hex()}
                registrations.append(row)
            if previous >> 26 == 3:
                # After EVERY call's executed delay slot, discard all tracked
                # nonzero registers. Later admissions must establish each
                # argument afresh; no callee-preservation assumption is needed.
                registers = [0] + [None] * 31
    return registrations


def plugin_attach_calls(original):
    """Pair the tested JAL result with the diagnostic on its zero-result path."""
    begin, exit_pc = 0x126420, 0x126700

    def target(word, pc):
        return ((pc + 4) & 0xf0000000) | ((word & 0x03ffffff) << 2)

    rows = []
    for pc in range(begin, exit_pc - 24, 4):
        call, delay, branch, branch_delay, hi, log, lo = struct.unpack("<7I", original(pc, 28))
        if call >> 26 != 3 or branch >> 16 not in (0x1440, 0x5440):
            continue
        # Only these call delay slots occur: NOP or li a0,4 for PDS.
        if delay not in (0, 0x24040004) or hi >> 16 != 0x3c04 or lo >> 16 != 0x2484:
            raise AssertionError(f"Unexpected plugin return/diagnostic arguments at {pc:08x}")
        if log >> 26 != 3 or target(log, pc + 20) != 0x163cb0:
            raise AssertionError(f"Missing original diagnostic call at {pc:08x}")
        taken = pc + 12 + 4 * signed_immediate(branch)
        if branch >> 26 == 5:  # BNE: success skips logging and the failure return.
            jump, zero = struct.unpack("<2I", original(pc + 28, 8))
            jump_disp = signed_immediate(jump)
            if (branch_delay != 0 or taken != pc + 36 or jump >> 16 != 0x1000
                    or pc + 32 + 4 * jump_disp != exit_pc or zero != 0x0000102d):
                raise AssertionError(f"Plugin failure return changed at {pc:08x}")
        else:  # BNEL: its delay slot sets success=1 only on the taken edge.
            if (branch_delay != 0x24020001 or taken != exit_pc or pc + 32 != exit_pc
                    or original(pc + 28, 4) != struct.pack("<I", 0x0000102d)):
                raise AssertionError(f"Plugin final return changed at {pc:08x}")
        data = ((hi & 0xffff) << 16) + signed_immediate(lo)
        diagnostic = original(data, 64).split(b"\0", 1)[0]
        m = re.fullmatch(rb"([A-Za-z0-9_]+) failed\.", diagnostic)
        if not m:
            raise AssertionError(f"Unexpected diagnostic at {data:08x}")
        rows.append({"name": m[1].decode("ascii"), "target": f"{target(call, pc):08x}",
                     "call_site": f"{pc:08x}", "success_target": f"{taken:08x}",
                     "diagnostic_address": f"{data:08x}", "diagnostic": diagnostic.decode("ascii")})
    if len({row["name"] for row in rows}) != len(rows) or len(rows) != 14:
        raise AssertionError("Original plugin diagnostic chain incomplete or duplicated")
    return rows


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--parent-root", type=Path, required=True)
    parser.add_argument("--fork-root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--require-exact-renderware", action="store_true",
                        help="Write bounded evidence, then refuse if exact library ownership remains unknown")
    args = parser.parse_args()
    parent, fork = args.parent_root.resolve(), args.fork_root.resolve()
    output = args.output.resolve()
    telemetry = (fork / "build/oracle-telemetry").resolve()
    if not output.is_relative_to(telemetry) or output.exists():
        raise AssertionError("Output must be a NEW file inside fork build/oracle-telemetry")
    pins: dict[str, dict] = {}

    def pin(path: Path) -> bytes:
        data = path.read_bytes()
        key = ("fork/" + path.relative_to(fork).as_posix() if path.is_relative_to(fork)
               else "parent/" + path.relative_to(parent).as_posix())
        pins[key] = {"bytes": len(data), "sha256": digest(data)}
        return data

    pin(Path(__file__).resolve())
    pin(fork / "tools/research/aot_inventory.py")
    ledger = fork / "research/ledger/functions.csv"
    rows = list(csv.DictReader(pin(ledger).decode("utf-8-sig").splitlines()))
    inventory = json.loads(pin(fork / "research/ledger/aot_coverage_inventory.json"))
    pin(fork / "research/ledger/function_classification_inventory.json")
    pin(fork / "research/ledger/native_module_classification.json")
    raw_catalogue = parent / "research/runs/ghidra-function-coverage-20261118/export-20261118-230132-1875-18160/functions.tsv"
    raw_rows = list(csv.DictReader(pin(raw_catalogue).decode("utf-8-sig").splitlines(), delimiter="\t"))
    raw_starts = {int(row["entry"], 16) for row in raw_rows}
    base_starts = {int(row["address"], 16) for row in rows if row["module"] == "SLUS_214.10"}
    if raw_starts != base_starts:
        raise AssertionError("Raw PS2 Ghidra catalogue and ledger disagree")
    scoped = {(row["module"], int(row["address"], 16)) for row in rows
              if row["module"] != "NETR.ovl" and row["status"] != "out-of-scope-stubbed"}
    if len(scoped) != 15670:
        raise AssertionError(f"Unexpected raw in-scope baseline: {len(scoped)}")
    elf_path = parent / inventory["sources"]["pinned_elf"]["rel_path"]
    elf = pin(elf_path)
    if digest(elf) != ELF_SHA or elf[:7] != b"\x7fELF\x01\x01\x01":
        raise AssertionError("Original PS2 ELF identity changed")
    identity_strings = [{"file_offset": match.start(), "ascii": match[0].decode("ascii")}
                        for match in re.finditer(rb"[ -~]{6,}", elf)
                        if any(token in match[0].lower() for token in
                               (b"renderware", b"criterion", b"rwcore", b"rpworld"))]
    phoff = struct.unpack_from("<I", elf, 28)[0]
    phsize, phcount = struct.unpack_from("<HH", elf, 42)
    segments = [struct.unpack_from("<8I", elf, phoff + i * phsize) for i in range(phcount)]

    def original(address: int, size: int) -> bytes:
        for kind, offset, start, _, extent, _, _, _ in segments:
            if kind == 1 and start <= address and address + size <= start + extent:
                return elf[offset + address - start:offset + address - start + size]
        raise AssertionError(f"Unmapped original range {address:08x}+{size}")

    candidates: dict[int, dict] = {}
    outside_catalogue: dict[int, dict] = {}

    def admit(address: int, reason: dict) -> None:
        destination = candidates if ("SLUS_214.10", address) in scoped else outside_catalogue
        destination.setdefault(address, {"module": "SLUS_214.10", "address": f"{address:08x}",
                                       "entry_word_le": original(address, 4).hex(), "reasons": []})["reasons"].append(reason)

    for record in RECORDS:
        root = fork / "research/runs" / record
        joined = json.loads(pin(root / "SOURCE_JOIN.json"))
        pin(root / "WORK.md")
        if ELF_SHA not in (joined.get("image_sha256"), joined.get("ELF_sha256"), joined.get("elf_sha256")):
            raise AssertionError("Source join does not identify original PS2 ELF")
        functions = joined.get("full_listings", joined.get("functions", {}))
        for relative, item in functions.items():
            address = int(item["begin"], 16)
            # 127260 is a retail bootstrap caller, not an engine identity.
            if address == 0x127260:
                continue
            path = fork / relative if relative.startswith("research/") else root / relative
            data = pin(path)
            hashes = {digest(data), digest(data.replace(b"\r\n", b"\n"))}
            if not hashes.intersection({item.get("sha256"), item.get("lf_sha256"), item.get("committed_blob_sha256")}):
                raise AssertionError(f"Source listing identity changed: {path}")
            listed = 0
            for line in data.decode("utf-8-sig").splitlines():
                match = re.match(r"^([0-9a-fA-F]{8})\s+([0-9a-fA-F]{8})\s", line)
                if match:
                    at, word = int(match[1], 16), bytes.fromhex(match[2])
                    if original(at, 4) != word:
                        raise AssertionError(f"Original/listing byte mismatch: {at:08x}")
                    listed += 1
            end = int(item["end_exclusive"], 16)
            extent = original(address, end - address)
            mapping = item.get("mapping", {})
            if mapping.get("bytes") and bytes.fromhex(mapping["bytes"]) != extent:
                raise AssertionError("Complete source body mapping mismatch")
            if item.get("bytes_sha256") and digest(extent) != item["bytes_sha256"]:
                raise AssertionError("Complete source body hash mismatch")
            if not listed:
                raise AssertionError("No independently original-checked source words")
            admit(address, {"kind": "individually_source_joined_engine_helper", "record": record,
                            "listing": relative, "end_exclusive": f"{end:08x}",
                            "listed_original_words_checked": listed, "body_sha256": digest(extent)})

    def admit_callbacks(registrations, owner):
        for row in registrations:
            for role in ("constructor", "destructor"):
                admit(int(row[role], 16), {"kind": "original_registration_callback", "role": role,
                                          "owner": owner, **row})

    registrations = registry_calls(original, 0x1A66A0, 0x1A6890)
    admit_callbacks(registrations, "core")
    expected_ids = ["040f", "0401", "040d", "0402", "0403", "0404", "0405", "0406", "0407", "0408", "0409", "0412", "040a", "040b"]
    if [row["plugin_id"] for row in registrations] != expected_ids:
        raise AssertionError("Original registration argument decode changed")
    # The game also reports names for locally supplied extensions. Retain all
    # diagnostic joins, but count only these cross-checked engine-family roots.
    # Each prefix stops immediately after the last selected registry delay slot.
    # No transitive call graph, adjacent addresses or vendor ownership inferred.
    families = {
        "RpPDSAttachPlugin": (0x30e5f4, (0x131,)),
        "RpWorldPluginAttach": (0x1cdcec, (0x501, 0x502, 0x503, 0x504, 0x505, 0x50a, 0x507, 0x50b)),
        "RpSkinPluginAttach": (0x21a654, (0x116,)),
        "RpHAnimPluginAttach": (0x21b664, (0x11e,)),
        "RpMatFXPluginAttach": (0x1d7014, (0x120,)),
        "RpSpecularPluginAttach": (0x2dfd94, (0xff,)),
    }
    attach_rows = plugin_attach_calls(original)
    if not families.keys() <= {row["name"] for row in attach_rows}:
        raise AssertionError("Original named renderer families missing")
    spans = {0x126420: 0x12670c}
    for row in attach_rows:
        name = row["name"]
        if name not in families:
            continue
        start = int(row["target"], 16)
        end, ids = families[name]
        if not start < end:
            raise AssertionError(f"Invalid {name} registration prefix")
        spans[start] = end
        calls = registry_calls(original, start, end)
        if tuple(int(c["plugin_id"], 16) for c in calls) != ids:
            raise AssertionError(f"Registry identity mismatch for {name}")
        admit(start, {"kind": "diagnostic_and_core_registry_join", **row})
        admit_callbacks(calls, name)
        row["core_registrations"] = calls
        row["registration_prefix_sha256"] = digest(original(start, end - start))
        row["registration_prefix_end_exclusive"] = f"{end:08x}"
    # Every instruction in each admitted prefix and the complete diagnostic caller
    # must belong to that function in the EE listing and match its PT_LOAD word.
    listing_path = raw_catalogue.with_name("instructions.tsv")
    listing = io.BytesIO(pin(listing_path))
    next(listing)  # TSV header
    pending = {pc: start for start, end in spans.items() for pc in range(start, end, 4)}
    listing_words = len(pending)
    for line in listing:
        address, data, _, _, owner, _ = line.split(b"\t")
        pc = int(address, 16)
        if pc in pending:
            start = pending.pop(pc)
            if int(owner, 16) != start or bytes.fromhex(data.decode("ascii")) != original(pc, 4):
                raise AssertionError(f"EE listing disagrees with plugin evidence at {pc:08x}")
    if pending:
        raise AssertionError(f"Missing EE plugin listing word at {min(pending):08x}")
    consulted = []
    for path in sorted((fork / "research/runs").glob("native-xbox-renderer-*/IMPLEMENTATION.md")):
        pin(path)
        consulted.append(path.relative_to(fork).as_posix())
    # These records corroborate architecture/module meanings; Xbox addresses and
    # conventional Rp/Rw names NEVER enter the candidate set.
    # Query independently proved non-catalogue starts too. These temporary query
    # rows are NEVER written into the ledger or added to the raw denominator.
    query_rows = rows + [{"module": "SLUS_214.10", "address": f"{address:08x}"}
                         for address in outside_catalogue if address not in base_starts]
    measured = measure_entries(fork / "build/generated-aot/phase4-all", query_rows)
    covered = {int(address, 16) for address in measured["modules"]["SLUS_214.10"]["callable_catalogue_entries"]}
    all_callable = {(module, int(address, 16)) for module, item in measured["modules"].items()
                    for address in item["callable_catalogue_entries"]}
    proved_data = {("SLUS_214.10", pc) for pc in verified_nonfunction_catalogue_addresses(fork, rows)}
    if scoped - all_callable != proved_data:
        raise AssertionError("In-scope AOT omissions do not equal the authenticated descriptor data")
    for address, item in candidates.items():
        item["callable_aot"] = address in covered
    for address, item in outside_catalogue.items():
        item["callable_aot"] = address in covered
    missing = [f"{address:08x}" for address in sorted(candidates) if address not in covered]
    missing_outside = [f"{address:08x}" for address in sorted(outside_catalogue) if address not in covered]
    callable_scope = scoped - proved_data
    unclassified = sorted(
        ({"module": row["module"], "address": f"{int(row['address'], 16):08x}", "symbol": row["symbol"]}
         for row in rows if (row["module"], int(row["address"], 16)) in callable_scope
         and not (row["module"] == "SLUS_214.10" and int(row["address"], 16) in candidates)),
        key=lambda row: (row["module"], row["address"]))
    if len(unclassified) + len(candidates) + len(proved_data) != len(scoped):
        raise AssertionError("Engine-family membership/complement does not partition raw scope")
    exact_refusal = args.require_exact_renderware
    report = {
        "schema": "renderer_scope_lower_bound_v2", "script_version": 2,
        "result": ("FAIL_NAMED_ENGINE_CALLABLES_MISSING" if missing or missing_outside else
                   "REFUSE_EXACT_RENDERWARE_TOTAL_UNKNOWN" if exact_refusal else
                   "PASS_BOUNDED_ENGINE_FAMILY_CENSUS_TOTAL_OPEN"),
        "require_exact_renderware": args.require_exact_renderware,
        "fork_head": subprocess.check_output(["git", "-C", str(fork), "rev-parse", "HEAD"], text=True).strip(),
        "python_version": __import__("sys").version,
        "raw_in_scope_catalogue_entries": len(scoped),
        "positively_identified_engine_family_entries": len(candidates),
        "engine_family_actual_callable_aot_entries": len(candidates) - len(missing),
        "missing_engine_callables": missing,
        "missing_source_proved_callables_outside_raw_catalogue": missing_outside,
        "additional_source_proved_starts_outside_raw_catalogue": len(outside_catalogue),
        "remaining_unclassified_for_engine_membership": len(unclassified),
        "ownership_unclassified_entries": unclassified,
        "exact_renderware_symbol_ownership_count": None,
        "exact_renderware_total": None,
        "identity_limit": "RenderWare product attribution is an inference from existing cross-version engine/module evidence. Registration-family membership and exact PS2 bodies are proved separately; conventional Rw/Rp names are not recovered symbols.",
        "unknown_total_reason": "No authenticated complete link map, library object boundary or original-symbol/signature attribution covers the remaining catalogue. Contiguous code proximity, SDK status, and native source filenames cannot close this gap.",
        "original_identity_strings_not_function_boundary_proof": identity_strings,
        "heuristics_not_used_for_admission": {
            "in_scope_sdk_or_library_label_rows": sum(row["status"] == "sdk-or-library" and
                (row["module"], int(row["address"], 16)) in scoped for row in rows),
            "raw_ps2_Rw_Rp_Rx_prefixed_names": sum(bool(re.match(r"^(?:Rw|Rp|Rx)", row["name"])) for row in raw_rows),
            "reason": "Mixed SDK/library status and convention names are not an authenticated RenderWare ownership partition.",
        },
        "not_claimed": ["entire library ownership", "whole-address-range attribution", "native transformation", "semantic equivalence", "game reachability or acceptance"],
        "registration_original_span": {"start": "001a66a0", "end_exclusive": "001a6890", "sha256": digest(original(0x1A66A0, 0x1F0))},
        "catalogue_aot_partition": {"callable_in_scope": len(scoped & all_callable),
                                    "proved_data_exceptions": [{"module": m, "address": f"{pc:08x}"} for m, pc in sorted(proved_data)],
                                    "unexplained_missing_entries": len(scoped - all_callable - proved_data)},
        "diagnostic_source_join_words": listing_words,
        "diagnostic_attach_calls": attach_rows,
        "diagnostic_caller_sha256": digest(original(0x126420, 0x12670c - 0x126420)),
        "registrations": registrations, "entries": [candidates[a] for a in sorted(candidates)],
        "registration_extraction_contract": "Offline pinned straight-line extraction only: word-sign-extending LUI/ADDIU, OR, zero-move-only DADDU; reject unhandled argument writers; discard all nonzero tracked registers after every JAL delay slot and require fresh known a0..a3 before each registration.",
        "source_proved_starts_outside_in_scope_raw_catalogue_not_counted": [outside_catalogue[a] for a in sorted(outside_catalogue)],
        "consulted_cross_version_records_not_address_admission": consulted,
        "aot": {"table_sha256": measured["table_sha256"], "total_units": measured["total_units"],
                "all_measured_source_sha256": measured["source_sha256"]},
        "pins": pins,
    }
    output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps({key: report[key] for key in ("result", "raw_in_scope_catalogue_entries", "positively_identified_engine_family_entries", "engine_family_actual_callable_aot_entries", "remaining_unclassified_for_engine_membership", "missing_engine_callables")} | {"receipt": str(output), "sha256": digest(output.read_bytes())}))
    if missing or missing_outside:
        raise SystemExit(1)
    if exact_refusal:
        raise SystemExit(2)


if __name__ == "__main__":
    main()
