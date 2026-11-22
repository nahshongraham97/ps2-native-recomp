"""Freeze the tested source and scoped integration patches before clean verification."""
from pathlib import Path
import difflib
import gzip
import hashlib
import json
import subprocess
RECORD=Path(__file__).resolve().parent
ROOT=RECORD.parents[2];PARENT=ROOT.parents[2]
def git(*args):return subprocess.check_output(["git","-c","core.autocrlf=false",*args],cwd=ROOT,text=True).strip()
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
assert not git("diff","--cached","--name-only"),"Unowned staged files"
receipt=json.loads((RECORD/"validation-owner-02/VERIFICATION.json").read_text())
assert receipt["status"]=="PASS_CONNECTED_NATIVE_LOADED_SYSTEM_ART_COMPONENTS"
assert isinstance(receipt["clients"],list) and len(receipt["clients"])==4
assert isinstance(receipt["remaining_undefined_symbols"],list) and len(receipt["remaining_undefined_symbols"])==62
pins=json.loads((RECORD/"validation-owner-02/SOURCE_FILES.json").read_text())
assert pins=={name:sha(ROOT/name) for name in pins},"Source changed after passing Owner02"
approved=json.loads((RECORD/"OWNER_PATHS.json").read_text())["paths"]
assert set(git("diff","--name-only","--","runtime","tests","cmake").splitlines())<=set(approved)
for name in approved:assert b"\r" not in (ROOT/name).read_bytes(),name
for folder in ["source-consumers-01","source-filter-01"]:
    for path in (RECORD/folder).glob("*.c"):
        with path.with_suffix(".c.gz").open("xb") as f:f.write(gzip.compress(path.read_bytes(),mtime=0))
shared=git("diff","3f0155622a148ac490565337d65cb4aaf0cbce45","--","cmake/native_game.cmake","runtime/include/mka/retail_main.hpp")
(RECORD/"shared-main-cmake.patch.gz").write_bytes(gzip.compress((shared+"\n").encode(),mtime=0))
proposed=("Isolated prerequisite/font checkpoint: clean source5805592 passes seven prerequisite clients; "
    "Owner02 compiles/tests actual native sysart worker/CPU/GPU/job retirement, Session cache and production16FC70(0). "
    "All four affected clients pass; clean committed final ABI/link verification is pending. "
    "Native platform context acquisition is compiled, not game-executed. Original5FCE60 identifies "
    "permanent_strings_eng.mko; its215660/215730 native descendants remain explicitly required. "
    "Exact62-name full-entry refusal is diagnostic, not acceptance. Audio and primary dirty work preserved. "
    "Record: fork research/runs/native-loaded-font-20261122/. Owner: no action; agent next: clean verification, "
    "then the separate string resource and remaining connected startup.")
diff=[]
for name in ["PORT_STATUS.md","ROADMAP.md"]:
    old=(PARENT/name).read_bytes().decode().replace("\r\n","\n")
    lines=old.splitlines(True);assert lines[2].startswith("Current integration:")
    after="".join(lines[:2])+proposed+"\n"+"".join(lines[3:])
    after=after.replace("Both production profile connections remain OPEN.",
        "At that rejected snapshot both production profile connections remained OPEN; published3f01556 supersedes that binding status.",1)
    diff.extend(difflib.unified_diff(old.splitlines(True),after.splitlines(True),fromfile="a/"+name,tofile="b/"+name))
(RECORD/"parent-documents.phase-02.patch.gz").write_bytes(gzip.compress("".join(diff).encode(),mtime=0))
records=git("ls-files","--others","--exclude-standard","--",RECORD.relative_to(ROOT).as_posix()).splitlines()
modified_records=git("diff","--name-only","--",RECORD.relative_to(ROOT).as_posix()).splitlines()
paths=sorted(set(approved+records+modified_records))
git("add","--",*paths)
assert sorted(git("diff","--cached","--name-only").splitlines())==paths
git("diff","--cached","--check")
for name in paths:
    raw=subprocess.check_output(["git","show",":"+name],cwd=ROOT)
    assert raw==(ROOT/name).read_bytes(),name
print(git("commit","-m","Own native sysart conversion and connect retail font-zero cache"),flush=True)
print("Committed source",git("rev-parse","HEAD"),"ready for fresh clean-source verification",flush=True)
