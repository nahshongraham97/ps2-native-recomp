"""Produce a scoped parent-document integration patch without editing shared files."""
from pathlib import Path
import difflib
import gzip
import hashlib
import json
import subprocess

RECORD = Path(__file__).resolve().parent
ROOT = RECORD.parents[2]
PARENT = ROOT.parents[2]
patch = []
identities = {}
addition = (
    "Source-derived selected-resource audit (isolated selected-resource-owner-20261122, "
    "base fork3f01556): both saved-profile production bindings and actual334700 arcade "
    "reset already exist.16FC70(0) and18DA30 remain required. Original-byte/private-EE "
    "evidence confirms that texture-cache publication cannot use the partial CPU "
    "section as loaded/GPU success; actual section conversion, pending-job retirement "
    "and production renderer/context acquisition remain required. No new runtime "
    "implementation/build/game pass. See fork research/runs/native-selected-resource-owner-20261122/AUDIT.md. "
    "Primary next: finish in-flight prerequisites, then actual loaded-section/font "
    "ownership; do not bypass connected startup for broad GS replacement. Owner: no action.\n\n"
)
for name in ["PORT_STATUS.md", "ROADMAP.md",
             "research/runs/parallel-agent-preparation-20261122/READY_WORK.md"]:
    path = PARENT / name
    before = path.read_bytes()
    old = before.decode("utf-8").replace("\r\n", "\n")
    if name == "research/runs/parallel-agent-preparation-20261122/READY_WORK.md":
        after = old.replace(
            "# Historical preparation and current agent assignments\n\n",
            "# Historical preparation and current agent assignments\n\n"
            "Superseding source check, 2026-11-22: fork main3f01556 now defines both "
            "production saved-profile bindings; the53e1a59 remote check below is "
            "historical. Their clean component checkpoint is recorded in "
            "`../native-retail-saved-profile-records-20261122/`. The remaining "
            "16FC70 resource lane requires actual loaded-section/GPU ownership, "
            "not a synthetic or partial-CPU font binding. See fork "
            "`research/runs/native-selected-resource-owner-20261122/AUDIT.md`.\n\n", 1)
        after = after.replace("Current assignment update: rejected b986341", "Historical assignment update: rejected b986341", 1)
        after = after.replace("both production profile connections remain OPEN.",
                              "both production profile connections remained OPEN at that snapshot; "
                              "the published3f01556 checkpoint supersedes that binding status.", 1)
    else:
        at = old.index("\n\n") + 2
        after = old[:at] + addition + old[at:]
        after = after.replace(
            "Both production profile connections remain OPEN.",
            "At that rejected/retry snapshot both production profile connections remained OPEN; "
            "the published3f01556 checkpoint above supersedes that binding status.", 1)
    assert after != old and "\r" not in after
    identities[name] = dict(base_raw_sha256=hashlib.sha256(before).hexdigest(),
                             proposed_lf_sha256=hashlib.sha256(after.encode()).hexdigest())
    patch.extend(difflib.unified_diff(old.splitlines(True), after.splitlines(True),
                                     fromfile="a/" + name, tofile="b/" + name))
with (RECORD / "parent-documents.patch").open("x", newline="\n") as f:
    f.writelines(patch)
for name, identity in identities.items():
    assert hashlib.sha256((PARENT / name).read_bytes()).hexdigest() == identity["base_raw_sha256"]

private_decompiler = {}
for path in sorted((RECORD / "source-01").glob("*.c")):
    raw = path.read_bytes()
    with path.with_suffix(".c.gz").open("xb") as f:
        f.write(gzip.compress(raw, mtime=0))
    private_decompiler[path.relative_to(ROOT).as_posix()] = hashlib.sha256(raw).hexdigest()
paths = sorted(p for p in RECORD.rglob("*") if p.is_file() and p.suffix != ".c")
for path in paths:
    if path.suffix in {".md", ".py", ".java", ".asm", ".c", ".tsv", ".txt", ".json", ".patch"}:
        assert b"\r" not in path.read_bytes(), path
value = dict(status="SOURCE_AUDIT_ONLY_IMPLEMENTATION_BLOCKED",
    parent_patch_inputs=identities,
    unchanged_production_paths=subprocess.check_output(
        ["git", "diff", "--name-only", "--", "runtime", "tests", "cmake", "CMakeLists.txt"],
        cwd=ROOT, text=True).splitlines(),
    record_files={p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                  for p in paths},
    raw_private_decompiler_outputs=private_decompiler,
    Main_CMake_changes=False, audio_changes=False, active_background_runs=False,
    full_game_acceptance="OPEN")
assert not value["unchanged_production_paths"]
with (RECORD / "DELIVERY.json").open("x", newline="\n") as f:
    json.dump(value, f, indent=2)
    f.write("\n")
print("Prepared documentation-only integration patch; shared source/docs unchanged")
