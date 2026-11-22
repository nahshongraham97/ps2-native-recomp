"""Failure-propagating, exact-path source-audit branch publication; never writes main."""
from pathlib import Path
import hashlib
import json
import subprocess

RECORD = Path(__file__).resolve().parent
ROOT = RECORD.parents[2]
BRANCH = "selected-resource-owner-20261122"

def run(*args):
    return subprocess.check_output(["git", "-c", "core.autocrlf=false", *args],
                                   cwd=ROOT, text=True).strip()

assert run("branch", "--show-current") == BRANCH
assert run("rev-parse", "HEAD") == "3f0155622a148ac490565337d65cb4aaf0cbce45"
assert not run("diff", "--cached", "--name-only"), "Preexisting staged paths are not ours"
delivery = json.loads((RECORD / "DELIVERY.json").read_text())
assert delivery["status"] == "SOURCE_AUDIT_ONLY_IMPLEMENTATION_BLOCKED"
assert delivery["unchanged_production_paths"] == []
paths = sorted([*delivery["record_files"], (RECORD / "DELIVERY.json").relative_to(ROOT).as_posix()])
for path, expected in delivery["record_files"].items():
    assert path.startswith("research/runs/native-selected-resource-owner-20261122/")
    assert hashlib.sha256((ROOT / path).read_bytes()).hexdigest() == expected, path
assert not run("diff", "--name-only", "--", "runtime", "tests", "cmake", "CMakeLists.txt")
run("add", "--", *paths)
staged = run("diff", "--cached", "--name-only").splitlines()
assert sorted(staged) == paths, (staged, paths)
run("diff", "--cached", "--check")
for path in paths:
    raw = subprocess.check_output(["git", "show", ":" + path], cwd=ROOT)
    assert raw == (ROOT / path).read_bytes(), path
print(run("commit", "-m", "Audit selected-font production prerequisites against published main"), flush=True)
print(run("push", "-u", "origin", "HEAD:refs/heads/" + BRANCH), flush=True)
print("Published branch", BRANCH, run("rev-parse", "HEAD"), flush=True)
