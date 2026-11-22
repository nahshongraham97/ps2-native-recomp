"""Commit the verified implementation, with strict scoped staging, before clean checks."""
from pathlib import Path
import hashlib
import json
import subprocess
RECORD=Path(__file__).resolve().parent;ROOT=RECORD.parents[2];PARENT=ROOT.parents[2]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def git(*args):return subprocess.check_output(["git","-c","core.autocrlf=false",*args],cwd=ROOT,text=True).strip()
assert git("branch","--show-current")=="main"
assert not git("diff","--cached","--name-only")
result=json.loads((RECORD/"validation-02/VERIFICATION.json").read_text())
assert result["status"]=="PASS_NATIVE_PERMANENT_STRINGS_COMPONENTS"
assert result["focused_new_client_only"] is True
assert [c["name"] for c in result["clients"]]==["test_native_retail_permanent_strings"]
assert len(result["remaining_undefined_symbols"])==60 and len(result["resolved_bindings"])==2
pins=json.loads((RECORD/"validation-02/SOURCE_FILES.json").read_text())
assert pins=={n:sha(ROOT/n) for n in pins}
new=["runtime/include/mka/retail_permanent_string_data.hpp","runtime/src/native/retail_permanent_string_data.cpp",
 "runtime/include/mka/retail_permanent_strings.hpp","runtime/src/native/retail_permanent_strings.cpp",
 "tests/test_retail_permanent_strings.cpp"]
changed=git("diff","--name-only","--","runtime","tests","cmake").splitlines()
approved=sorted(set(new+changed))
assert all("/audio" not in n and "/sound" not in n and "/iop/" not in n for n in approved)
for name in approved:assert b"\r" not in (ROOT/name).read_bytes(),name
for name in ["MAIN_PROMOTION.json","BEFORE_MAIN_PROMOTION.json"]:
    source=PARENT/"research/runs/native-permanent-strings-20261122"/name
    with (RECORD/("PARENT_"+name)).open("xb") as f:f.write(source.read_bytes())
with (RECORD/"CHECKPOINT_SOURCE.json").open("x",newline="\n") as f:
    json.dump(dict(base_main=git("rev-parse","HEAD"),paths=approved,source_manifest_sha256=sha(RECORD/"validation-02/SOURCE_FILES.json"),
        focused_result_sha256=sha(RECORD/"validation-02/VERIFICATION.json"),
        initial_completed_rows_sha256=sha(RECORD/"validation-01/RECOVERED_VERIFICATION.json"),
        stage="TESTED_COMPONENT_CLEAN_COMMITTED_CHECK_PENDING"),f,indent=2);f.write("\n")
records=git("ls-files","--others","--exclude-standard","--",RECORD.relative_to(ROOT).as_posix()).splitlines()
modified=git("diff","--name-only","--",RECORD.relative_to(ROOT).as_posix()).splitlines()
paths=sorted(set(approved+records+modified))
git("add","--",*paths)
assert sorted(git("diff","--cached","--name-only").splitlines())==paths
git("diff","--cached","--check")
for name in paths:
    assert subprocess.check_output(["git","show",":"+name],cwd=ROOT)==(ROOT/name).read_bytes(),name
print(git("commit","-m","Load original permanent strings through native records and worker ownership"),flush=True)
print("Committed source",git("rev-parse","HEAD"),"clean connected verification next",flush=True)
