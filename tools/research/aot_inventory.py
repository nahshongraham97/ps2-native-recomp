"""Measure callable catalogue entries in build-time generated AOT output.

Unit address ranges include untranslated words. Only a nonzero entry-map
element whose switch target is that exact address establishes static coverage.
This measures generation, not semantic equivalence or native transformation.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
from pathlib import Path
import re
import struct


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verified_nonfunction_catalogue_addresses(repo: Path, rows: list[dict]) -> set[int]:
    """Authenticate the specific descriptor-data contradiction without crediting code.

    Raw catalogue rows and denominators are retained. This exception can only
    explain an uncallable data address; it never creates a generated entry.
    """
    inventory = json.loads((repo / "research/ledger/aot_coverage_inventory.json").read_text(encoding="utf-8"))
    records = inventory.get("nonfunction_catalogue_records", [])
    if not records:
        return set()
    if len(records) != 1 or records[0].get("proof_id") != "SLUS-descriptor-005bf218-v1":
        raise AssertionError("Unknown catalogue data exception requires independent source audit")
    record = records[0]
    if record.get("module") != "SLUS_214.10" or record.get("address") != "005bf218":
        raise AssertionError("Catalogue descriptor address mismatch")
    source = inventory["sources"]["pinned_elf"]
    relative = Path(source["rel_path"])
    if relative.is_absolute() or ".." in relative.parts:
        raise AssertionError("Original ELF source path is not repository-relative")
    original = next((base / relative for base in [repo, *repo.parents] if (base / relative).is_file()), None)
    if original is None:
        raise AssertionError("Original ELF required to verify catalogue data exception")
    raw = original.read_bytes()
    if hashlib.sha256(raw).hexdigest() != source["sha256"] or source["sha256"] != record["elf_sha256"]:
        raise AssertionError("Catalogue data original ELF hash mismatch")
    if raw[:7] != b"\x7fELF\x01\x01\x01":
        raise AssertionError("Catalogue data proof requires original ELF32 little endian")
    phoff = struct.unpack_from("<I", raw, 28)[0]
    phsize, phcount = struct.unpack_from("<HH", raw, 42)
    segments = []
    for index in range(phcount):
        kind, offset, address, _, extent, _, _, _ = struct.unpack_from("<8I", raw, phoff + index * phsize)
        if kind == 1:
            segments.append((address, extent, offset))
    def word(address):
        for start, extent, offset in segments:
            if start <= address and address + 4 <= start + extent:
                return struct.unpack_from("<I", raw, offset + address - start)[0]
        raise AssertionError(f"Unmapped catalogue proof word: {address:08x}")
    for mapping in (record["descriptor_words"], record["producer_words"]):
        for address, expected in mapping.items():
            if word(int(address, 16)) != int(expected, 16):
                raise AssertionError("Catalogue data instruction/descriptor witness changed")
    # Positive original dataflow: form the node address in a1, link +0x1c,
    # load and call its +8 callback, then traverse the next-node field.
    required = {0x2966F0: 0x3C05005C, 0x29670C: 0x24A5F218,
                0x296744: 0xACA6001C, 0x296748: 0xAC45001C,
                0x296774: 0x8E220008, 0x296780: 0x0040F809,
                0x2967C0: 0x8E31001C}
    if any(word(address) != expected for address, expected in required.items()):
        raise AssertionError("Catalogue descriptor producer/callback dataflow mismatch")
    immediate = word(0x29670C) & 0xffff
    formed = ((word(0x2966F0) & 0xffff) << 16) + (immediate if immediate < 0x8000 else immediate - 0x10000)
    address = int(record["address"], 16)
    callback = word(address + 8)
    if formed != address or word(address) != 0x1c or callback != int(record["callback_address"], 16):
        raise AssertionError("Catalogue data node/callback identity mismatch")
    catalogue = {int(row["address"], 16) for row in rows if row["module"] == "SLUS_214.10"}
    if address not in catalogue or callback not in catalogue or callback == address:
        raise AssertionError("Catalogue descriptor/callback rows missing")
    return {address}


def verify_vu_snapshot_measurement(repo: Path, vu: dict) -> dict:
    evidence = vu.get("snapshot_verification")
    if not evidence:
        return {}
    root = repo / "external/forks/ps2-native-recomp"
    if not root.is_dir():
        root = repo
    receipt = root / "build/oracle-telemetry/phase4-fixed-verify.json"
    manifest = root / "build/oracle-telemetry/phase4-fixed-compiled-source.json"
    if not receipt.is_file() or sha(receipt) != evidence["receipt_sha256"]:
        raise AssertionError("VU snapshot execution receipt missing or changed")
    measured = json.loads(receipt.read_text(encoding="utf-8"))
    if not manifest.is_file() or sha(manifest) != measured["source_manifest_sha256"]:
        raise AssertionError("VU compiled-source manifest missing or changed")
    sources = json.loads(manifest.read_text(encoding="utf-8"))
    required_sources = ["recomp/vurecomp_gen.cpp", "tests/test_phase4_aot_equivalence.cpp",
                        "tests/phase4_vu_reference.hpp"]
    required_sources += [name for name in sources if name.startswith("build/generated-aot/vu1/")]
    for relative in required_sources:
        if relative not in sources or sha(root / relative) != sources[relative]:
            raise AssertionError("VU measured source changed: " + relative)
    if sha(root / "b/test_phase4_aot_equivalence.exe") != measured["binary_sha256"]:
        raise AssertionError("VU measured executable changed")
    checks = {item["name"]: item for item in measured["checks"]}
    if measured["status"] != "PASS" or any(item["status"] != "PASS" for item in checks.values()):
        raise AssertionError("VU snapshot comparison is not passing")
    if checks["states"]["exit_code"] != 0 or checks["planted-packet"]["exit_code"] != 1:
        raise AssertionError("VU positive/negative comparison outcomes changed")
    summary = checks["states"]["tail"][-1]
    fields = {name: int(value) for name, value in re.findall(r"(\w+)=(\d+)", summary)}
    required = ("oracle_states", "vu1_independent_snapshot_replays", "gamer_complete_leaf_references")
    if not fields.get("oracle_states") or len({fields.get(key) for key in required}) != 1 or not fields.get("vu1_packet_bytes"):
        raise AssertionError("VU/EE state comparison counts are incomplete")
    if "exact ordered GIF path and output packet bytes" not in "\n".join(checks["planted-packet"]["tail"]):
        raise AssertionError("VU packet corruption did not reach the intended rejection")
    return {"states": fields["oracle_states"], "packet_bytes": fields["vu1_packet_bytes"],
            "gamer_boundaries": fields["gamer_complete_leaf_references"],
            "scope": "Independent snapshot-seeded replay; historical invocation and general hardware parity unverified"}


def classify_vu_inputs(actual: dict, legacy: dict, rejected: dict) -> tuple[dict, dict]:
    """Admit measured legacy bytes, never arbitrary files found in a directory."""
    if set(legacy) & set(rejected):
        raise AssertionError("VU capture cannot be both admitted and rejected")
    unknown = set(actual) - set(legacy) - set(rejected)
    missing = (set(legacy) | set(rejected)) - set(actual)
    if unknown or missing:
        raise AssertionError(f"Unknown/missing VU capture inputs: unknown={sorted(unknown)}, missing={sorted(missing)}")
    for name, expected in {**legacy, **rejected}.items():
        if not re.fullmatch(r"[0-9A-Fa-f]{16}_[0-9A-Fa-f]{4}\.bin", name):
            raise AssertionError(f"Noncanonical VU capture name: {name}")
        if actual[name] != {"bytes": expected["bytes"], "sha256": expected["sha256"]}:
            raise AssertionError(f"VU capture source hash/extent mismatch: {name}")
    return ({name: actual[name] for name in sorted(legacy)},
            {name: actual[name] for name in sorted(rejected)})


def stage_vu_inputs(repo: Path, fork: Path, destination: Path) -> dict:
    inventory = json.loads((repo / "research/ledger/aot_coverage_inventory.json").read_text(encoding="utf-8"))
    source = inventory["sources"]["vu_corpus_strict"]
    receipt = repo / source["rel_path"]
    if sha(receipt) != source["sha256"]:
        raise AssertionError("Measured legacy VU capture receipt changed")
    legacy = {}
    for path, item in json.loads(receipt.read_text(encoding="utf-8"))["captures"].items():
        name = Path(path).name
        if name in legacy:
            raise AssertionError(f"Duplicate legacy VU capture basename: {name}")
        legacy[name] = item
    rejected = inventory["vu_microprograms"]["rejected_captures"]
    original = repo / "private-inputs/mkarmageddon-aot-inputs/vu1"
    contents = {path.name: path.read_bytes() for path in original.glob("*.bin")}
    actual = {name: {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}
              for name, raw in contents.items()}
    admitted, excluded = classify_vu_inputs(actual, legacy, rejected)
    target = destination.resolve()
    if not target.is_relative_to(fork.resolve()) or target == fork.resolve():
        raise AssertionError("VU capture staging escaped the active checkout")
    # Validate every original before creating an output. Protected inputs are
    # never renamed, overwritten or deleted, including the mis-extraction.
    target.mkdir(parents=True, exist_ok=False)
    for name in admitted:
        with (target / name).open("xb") as stream:
            stream.write(contents[name])
    return {"legacy_receipt_sha256": source["sha256"], "admitted": admitted,
            "excluded": excluded, "original_inputs_modified": False}


def measure_entries(output: Path, rows: list[dict]) -> dict:
    table_path = output / "table.cpp"
    table = table_path.read_text(encoding="utf-8")
    module_bases = {"kUnits": ("SLUS_214.10", 0)}
    declared_modules = re.findall(
        r'\{"([^"]+)",\s*0x([0-9a-fA-F]+)u,\s*0x([0-9a-fA-F]+)u,\s*\w+,\s*\d+u,\s*(\w+),\s*(\d+)u\}', table)
    dynamic_groups = {}
    for name, base, span, units, count in declared_modules:
        module_bases[units] = (name + ".ovl", int(base, 16))
        if re.fullmatch(r"[0-9a-fA-F]{8}_[0-9a-fA-F]{8}", name):
            dynamic_groups[units] = {"name": name, "base_addr": int(base, 16),
                                     "span_bytes": int(span, 16), "declared_units": int(count)}
    modules = {}
    sources = {}
    total_units = 0
    dynamic_windows = []
    for group, body in re.findall(r"const ps2::UnitInfo (\w+)\[\] = \{(.*?)\};", table, re.S):
        if group not in module_bases:
            raise AssertionError("Unregistered generated module unit group: " + group)
        entries = re.findall(r"\{0x([0-9a-fA-F]+)u,\s*(\d+)u,\s*&(\w+),\s*(\w+)\}", body)
        total_units += len(entries)
        module, fixed_base = module_bases.get(group, ("", 0))
        catalogue = {int(r["address"], 16) for r in rows if r["module"] == module}
        translated = set()
        ranges = []
        for base, size, function, entry_name in entries:
            source = output / (function + ".cpp")
            text = source.read_text(encoding="utf-8")
            sources[source.name] = sha(source)
            start, size = fixed_base + int(base, 16), int(size)
            array = re.search(r"const std::uint16_t " + re.escape(entry_name) + r"\[(\d+)\] = \{(.*?)\};", text, re.S)
            if array is None:
                raise AssertionError(f"Missing static entry map: {source.name}:{entry_name}")
            values = [int(v.strip().removesuffix("u")) for v in array[2].split(",") if v.strip()]
            if len(values) != int(array[1]) or len(values) * 4 != size:
                raise AssertionError(f"Static entry map extent mismatch: {source.name}")
            targets = {int(i): int(pc, 16) for i, pc in re.findall(r"case (\d+)u: goto L_([0-9a-fA-F]+);", text)}
            for offset, entry in enumerate(values):
                pc = start + offset * 4
                if entry:
                    if targets.get(entry) != pc:
                        raise AssertionError(f"Static entry target mismatch: {source.name}:{pc:08x}")
                    translated.add(pc)
            ranges.append({"start": start, "size": size})
        if group in dynamic_groups:
            item = dynamic_groups[group]
            if item["declared_units"] != len(entries):
                raise AssertionError("Dynamic window declared/measured unit count mismatch")
            dynamic_windows.append({key: item[key] for key in ("name", "base_addr", "span_bytes")} |
                                   {"unit_count": len(entries), "callable_entries": len(translated)})
        if catalogue:
            covered = catalogue & translated
            modules[module] = {
                "callable_catalogue_entries": [f"{pc:08x}" for pc in sorted(covered)],
                "missing_catalogue_entries": [f"{pc:08x}" for pc in sorted(catalogue - covered)],
                "unit_ranges": ranges,
            }
    for required in ("SLUS_214.10", "MOVIER.ovl", "GAMER.ovl"):
        if required not in modules:
            raise AssertionError(f"Missing generated static module: {required}")
    return {"modules": modules, "table_sha256": sha(table_path),
            "source_sha256": sources, "total_units": total_units,
            "dynamic_windows": sorted(dynamic_windows, key=lambda item: item["name"])}


def verify_entries(repo: Path, inventory: dict, rows: list[dict]) -> None:
    root = repo / "external/forks/ps2-native-recomp"
    if not root.is_dir():
        root = repo
    output = root / "build/generated-aot/phase4-all"
    if not (output / "table.cpp").is_file():
        raise AssertionError("Measured static AOT output missing; reproduce the pipeline before claiming coverage")
    actual = measure_entries(output, rows)
    expected_windows = {(item["name"], int(item["base_addr"], 16), item["span_bytes"])
                        for item in inventory["dynamic_overlay_windows_kModules"]}
    actual_windows = {(item["name"], item["base_addr"], item["span_bytes"])
                      for item in actual["dynamic_windows"]}
    if expected_windows != actual_windows or any(not item["unit_count"] or not item["callable_entries"]
                                                 for item in actual["dynamic_windows"]):
        raise AssertionError("Dynamic window input/generated module inventory mismatch")
    recorded = inventory.get("entry_measurement", {})
    for key in ("table_sha256", "source_sha256", "total_units", "dynamic_windows"):
        if recorded.get(key) != actual[key]:
            raise AssertionError(f"Static AOT measurement mismatch: {key}")
    for module, measured in actual["modules"].items():
        for key, value in measured.items():
            if inventory["modules"][module].get(key) != value:
                raise AssertionError(f"Static AOT callable entry mismatch: {module}:{key}")


def update_inventory(repo: Path, output: Path) -> dict:
    rows = list(csv.DictReader((repo / "research/ledger/functions.csv").open(encoding="utf-8", newline="")))
    measured = measure_entries(output, rows)
    path = repo / "research/ledger/aot_coverage_inventory.json"
    inventory = json.loads(path.read_text(encoding="utf-8"))
    inventory["entry_measurement"] = {
        "method": "Nonzero generated entry-map element and exact switch target; never unit-range membership",
        **{key: measured[key] for key in ("table_sha256", "source_sha256", "total_units", "dynamic_windows")},
        "catalogue_sha256": sha(repo / "research/ledger/functions.csv"),
    }
    for module, result in measured["modules"].items():
        item = inventory["modules"][module]
        item.update(result)
        item["static_aot_count"] = len(result["callable_catalogue_entries"])
        covered = set(result["callable_catalogue_entries"])
        item["in_scope_static_aot_count"] = sum(
            r["address"] in covered and r["status"] != "out-of-scope-stubbed"
            for r in rows if r["module"] == module
        )
        item["static_units"] = len(result["unit_ranges"])
        item["note"] = "Callable catalogue coverage only; semantic equivalence and native transformation are separate OPEN gates."
        if module != "SLUS_214.10":
            prefix = "m_" + module.removesuffix(".ovl") + "_unit_"
            item["static_overlay_sources"] = ["build/generated-aot/phase4-all/" + n
                                               for n in sorted(measured["source_sha256"]) if n.startswith(prefix)]
    inventory["active_fork_build_generated_units"] = measured["total_units"]
    inventory["active_fork_status"] = "Static generation measured by actual callable entries and dynamic windows; independent execution evidence and memory/ABI transformation are recorded separately."
    reproduced = inventory["sources"]["reproduced_table_cpp"]
    reproduced["sha256"] = measured["table_sha256"]
    reproduced["static_unit_count"] = measured["total_units"]
    for name, field in [("SLUS_214.10", "slus_unit_count"), ("MOVIER.ovl", "movier_unit_count"), ("GAMER.ovl", "gamer_unit_count")]:
        reproduced[field] = inventory["modules"][name]["static_units"]
    reproduced["dynamic_window_unit_count"] = measured["total_units"] - sum(len(m["unit_ranges"]) for m in measured["modules"].values())
    seed = output.parent / "catalogue_entries.txt"
    if seed.exists():
        seed_source = inventory["sources"].get("catalogue_entry_seeds", {})
        seed_source.update({"rel_path": "build/generated-aot/catalogue_entries.txt", "sha256": sha(seed)})
        inventory["sources"]["catalogue_entry_seeds"] = seed_source
    path.write_text(json.dumps(inventory, indent=2) + "\n", encoding="utf-8", newline="\n")
    return measured


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = update_inventory(args.repo, args.output)
    print(json.dumps({name: {"callable": len(value["callable_catalogue_entries"]),
                            "missing": len(value["missing_catalogue_entries"])}
                      for name, value in result["modules"].items()}))
