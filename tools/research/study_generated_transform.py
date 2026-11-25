"""Measure generated-unit transformation attempts for the mandatory scale-out sample."""
from __future__ import annotations

import argparse
import csv
import gc
import hashlib
import json
from pathlib import Path
import re
import struct
import time

import transform_generated_units as transform

MANDATORY = {
    0x104000: "requested leaf matrix/float sample",
    0x102BB4: "requested struct-mutator sample",
    0x100320: "branching caller",
    0x18DA30: "$gp global cleaner",
    0x1225D0: "virtual method transition",
    0x19F0B0: "chunk-header seed",
    0x19F1E0: "chunk-finder seed",
    0x1A2BC0: "plugin-registry seed",
    0x1A64A0: "camera view-window seed",
    0x197BC0: "health deduction",
}
SUPPLEMENTAL = {
    0x102B88: "authenticated enclosing function for the requested 00102BB4 continuation",
    0x18D970: "authenticated enclosing function for the requested 0018DA30 continuation",
    0x103FA0: "candidate enclosing math CFG; entry and signature remain unproved",
    0x14DE80: "authenticated signed-index invalidation leaf; relative receiver projection requires separate E2",
}
BOUNDARIES = {
    "Core Renderer & Allocator": 0x1A3190,
    "Display & Movie GL Presentation": 0x1A63D0,
    "VFS, Files & Saves": 0x2E06E0,
    "Audio / Sound / Codecs": 0x12C440,
    "Input & Haptics": 0x185CB0,
    "Session (Host Container + Game Bindings)": 0x1588C0,
    "Tasks (Task Domain & Registry)": 0x130AC0,
    "Main Reentry & Frame Loop": 0x12E4E0,
    "Profiles (Aggregate, Records, Preferences, Menu)": 0x171B10,
    "Modes (Mode State & Dispatch)": 0x185EC0,
    "First-Scene Allocator & Continuations": 0x220900,
}

# Source-declared projections, not semantic equivalence claims. Numeric 040x
# labels are deliberately absent: they do not bind native code to retail logic.
COUNTERPARTS = {
    "0412 World": (0x1CF300, "runtime/src/native/retail_camera_world.cpp", "NativeRetailCameraWorldLink::attach_after_frame_update_from_1cf300("),
    "Tasks (Task Domain & Registry)": (0x130AC0, "runtime/src/native/retail_task_registry.cpp", "NativeRetailTaskRegistry::drain_task_observers_from_130ac0("),
    "Main Reentry & Frame Loop": (0x12E4E0, "runtime/src/native/retail_main.cpp", "!pump_retail_middleware(m, f)"),
    "Profiles (Aggregate, Records, Preferences, Menu)": (0x171B10, "runtime/src/native/retail_saved_profile_records.cpp", "NativeRetailSavedProfileRecords::initialize_from_171b10("),
    "Modes (Mode State & Dispatch)": (0x185EC0, "runtime/src/native/retail_main_mode.cpp", "// This is the original185EC0 effect, not implicit readiness in a reader."),
    "First-Scene Allocator & Continuations": (0x220900, "runtime/src/native/retail_first_scene_allocator.cpp", "continue_first_scene_at_220900("),
}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate_sample_coverage(samples, modules, reviews):
    if not set(MANDATORY) <= samples.keys():
        raise ValueError("Mandatory transform sample is missing")
    expected = {module["name"] for module in modules}
    if expected != {item["name"] for item in reviews} or len(reviews) != len(expected):
        raise ValueError("Native module correspondence review differs")
    for item in reviews:
        if item["status"] == "SOURCE_DECLARED_RETAIL_PROJECTION":
            address = int(item["original_entry"], 16)
            if address not in samples or item["name"] not in [m["name"] for m in samples[address]["native_modules"]]:
                raise ValueError("Source-bound native counterpart was not attempted")
        elif item["status"] != "NO_PROVEN_ONE_TO_ONE_RETAIL_LOGIC_COUNTERPART":
            raise ValueError("Unknown native correspondence disposition")


def verify_selected_unit(path, expected):
    if sha(path) != expected:
        raise ValueError("Selected generated-unit identity changed")


def study_controls(samples, modules, reviews, unit, expected):
    import copy
    validate_sample_coverage(samples, modules, reviews)
    verify_selected_unit(unit, expected)
    rejected = []
    missing_sample = copy.deepcopy(samples)
    missing_sample.pop(0x18DA30)
    for label, operation in (
        ("missing requested GP cleaner", lambda: validate_sample_coverage(missing_sample, modules, reviews)),
        ("missing native module review", lambda: validate_sample_coverage(samples, modules, reviews[1:])),
        ("changed generated-unit digest", lambda: verify_selected_unit(unit, "0" * 64)),
    ):
        try:
            operation()
        except ValueError:
            rejected.append(label)
        else:
            raise AssertionError("Study admitted " + label)
    assert classify_failure("unbound_memory:non_GP_base", "", True) == "unbound typed pointer/field ABI"
    assert classify_failure("unbound_indirect_target:cursor_assignment", "", True) == "lowerer cursor transport/parser limitation"
    return {"status": "PASS", "planted_rejections": rejected,
            "taxonomy_controls": ["non_GP_base is not GP evidence", "cursor_assignment is not virtual dispatch evidence"]}


def classify_failure(reason, text, authenticated_entry):
    if not authenticated_entry:
        return "function boundary and live-in state"
    if "cursor_assignment" in reason:
        return "lowerer cursor transport/parser limitation"
    if "non_GP_base" in reason or "relative_receiver" in reason or "relative_field" in reason:
        return "unbound typed pointer/field ABI"
    if "GP_field" in reason or "unknown_GP" in reason:
        return "unbound specific GP field contract"
    if "unbound_indirect_target" in reason:
        return "unbound generated indirect target; original dispatch recorded separately"
    if "unsupported_conditional" in reason or "GotoStmt" in reason:
        return "branching control flow and delay slots"
    if "ld" in text or "st" in text or "memory" in reason or "register" in reason:
        return "pointers and ABI live-ins"
    return "unsupported generated operation"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--parent-root", type=Path, required=True)
    parser.add_argument("--census", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--compiler", type=Path)
    args = parser.parse_args()
    parent = args.parent_root.resolve()
    fork = parent / "external/forks/ps2-native-recomp"
    output = args.output.resolve()
    if output.exists() or not output.is_relative_to((fork / "build/oracle-telemetry").resolve()):
        raise ValueError("Study output must be a new owned telemetry directory")
    output.mkdir()
    census = json.loads(args.census.read_bytes())
    contract_path = fork / "recomp/overlay_initializer_contract.json"
    contract = json.loads(contract_path.read_bytes())
    generated = fork / "build/generated-aot/phase4-all"
    compiler = args.compiler or parent.parent / "dependencies/compiler/llvm-mingw-20261006-ucrt-x86_64/bin/clang++.exe"
    modules_file = fork / "research/ledger/native_module_classification.json"
    modules = json.loads(modules_file.read_bytes())["modules"]
    registrations = {row["plugin_id"]: int(row["constructor"], 16) for row in census["registrations"]}
    reviews = []
    samples = {address: {"address": f"{address:08x}", "reasons": [reason], "native_modules": []}
               for address, reason in MANDATORY.items()}
    for module in modules:
        prefix = module["name"].split()[0].lower()
        binding = COUNTERPARTS.get(module["name"])
        # Retain the old exploratory callback probes, but never call their IDs
        # a native function correspondence. Actual source bindings replace two
        # misleading module-prefix selections (World and Modes).
        address = binding[0] if binding else registrations.get(prefix, BOUNDARIES.get(module["name"]))
        if address is None:
            raise ValueError("Missing native-module sample selection: " + module["name"])
        sample = samples.setdefault(address, {"address": f"{address:08x}", "reasons": [], "native_modules": []})
        review = {**module, "source_sha256": {name: sha(fork / name) for name in module["sources"]}}
        if binding:
            _, source, witness = binding
            lines = (fork / source).read_text(encoding="utf-8").splitlines()
            matches = [i+1 for i, line in enumerate(lines) if witness in line]
            if len(matches) != 1 or source not in module["sources"]:
                raise ValueError("Native counterpart source witness changed")
            review.update(status="SOURCE_DECLARED_RETAIL_PROJECTION", original_entry=f"{address:08x}",
                          witness={"path": source, "line": matches[0], "text": lines[matches[0]-1], "sha256": sha(fork/source)},
                          limit="Source-declared bounded projection, not whole routine equivalence or generated/native E2.")
            if module["name"] == "Main Reentry & Frame Loop":
                # Main calls a separately implemented original task dispatcher;
                # 12E4E0 is not the entry of the complete native main loop.
                links = [("runtime/src/native/retail_main.cpp", "!dispatch_retail_task_lists(m, f)"),
                         ("runtime/include/mka/retail_main.hpp", "// 12E4E0"),
                         ("runtime/src/native/retail_task_dispatch_main.cpp", "dispatch_first_root_zero_flags_from_12e4e0(")]
                review["call_chain_witnesses"] = []
                for path, text in links:
                    content = (fork/path).read_text(encoding="utf-8").splitlines()
                    locations = [i+1 for i,line in enumerate(content) if text in line]
                    if len(locations) != 1:
                        raise ValueError("Main to original task-dispatch source chain changed")
                    review["call_chain_witnesses"].append({"path":path, "line":locations[0], "text":content[locations[0]-1], "sha256":sha(fork/path)})
                review["limit"] = "Main calls the separately source-bound 12E4E0 task dispatcher; it is not the entry or complete semantics of native Main."
            sample["native_modules"].append({"name": module["name"], "source_witness": review["witness"]})
            sample["reasons"].append("exact native source-declared original projection")
        else:
            review.update(status="NO_PROVEN_ONE_TO_ONE_RETAIL_LOGIC_COUNTERPART", original_entry=None,
                          limit="Native host/platform/domain service; names, numeric prefixes and nearby retail callers do not establish a one-to-one original routine.")
            sample["reasons"].append("exploratory original lifecycle callback; not bound to native module" if prefix in registrations else "exploratory domain probe; not bound to native service")
        reviews.append(review)
    for address, reason in SUPPLEMENTAL.items():
        sample = samples.setdefault(address, {"address": f"{address:08x}", "reasons": [], "native_modules": []})
        sample["reasons"].append(reason)
    validate_sample_coverage(samples, modules, reviews)
    export = parent / "research/runs/ghidra-function-coverage-20261118/export-20261118-230132-1875-18160"
    with (export / "function-bodies.tsv").open(encoding="utf-8", newline="") as stream:
        ranges = list(csv.DictReader(stream, delimiter="\t"))
    catalogue = {int(row["entry"], 16) for row in ranges}
    from bounded_ownership import code_ranges
    owner = code_ranges(export)
    original_path = parent / "research/inputs/SLUS_214.10"
    image = original_path.read_bytes()
    if sha(original_path) != "a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f":
        raise ValueError("Original ELF pin changed")
    phoff = struct.unpack_from("<I", image, 28)[0]
    stride, count = struct.unpack_from("<HH", image, 42)
    segments = [struct.unpack_from("<8I", image, phoff+i*stride) for i in range(count)]
    def original(va, size):
        matches = [image[off+va-base:off+va-base+size] for kind,off,base,_,length,_,_,_ in segments
                   if kind == 1 and base <= va and va+size <= base+length]
        if len(matches) != 1 or len(matches[0]) != size:
            raise ValueError("Original address is not uniquely file backed")
        return matches[0]
    selected_owners = {owner(address) for address in samples}
    original_features = {entry: {"instructions": 0, "gp_accesses": [], "indirect_calls": [], "wide_register_memory": []}
                         for entry in selected_owners if entry is not None}
    with (export / "instructions.tsv").open(encoding="utf-8", newline="") as stream:
        for row in csv.DictReader(stream, delimiter="\t"):
            entry = int(row["function_entry"], 16) if row["function_entry"] else None
            if entry not in original_features:
                continue
            feature = original_features[entry]
            feature["instructions"] += 1
            if original(int(row["address"], 16), len(bytes.fromhex(row["bytes"]))) != bytes.fromhex(row["bytes"]):
                raise ValueError("Original instruction/export mismatch")
            word = int.from_bytes(bytes.fromhex(row["bytes"]), "little")
            opcode, base = word >> 26, word >> 21 & 31
            evidence = {"pc": row["address"], "word_le": row["bytes"].lower(), "instruction": row["mnemonic"]}
            if base == 28 and opcode in (0x20, 0x21, 0x23, 0x24, 0x25, 0x28, 0x29, 0x2b, 0x31, 0x39, 0x37, 0x3f):
                offset = word & 0xffff
                if offset >= 0x8000:
                    offset -= 0x10000
                evidence["original_global_va"] = f"{(int(contract['source_gp'], 16)+offset)&0xffffffff:08x}"
                evidence["native_field_binding"] = next((field for field in contract["fields"]
                                                          if field["source_address"].lower() == evidence["original_global_va"]), None)
                feature["gp_accesses"].append(evidence)
            if opcode == 0 and word & 63 == 9:
                feature["indirect_calls"].append(evidence)
            if opcode in (0x1e, 0x1f):
                feature["wide_register_memory"].append(evidence)
    registered = {int(row[key], 16) for row in census["registrations"] for key in ("constructor", "destructor")}
    unit_groups = {}
    for unit in sorted(generated.glob("unit_*.cpp")):
        text = unit.read_text(encoding="utf-8")
        cases = {int(address, 16): int(case) for case, address in re.findall(r"case (\d+)u: goto L_([0-9a-fA-F]+);", text)}
        for address in samples.keys() & cases.keys():
            if "generated_unit" in samples[address]:
                raise ValueError("Ambiguous generated entry")
            samples[address].update(generated_unit=unit.name, entry_case=cases[address])
            unit_groups.setdefault(unit.name, []).append(address)
    selected = list(samples.values())
    (output / "sample.json").write_text(json.dumps(selected, indent=2) + "\n", encoding="utf-8", newline="\n")
    results = []
    first_unit = generated / next(iter(unit_groups))
    controls = study_controls(samples, modules, reviews, first_unit, census["aot"]["all_measured_source_sha256"][first_unit.name])
    started = time.perf_counter()
    for unit_name, addresses in unit_groups.items():
        unit = generated / unit_name
        expected = census["aot"]["all_measured_source_sha256"][unit_name]
        verify_selected_unit(unit, expected)
        text = unit.read_text(encoding="utf-8")
        begin = time.perf_counter()
        definitions = transform.clang_ast(compiler, unit, [generated, fork / "runtime/include"], unit.stem, fork)
        definition = transform.function_definition(definitions, unit.stem)
        ast_seconds = time.perf_counter() - begin
        for address in addresses:
            sample = samples[address]
            label = "L_" + sample["address"].upper() + ":"
            segment = text.split(label, 1)[1].split("\nL_", 1)[0]
            authenticated = address in catalogue or address in registered
            descriptor = {"source_entry": sample["address"], "entry_case": sample["entry_case"],
                          "unit_name": unit.stem, "module": "SLUS_214.10"}
            begin = time.perf_counter()
            lowering = transform.Lowering(definition, descriptor, contract)
            reason = None
            try:
                ir = lowering.lower()
            except transform.Refusal as error:
                reason = str(error)
                ir = None
            elapsed = time.perf_counter() - begin
            result = dict(sample, source_unit_sha256=expected, entry_authenticated=authenticated,
                          source_span_count=sum(int(row["entry"], 16) == address for row in ranges),
                          generated_entry_fragment=segment[:3000], ast_parse_seconds=ast_seconds,
                          ast_parse_shared_by=len(addresses), lowering_seconds=elapsed,
                          first_unlowered_pc=f"{lowering.pc:08x}", lowered_effects=len(lowering.effects),
                          source_budget_debits_processed=lowering.debit)
            containing = owner(address)
            result["original_function_context"] = {"entry": f"{containing:08x}" if containing is not None else None,
                                                   "features": original_features.get(containing)}
            spans = [row for row in ranges if containing is not None and int(row["entry"],16)==containing]
            result["original_function_context"]["body_spans"] = [
                {"start": row["start"], "end": row["end"], "sha256": hashlib.sha256(original(int(row["start"],16), int(row["end"],16)-int(row["start"],16)+1)).hexdigest()} for row in spans]
            result["original_function_context"]["evidence_scope"] = "Complete body instruction bytes joined to pinned ELF; feature presence is separate from the immediate lowerer refusal."
            if reason is None:
                result.update(status="LOWERED_REQUIRES_FUNCTION_ABI_AND_DIFFERENTIAL",
                              failure_category="unbound function ABI or owner layout",
                              reason="Existing scalar initializer IR is insufficient to authenticate this distinct function ABI.",
                              typed_ir=ir)
            else:
                result.update(status="REFUSED", failure_category=classify_failure(reason, segment, authenticated), reason=reason)
            result["differential"] = {"status": "NOT_RUN_NO_AUTHENTICATED_NATIVE_UNIT",
                                       "passing_new_unit": False}
            results.append(result)
            (output / (sample["address"] + ".json")).write_text(json.dumps(result, indent=2) + "\n",
                                                               encoding="utf-8", newline="\n")
            print(sample["address"], result["status"], result["reason"], flush=True)
        del definitions, definition
        gc.collect()
    missing = [sample for sample in samples.values() if "generated_unit" not in sample]
    for sample in missing:
        results.append(dict(sample, status="REFUSED", failure_category="generated entry absent",
                            first_unlowered_pc=None, generated_unit=None, entry_case=None,
                            entry_authenticated=int(sample["address"],16) in catalogue,
                            reason="The mandatory selected original address has no generated entry.",
                            differential={"status": "NOT_RUN_NO_AUTHENTICATED_NATIVE_UNIT", "passing_new_unit": False}))
    report = {"schema": "mkar.generated-transform-scale-study.v1", "status": "COMPLETED_ATTEMPTS_WITH_RECORDED_LIMITS", "samples": results,
              "controls": controls,
              "mandatory_addresses": [f"{address:08x}" for address in MANDATORY],
              "supplemental_addresses": [f"{address:08x}" for address in SUPPLEMENTAL],
              "native_module_count": len(modules), "sampled_unique_entries": len(samples),
              "native_module_correspondence_review": reviews,
              "native_module_coverage": {
                  "all_modules_reviewed": len(reviews),
                  "platform_boundary_modules": sum(m["classification"] == "platform-boundary" for m in reviews),
                  "retail_logic_modules": sum(m["classification"] == "retail-logic" for m in reviews),
                  "mixed_modules": sum(m["classification"] == "mixed" for m in reviews),
                  "source_bound_retail_logic_or_mixed_attempted": sum(m["classification"] in ("retail-logic", "mixed") and m["status"] == "SOURCE_DECLARED_RETAIL_PROJECTION" for m in reviews),
                  "unbound_retail_logic_or_mixed": [m["name"] for m in reviews if m["classification"] in ("retail-logic", "mixed") and m["status"] != "SOURCE_DECLARED_RETAIL_PROJECTION"],
                  "scope": "Platform boundaries do not require generated retail counterparts. Unbound mixed Session remains a specific 5S.1 coverage limit, not absent production native code."},
              "elapsed_seconds": time.perf_counter() - started,
              "inputs": {"census_sha256": sha(args.census), "contract_sha256": sha(contract_path),
                         "original_elf_sha256": sha(original_path), "function_bodies_sha256": sha(export/"function-bodies.tsv"),
                         "instructions_sha256": sha(export/"instructions.tsv"), "study_tool_sha256": sha(Path(__file__)),
                         "module_manifest_sha256": sha(modules_file), "transformer_sha256": sha(Path(transform.__file__)),
                         "compiler_sha256": sha(compiler)},
              "new_verified_units": 0, "native_credit_added": 0,
              "binding_scope": {"global_pointer_original_va": contract["source_gp"],
                                "bound_global_fields": contract["fields"],
                                "pointer_fields": "No general 32-bit game pointer to native object binding is implemented in this scalar initializer lowerer.",
                                "global_map_limit": "Existing GP access inventories identify original locations. The initializer contract binds only its measured field; it is not a full native global-type map.",
                                "vtable_limit": "Recovered constructor/table layouts do not by themselves bind every indirect call in the sample. A native callable/receiver binding must be proved before emission.",
                                "runtime_cost": "New samples produced no authenticated native unit, so no runtime cost or differential result can be measured for them."},
              "Not verified": ["Every new sample lacks a passing native differential. Existing two prototypes retain their separate measured credit.",
                               "The overlay field map does not establish arbitrary game-struct pointer, global or vtable bindings."],
              "Contradictions with earlier documents": ["Generated page entries and labels can be continuation points. Original entry authentication is recorded separately.",
                  "Earlier module-prefix sampling did not prove native/original correspondence. The 0409 native Texture Dictionaries, 040a Materials and 040b Geometry labels differ from the original plugin identities; production module names/code are preserved.",
                  "A cursor_assignment parser refusal is not original virtual dispatch. A non_GP_base refusal is not GP access evidence."],
              "Falsification attempts": ["The existing transformer refuses unsupported selected operations before native emission. Separate positive and semantic failure controls are required for this study receipt."]}
    (output / "RESULT.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print("STUDY", output, "samples", len(results))


if __name__ == "__main__":
    main()
