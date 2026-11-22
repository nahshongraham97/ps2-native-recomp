"""Freeze the authored owner batch as LF; preserve any prior raw newline bytes."""
from pathlib import Path
import gzip
import hashlib
import json
import subprocess
RECORD=Path(__file__).resolve().parent
ROOT=RECORD.parents[2]
changed=subprocess.check_output(["git","diff","--name-only","--","runtime","tests","cmake"],cwd=ROOT,text=True).splitlines()
new=["runtime/include/mka/retail_loaded_system_art.hpp","runtime/src/native/retail_loaded_system_art.cpp",
     "runtime/src/native/retail_loaded_system_art_main.cpp","tests/test_retail_loaded_system_art.cpp"]
paths=sorted(set(changed+new))
preserved={}
for name in paths:
    path=ROOT/name;raw=path.read_bytes()
    if b"\r" in raw:
        target=RECORD/("before-owner-LF-"+name.replace("/","__")+".gz")
        with target.open("xb") as f:f.write(gzip.compress(raw,mtime=0))
        normalized=raw.replace(b"\r\n",b"\n");assert b"\r" not in normalized
        path.write_bytes(normalized)
        preserved[name]=dict(before_sha256=hashlib.sha256(raw).hexdigest(),after_sha256=hashlib.sha256(normalized).hexdigest(),preserved=target.name)
with (RECORD/"OWNER_PATHS.json").open("x",newline="\n") as f:
    json.dump(dict(paths=paths,newline_pairs=preserved,phase="IMPLEMENTED_UNBUILT"),f,indent=2);f.write("\n")
subprocess.run(["git","-c","core.autocrlf=false","diff","--check","--",*paths],cwd=ROOT,check=True)
print("Frozen",len(paths),"authored owner paths; explicit LF and scoped diff check pass")
