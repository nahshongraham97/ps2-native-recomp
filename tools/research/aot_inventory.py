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


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


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
    for name, base, units in re.findall(
        r'\{"([^"]+)",\s*0x([0-9a-fA-F]+)u,\s*0x[0-9a-fA-F]+u,\s*\w+,\s*\d+u,\s*(\w+),\s*\d+u\}', table
    ):
        module_bases[units] = (name + ".ovl", int(base, 16))
    modules = {}
    sources = {}
    total_units = 0
    for group, body in re.findall(r"const ps2::UnitInfo (\w+)\[\] = \{(.*?)\};", table, re.S):
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
            if module not in ("SLUS_214.10", "MOVIER.ovl", "GAMER.ovl"):
                continue
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
            "source_sha256": sources, "total_units": total_units}


def verify_entries(repo: Path, inventory: dict, rows: list[dict]) -> None:
    root = repo / "external/forks/ps2-native-recomp"
    if not root.is_dir():
        root = repo
    output = root / "build/generated-aot/phase4-all"
    if not (output / "table.cpp").is_file():
        raise AssertionError("Measured static AOT output missing; reproduce the pipeline before claiming coverage")
    actual = measure_entries(output, rows)
    recorded = inventory.get("entry_measurement", {})
    for key in ("table_sha256", "source_sha256", "total_units"):
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
        **{key: measured[key] for key in ("table_sha256", "source_sha256", "total_units")},
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
    inventory["active_fork_status"] = "Static generation measured by callable entries; Phase 4 semantic equivalence remains OPEN."
    reproduced = inventory["sources"]["reproduced_table_cpp"]
    reproduced["sha256"] = measured["table_sha256"]
    reproduced["static_unit_count"] = measured["total_units"]
    for name, field in [("SLUS_214.10", "slus_unit_count"), ("MOVIER.ovl", "movier_unit_count"), ("GAMER.ovl", "gamer_unit_count")]:
        reproduced[field] = inventory["modules"][name]["static_units"]
    reproduced["dynamic_window_unit_count"] = measured["total_units"] - sum(len(m["unit_ranges"]) for m in measured["modules"].values())
    seed = output.parent / "catalogue_entries.txt"
    if seed.exists():
        inventory["sources"]["catalogue_entry_seeds"] = {"rel_path": "build/generated-aot/catalogue_entries.txt", "sha256": sha(seed)}
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
