"""Validate exact scoped staging, commit and push the already-tested component."""
from pathlib import Path
import json,subprocess
from authenticate_frame import authenticate,git,sha,RECORD,ROOT
assert git("branch","--show-current")=="main"
assert git("rev-parse","HEAD")=="b0e79e7627f93b57370751235b06e4f044079a81"
assert not git("diff","--cached","--name-only")
approved=[
 "cmake/native_game.cmake","runtime/include/mka/retail_main.hpp",
 "runtime/src/native/retail_main_state.cpp","runtime/include/mka/retail_frame_work_storage.hpp",
 "runtime/src/native/retail_frame_work_storage.cpp","runtime/src/native/retail_frame_work_storage_main.cpp",
 "tests/test_retail_frame_work_storage.cpp"]
modified=git("diff","--name-only","--","runtime","tests","cmake","CMakeLists.txt").splitlines()
new=git("ls-files","--others","--exclude-standard","--","runtime","tests","cmake").splitlines()
assert sorted(set(modified+new))==sorted(approved)
assert all("/audio" not in n and "/sound" not in n and "/iop/" not in n for n in approved)
for n in approved:assert b"\r" not in (ROOT/n).read_bytes(),n
value=authenticate("validation-frame-01")
with (RECORD/"PRECOMMIT_AUTHENTICATION.json").open("x",newline="\n") as f:
    json.dump(value,f,indent=2);f.write("\n")
record=RECORD.relative_to(ROOT).as_posix()
records=git("ls-files","--others","--exclude-standard","--",record).splitlines()
assert all(n.startswith(record+"/") for n in records)
paths=sorted(approved+records)
git("add","--",*paths)
assert sorted(git("diff","--cached","--name-only").splitlines())==paths
git("diff","--cached","--check")
for n in paths:assert subprocess.check_output(["git","show",":"+n],cwd=ROOT)==(ROOT/n).read_bytes(),n
print(git("commit","-m","Initialize retail frame work on native Permanent backing and stable leases"),flush=True)
print(git("push","origin","main"),flush=True)
print("Published tested component",git("rev-parse","HEAD"),"; clean committed verification next",flush=True)
