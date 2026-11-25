#!/usr/bin/env python3
"""Read-only artifact audit; never harvest, build, import lane scripts or run a game.

An audit completing successfully is not closure acceptance. --require-closed
returns 2 when the recorded proof is incomplete (the historical lane is OPEN).
All input paths are explicit; the only writes are two files in a fresh owned
build/oracle-telemetry output directory in this tool's worktree.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys
from datetime import datetime, timezone

WORKTREE = Path(__file__).resolve().parents[2]
PC = 0x00B17700
RA = 0x002E6180


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


class Reader:
    def __init__(self):
        self.pins = {}

    def read(self, path: Path, required=False):
        path = path.resolve()
        if not path.is_file():
            if required:
                raise ValueError(f"missing required input: {path}")
            self.pins[str(path)] = {"present": False}
            return None
        before = path.stat()
        data = path.read_bytes()
        after = path.stat()
        if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
            raise ValueError(f"input changed while reading: {path}")
        old = self.pins.get(str(path))
        if old is not None and (not old["present"] or old["sha256"] != sha(data)):
            raise ValueError(f"input changed between reads: {path}")
        self.pins[str(path)] = {
            "present": True, "size": len(data), "sha256": sha(data),
            "mtime_ns": after.st_mtime_ns,
            "mtime_utc": datetime.fromtimestamp(after.st_mtime, timezone.utc).isoformat(),
        }
        return data

    def text(self, path, required=False):
        data = self.read(path, required)
        return data.decode("utf-8", errors="strict") if data is not None else ""

    def unchanged(self):
        # Rehash every observed input. A stat-only check cannot prove stability.
        for path, pin in self.pins.items():
            p = Path(path)
            if pin["present"]:
                if not p.is_file() or sha(p.read_bytes()) != pin["sha256"]:
                    raise ValueError(f"input changed during audit: {path}")
            elif p.exists():
                raise ValueError(f"previously absent input appeared: {path}")


def image_join(expected, candidate):
    """Full image identity plus exact entry; same VA alone is insufficient."""
    failures = []
    for field in ("base", "size", "sha256"):
        if expected.get(field) != candidate.get(field):
            failures.append(f"image_{field}_mismatch")
    if expected.get("pc") not in candidate.get("entries", []):
        failures.append("missing_exact_entry")
    if expected.get("word") != candidate.get("word"):
        failures.append("same_va_word_mismatch")
    return failures


def generated_entry(unit, name, offset):
    array = re.search(r"\b" + re.escape(name) + r"_entries_0000\[\d+\]\s*=\s*\{([^}]+)\}", unit)
    values = [int(v) for v in re.findall(r"\b\d+\b", array[1])] if array else []
    entry = values[offset // 4] if offset % 4 == 0 and offset // 4 < len(values) else 0
    pc = 0xB14000 + offset
    label = f"L_{pc:08X}"
    return {"entry_id": entry, "switch_target_matches": bool(entry and re.search(
        rf"case\s+{entry}u?:\s*goto\s+{label}\s*;", unit)),
        "label_present": bool(re.search(r"\b" + label + r"\s*:", unit))}


def closure_objections(proof):
    """Necessary evidence checks, never an artifact-name or zero-count shortcut."""
    failures = []
    if not proof.get("binary_sha256") or proof.get("run_binary_sha256") != proof.get("binary_sha256"):
        failures.append("run_binary_unbound")
    expected = proof.get("expected_cases", [])
    actual = proof.get("completed_cases", [])
    if not expected or not actual:
        failures.append("zero_case_denominator")
    if len(set(expected)) != len(expected) or len(set(actual)) != len(actual) or set(expected) != set(actual):
        failures.append("full_suite_not_proved")
    if proof.get("subset"):
        failures.append("subset_not_full_suite")
    for field in ("all_cases_passed", "no_ee_live_runtime", "heldout_passed",
                  "exact_images_bound", "template_registry_authenticated", "historical55_fit"):
        if proof.get(field) is not True:
            failures.append(field + "_unproved")
    return failures


def duration_objections(loaded, stopped, absolute_limit, requested_new_frames):
    failures = []
    if loaded is None or stopped is None:
        return ["loaded_or_stopped_frame_unbound"]
    if absolute_limit is None or absolute_limit <= loaded:
        failures.append("absolute_threshold_not_after_loaded_frame")
    if stopped - loaded < requested_new_frames:
        failures.append("requested_new_frame_duration_not_observed")
    return failures


def self_test():
    original = {"base": 0xB14000, "size": 16384, "sha256": "a" * 64,
                "pc": PC, "word": 0xD9650000}
    candidate = dict(original, entries=[PC])
    assert not image_join(original, candidate)
    cases = []
    for name, changes, reason in [
        ("wrong_image", {"sha256": "b" * 64}, "image_sha256_mismatch"),
        ("same_va_word_change", {"word": 0x0088202D}, "same_va_word_mismatch"),
        ("missing_entry", {"entries": [PC + 4]}, "missing_exact_entry"),
    ]:
        got = image_join(original, dict(candidate, **changes))
        assert reason in got, (name, got)
        cases.append({"name": name, "rejected_by": got})
    complete = dict(binary_sha256="a", run_binary_sha256="a", expected_cases=["one", "two"],
                    completed_cases=["one", "two"], subset=False, all_cases_passed=True,
                    no_ee_live_runtime=True, heldout_passed=True, exact_images_bound=True,
                    template_registry_authenticated=True, historical55_fit=True)
    assert not closure_objections(complete)
    for name, changes, reason in [
        ("unbound_binary", {"run_binary_sha256": None}, "run_binary_unbound"),
        ("zero_case_false_convergence", {"expected_cases": [], "completed_cases": []}, "zero_case_denominator"),
        ("subset_false_convergence", {"completed_cases": ["two"], "subset": True}, "subset_not_full_suite"),
        ("missing_heldout", {"heldout_passed": False}, "heldout_passed_unproved"),
    ]:
        got = closure_objections(dict(complete, **changes))
        assert reason in got, (name, got)
        cases.append({"name": name, "rejected_by": got})
    assert not duration_objections(17793, 17883, 17883, 90)
    got = duration_objections(17793, 17794, 90, 90)
    assert "absolute_threshold_not_after_loaded_frame" in got
    cases.append({"name": "absolute_frame_threshold_below_loaded_start", "rejected_by": got})
    return {"status": "PASS", "positive_controls": 3, "rejected_mutations": cases,
            "scope": "Audit evidence predicates only; no game execution or semantic template proof."}


def source_lineage(source, base, reader):
    roots = []
    for label, root, cache in [
        ("protected_R192_build_source", source / "build/r192-hooks/src", source / "build/r192-hooks/out/CMakeCache.txt"),
        ("closure_build_source", base / "src", base / "out/CMakeCache.txt"),
        ("audit_worktree_tooling_only", WORKTREE, None),
        ("canonical_fork_current_source", source, None),
    ]:
        root = root.resolve()
        system = reader.text(root / "runtime/src/system.cpp", True)
        reader.read(root / "runtime/include/ps2/cpu.hpp", True)
        reader.read(root / "CMakeLists.txt", True)
        version = re.search(r"constexpr\s+std::uint32_t\s+kStateVersion\s*=\s*(\d+)\s*;", system)
        cache_text = reader.text(cache, True) if cache else ""
        home = re.search(r"^CMAKE_HOME_DIRECTORY:INTERNAL=(.+)$", cache_text, re.M)
        home_value = home[1].strip() if home else None
        roots.append({"role": label, "source_root": str(root), "serializer_system": str(root / "runtime/src/system.cpp"),
                      "cpu_header": str(root / "runtime/include/ps2/cpu.hpp"),
                      "state_write_version": int(version[1]) if version else None,
                      "cmake_cache": str(cache.resolve()) if cache else None,
                      "cmake_home_directory": home_value,
                      "cache_home_matches_source_root": Path(home_value).resolve() == root if home_value else None,
                      "binary_reproduction_proved": False})
    run_path = source.parents[2] / "research/runs/r192-hooks-playable-run-20261125/RUN.md"
    run = reader.text(run_path)
    recorded = next((line for line in run.splitlines() if "**Source Lineage:**" in line), None)
    versions = {r["role"]: r["state_write_version"] for r in roots}
    return {"roots": roots, "recorded_R192_lineage": {"source_record": str(run_path), "quoted_line": recorded,
             "scope": "Recorded 67612aa descendant plus restored hooks/wrappers; not independently reproduced by this audit."},
            "tooling_serializer_differs_from_R192": versions["audit_worktree_tooling_only"] != versions["protected_R192_build_source"],
            "closure_serializer_matches_R192": versions["closure_build_source"] == versions["protected_R192_build_source"],
            "limit": "W/main is audit tooling only. Its current source/serializer cannot be assumed to reproduce protected 8AB43BDA... PE. Cache HOME and source hashes identify present build inputs, not historical build reproducibility or complete ABI equality. No source/build/original changes performed."}


def state_durations(base, reader, fail_loop):
    system_path = base / "src/runtime/src/system.cpp"
    system = reader.text(system_path, True)
    patterns = [r"\+\+g_frames;", r"s\.pod\(g_frames\);",
                r"if \(g_options\.max_frames && g_frames >= g_options\.max_frames\) halt\(\"frame limit reached\"\);",
                r"\[state\] loaded frame"]
    snippets = [{"line": i, "text": line.strip()} for i, line in enumerate(system.splitlines(), 1)
                if any(re.search(pattern, line) for pattern in patterns)]
    frame_arg = re.search(r'\["--load-state", str\(st\), "--frames", "(\d+)"\]', fail_loop)
    limit = int(frame_arg[1]) if frame_arg else None
    rows = []
    for path in sorted((base / "e7_logs").glob("state_*.stderr.log")):
        text = reader.text(path, True)
        loaded = [int(v) for v in re.findall(r"\[state\] loaded frame (\d+)", text)]
        stopped = [int(v) for v in re.findall(r"\[ee\] stopped after (\d+) frames", text)]
        start = loaded[0] if len(loaded) == 1 else None
        end = stopped[0] if len(stopped) == 1 else None
        delta = end - start if start is not None and end is not None else None
        rows.append({"log": str(path.resolve()), "loaded_frame_observations": loaded,
                     "stopped_frame_observations": stopped, "observed_frame_delta": delta,
                     "frame_limit_stop_observed": "frame limit reached" in text,
                     "ninety_new_frames_proved": delta is not None and delta >= 90,
                     "runner_threshold_assessment": duration_objections(start, end, limit, 90),
                     "command_binding": "Historical command/binary not recorded in this log; threshold comes from current runner source."})
    return {"source": str(system_path.resolve()), "source_snippets": snippets,
            "g_frames_serialized_and_restored": "s.pod(g_frames);" in system,
            "frame_limit_is_absolute_restored_counter": all(bool(re.search(p, system)) for p in patterns[:3]),
            "current_runner_state_frame_argument": limit,
            "state_log_count": len(rows), "logs": rows,
            "measured_delta_histogram": {str(delta): sum(r["observed_frame_delta"] == delta for r in rows)
                                         for delta in sorted({r["observed_frame_delta"] for r in rows}, key=lambda v: (-1 if v is None else v))},
            "ninety_new_frames_proved_log_count": sum(r["ninety_new_frames_proved"] for r in rows),
            "limit": "Late version-8 snapshots restore g_frames. --frames 90 is an absolute threshold, not 90 additional frames. Report actual loaded/stopped deltas only. This concerns old closure replay duration, not native 61-snapshot boundary comparisons."}


def audit(source: Path, base: Path, reader: Reader):
    summary = json.loads(reader.text(base / "e7_last_run.json", True))
    log = reader.text(base / "e7_logs/session_20261125_full.stderr.log")
    pe = reader.read(base / "out/ps2run.exe", True)
    entries = reader.text(base / "entries_closure.txt", True).splitlines()
    fail_loop = reader.text(base / "e7_e9_failfast_loop.py", True)
    remaining_loop = reader.text(base / "e9_loop_remaining.py", True)
    finalize = reader.text(base / "e2_e3_finalize.py", True)
    matching = reader.text(base / "e3_match_eelive_to_ram.py")
    cmake = reader.text(base / "src/CMakeLists.txt", True)
    ninja = reader.text(base / "out/build.ninja", True)
    stubs = reader.text(base / "src/runtime/src/aot_only_stubs.cpp", True)
    dispatch = reader.text(base / "src/runtime/src/dispatch.cpp", True)
    reader.read(base / "e2_e3_summary.json")
    reader.read(source / "AGENTS.md", True)
    reader.read(Path(__file__), True)
    entry_map = {}
    for line in entries:
        if ":" in line:
            name, offset = line.strip().split(":", 1)
            entry_map.setdefault(name, []).append(int(offset, 16))
    variants = []
    for p in sorted((base / "overlays").glob("00B14000_*.bin")):
        data = reader.read(p, True)
        offsets = entry_map.get(p.stem, [])
        generated = base / "generated_ee" / f"m_{p.stem}_unit_0000.cpp"
        unit = reader.text(generated)
        variants.append({"path": str(p.resolve()), "base": 0xB14000, "size": len(data),
                         "sha256": sha(data), "word_at_B17700": f"{struct.unpack_from('<I', data, PC - 0xB14000)[0]:08X}" if len(data) >= PC - 0xB14000 + 4 else None,
                         "entries": [f"{0xB14000 + o:08X}" for o in offsets],
                         "has_exact_entry": PC - 0xB14000 in offsets,
                         "generated_unit_present": bool(unit),
                         "generated_entry_join": generated_entry(unit, "m_" + p.stem, PC - 0xB14000),
                         "runtime_image_match": "UNKNOWN: failing RAM image was not retained in this report"})
    table = reader.text(base / "generated_ee/table.cpp")
    for variant in variants:
        name = Path(variant["path"]).stem
        variant["module_named_in_generated_table"] = name in table
        checks = re.search(r"\bm_" + re.escape(name) + r"_checks\[\]\s*=\s*\{([^}]+)\}", table)
        values = [int(v, 16) for v in re.findall(r"0x([0-9A-Fa-f]+)u", checks[1])] if checks else []
        data = reader.read(Path(variant["path"]), True)
        mismatch = []
        for offset, word in zip(values[::2], values[1::2]):
            if offset + 4 > len(data) or struct.unpack_from("<I", data, offset)[0] != word:
                mismatch.append(offset)
        variant["generated_check_words"] = {"pair_count": len(values) // 2,
                                              "malformed_odd_word_count": len(values) % 2 != 0,
                                              "mismatch_offsets": mismatch,
                                              "all_recorded_pairs_match_page": bool(values) and len(values) % 2 == 0 and not mismatch,
                                              "limit": "Matches recorded translated words only; remaining page semantics and historical loaded identity unknown."}
    archived = []
    manifest_path = base / "e1_out/eelive_r192/manifest.jsonl"
    manifest = reader.text(manifest_path)
    rows = [json.loads(line) for line in manifest.splitlines() if line.strip()]
    for row in rows:
        fb = int(row["fixed_base"], 16)
        if fb <= PC < fb + row["size"]:
            check_path = manifest_path.parent / (row["name"] + ".checks.bin")
            data = reader.read(check_path, True)
            words = [word for off, word in struct.iter_unpack("<II", data) if fb + off == PC]
            if words or any(int(e, 16) == PC for e in row["entries"]):
                archived.append({"name": row["name"], "fixed_base": row["fixed_base"],
                                 "declared_size": row["size"], "check_count": row["checks"],
                                 "exact_entry": any(int(e, 16) == PC for e in row["entries"]),
                                 "words_at_pc": [f"{w:08X}" for w in words],
                                 "meaning": "Sparse checked words, not an authenticated complete page."})
    pe_pin = reader.pins[str((base / "out/ps2run.exe").resolve())]
    summary_pin = reader.pins[str((base / "e7_last_run.json").resolve())]
    frame_match = re.findall(r"stopped after (\d+) frames[^\n]*", log)
    missing = re.findall(r"UNTRANSLATED JUMP at 0x([\dA-Fa-f]+).*?ra=0x([\dA-Fa-f]+)", log)
    proof = {"binary_sha256": sha(pe), "run_binary_sha256": summary.get("binary_sha256"),
             "expected_cases": [], "completed_cases": [], "subset": True,
             "all_cases_passed": summary.get("failed") == 0 and summary.get("total", 0) > 0,
             "no_ee_live_runtime": False, "heldout_passed": False, "exact_images_bound": False,
             "template_registry_authenticated": False, "historical55_fit": False}
    return {
        "schema": "r192_aot_closure_read_only_audit_v1", "status": "OPEN",
        "audit_completed": True, "source_fork": str(source), "candidate_dir": str(base),
        "source_lineage": source_lineage(source, base, reader),
        "state_replay_duration": state_durations(base, reader, fail_loop),
        "source_configuration": {
            "aot_stubs_listed_in_cmake": "runtime/src/aot_only_stubs.cpp" in cmake,
            "aot_stubs_listed_in_ninja": "aot_only_stubs.cpp.obj" in ninja,
            "ee_live_object_listed_in_ninja": "ee_live.cpp.obj" in ninja,
            "ee_live_active_and_translate_false": bool(re.search(r"bool active\(\)\s*\{\s*return false;", stubs)) and "translate_missing(std::uint32_t) { return false; }" in stubs,
            "dispatch_failfast_present": "[ee-aot-failfast] UNTRANSLATED JUMP" in dispatch,
            "limit": "Source/build graph configuration is separate from bound runtime and zero-miss proof."},
        "latest_report": {"summary": summary, "stopped_frames": frame_match[-1] if frame_match else None,
                          "observed_misses_pc_ra": missing, "binary_newer_than_summary": pe_pin["mtime_ns"] > summary_pin["mtime_ns"],
                          "binary_bound_to_run": proof["run_binary_sha256"] == sha(pe),
                          "ra_interpretation": "Observed caller continuation only. Writer/emitter identity and original call-site context are UNPROVED."},
        "denominators": {
            "summary_total": summary.get("total"), "summary_passed": summary.get("passed"), "summary_failed": summary.get("failed"),
            "full_suite_cases_recorded": False, "heldout_session_proof": False,
            "only_failed_filter_present": "args.only_failed" in fail_loop,
            "zero_case_cli_can_exit_zero": "sys.exit(0 if failed == 0 else 1)" in fail_loop,
            "subset_convergence_loop_present": '"--only-failed"' in remaining_loop,
            "limit": "No explicit expected/completed case identity manifest or run command/binary pin. A subset/empty suite is not full-suite acceptance."},
        "images": {"probe_pc": f"{PC:08X}", "probe_ra": f"{RA:08X}", "retained_B14000_variant_count": len(variants),
                   "variants": variants, "archived_manifest_rows": len(rows), "archived_probe_checks": archived,
                   "join_status": "UNPROVED: exact failing full image not bound to a generated exact entry and tested binary",
                   "existing_matcher_limits": ["main_* rows are counted as disc-covered without comparing ELF words" if 'name.startswith("main_")' in matching else "matcher unavailable/changed",
                                                "other comparisons check archived pairs, not unrecorded bytes or historical execution"],
                   "partial_reconstruction_permitted": False},
        "templates": {"authenticated_count": None, "heuristic_anchor_count": 3 if "def find_stub_entries_in_unit" in finalize else None,
                      "heuristics_are_templates": False, "historical55_exact_manifest": False, "all55_fit": "NOT_VERIFIED",
                      "limit": "Entry discovery, page count, near-duplicate merging and sparse word matches do not prove a finite emitter template catalogue."},
        "acceptance_objections": closure_objections(proof),
        "zero_ee_live_acceptance_proved": False,
        "next_safe_dependency": "Static exact-image/check-word/entry/generated-table/binary identity join; retain all same-base variants and unknown words. No partial reconstruction or ingestion. Held-out and full-suite runtime acceptance remains a later separate authorized test.",
        "scope": "No harvesting, launch, build, ingestion, source-lane writes, ownership/native credit, protected original changes or writer attribution."}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--source-fork", type=Path)
    ap.add_argument("--candidate-dir", type=Path)
    ap.add_argument("--output", type=Path)
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--require-closed", action="store_true")
    args = ap.parse_args()
    tests = self_test() if args.self_test else None
    if not args.source_fork and not args.candidate_dir and not args.output and tests:
        print(json.dumps(tests, indent=2)); return 0
    if not all((args.source_fork, args.candidate_dir, args.output)):
        ap.error("--source-fork, --candidate-dir and --output are required together")
    source, candidate, output = (p.resolve() for p in (args.source_fork, args.candidate_dir, args.output))
    telemetry = (WORKTREE / "build/oracle-telemetry").resolve()
    if not output.is_relative_to(telemetry) or output == telemetry or output.exists():
        ap.error("output must be a fresh owned directory beneath this worktree's build/oracle-telemetry")
    if output.is_relative_to(candidate) or candidate.is_relative_to(output):
        ap.error("output must not overlap the read-only candidate")
    reader = Reader()
    result = audit(source, candidate, reader)
    result["self_test"] = tests
    reader.unchanged()
    result["input_pins"] = reader.pins
    result["replay_argv"] = [sys.executable, str(Path(__file__).resolve()), *sys.argv[1:]]
    result["output_replay_limit"] = "Use a new output directory for each replay; existing evidence is never overwritten."
    output.mkdir(parents=True, exist_ok=False)
    payload = (json.dumps(result, indent=2) + "\n").encode("utf-8")
    (output / "RESULT.json").write_bytes(payload)
    (output / "RECEIPT.json").write_bytes((json.dumps({"result_sha256": sha(payload), "status": result["status"],
                                                      "tool_sha256": sha(Path(__file__).read_bytes())}, indent=2) + "\n").encode())
    print(json.dumps({"status": result["status"], "result": str(output / "RESULT.json"),
                      "sha256": sha(payload), "variants": result["images"]["retained_B14000_variant_count"]}))
    return 2 if args.require_closed else 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ValueError, OSError, KeyError, json.JSONDecodeError) as exc:
        print(f"AUDIT_REFUSED: {exc}", file=sys.stderr)
        raise SystemExit(2)
