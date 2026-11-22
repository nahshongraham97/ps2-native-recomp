"""Create a scoped local implementation checkpoint after strict staging validation."""
from pathlib import Path
import gzip
import hashlib
import difflib
import json
import subprocess

RECORD = Path(__file__).resolve().parent
ROOT = RECORD.parents[2]
PARENT = ROOT.parents[2]
def git(*args):
    return subprocess.check_output(["git", "-c", "core.autocrlf=false", *args], cwd=ROOT, text=True).strip()
assert git("branch", "--show-current") == "selected-resource-owner-20261122"
assert not git("diff", "--cached", "--name-only"), "Unowned staged files"
imported = json.loads((RECORD / "PREREQUISITE_IMPORT.json").read_text())["paths"]
approved = sorted([*imported, "cmake/native_game.cmake"])
for name in approved:
    assert b"\r" not in (ROOT / name).read_bytes(), name
shared = git("diff", "--", "cmake/native_game.cmake").encode()
(RECORD / "shared-prerequisites.patch.gz").write_bytes(gzip.compress(shared, mtime=0))
proposed = ("Prerequisite integration is implemented/unbuilt on isolated selected-resource-owner-20261122: "
            "immutable b6042e8/6382224/69e0f3c sources plus explicitly authored validation-order and "
            "unknown-delete Session-seal corrections. Primary dirty work is preserved, audio untouched. "
            "Clean committed checks precede actual loaded-section/font ownership and connected tests. "
            "Record: fork research/runs/native-loaded-font-20261122/. Whole-game acceptance OPEN. Owner: no action.")
diff = []
for name in ["PORT_STATUS.md", "ROADMAP.md"]:
    old = (PARENT / name).read_bytes().decode().replace("\r\n", "\n")
    lines = old.splitlines(True)
    assert lines[2].startswith("Current integration:")
    after = "".join(lines[:2]) + proposed + "\n" + "".join(lines[3:])
    diff.extend(difflib.unified_diff(old.splitlines(True), after.splitlines(True), fromfile="a/"+name, tofile="b/"+name))
(RECORD / "parent-documents.phase-01.patch.gz").write_bytes(gzip.compress("".join(diff).encode(), mtime=0))
records = git("ls-files", "--others", "--exclude-standard", "--", RECORD.relative_to(ROOT).as_posix()).splitlines()
paths = sorted(set(approved + records))
git("diff", "--check", "--", *approved)
git("add", "--", *paths)
assert sorted(git("diff", "--cached", "--name-only").splitlines()) == paths
git("diff", "--cached", "--check")
for name in paths:
    raw = subprocess.check_output(["git", "show", ":"+name], cwd=ROOT)
    assert raw == (ROOT / name).read_bytes(), name
print(git("commit", "-m", "Integrate reviewed native file and GL prerequisites with admission corrections"), flush=True)
print("Local source checkpoint", git("rev-parse", "HEAD"), "implementation unbuilt until recorded verification", flush=True)
