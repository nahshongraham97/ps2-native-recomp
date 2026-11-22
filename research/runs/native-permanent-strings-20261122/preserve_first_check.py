"""Preserve the first measured implementation before a concrete lifetime correction."""
from pathlib import Path
import gzip
import hashlib
import json
import shutil
import subprocess
RECORD=Path(__file__).resolve().parent;ROOT=RECORD.parents[2]
pins=json.loads((RECORD/"validation-01/SOURCE_FILES.json").read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
assert pins=={n:sha(ROOT/n) for n in pins}
changed=subprocess.check_output(["git","-c","core.autocrlf=false","diff","--name-only","--","runtime","tests","cmake"],cwd=ROOT,text=True).splitlines()
new=["runtime/include/mka/retail_permanent_string_data.hpp","runtime/src/native/retail_permanent_string_data.cpp",
     "runtime/include/mka/retail_permanent_strings.hpp","runtime/src/native/retail_permanent_strings.cpp","tests/test_retail_permanent_strings.cpp"]
out=RECORD/"before-lifetime-02";out.mkdir(exist_ok=False)
for name in sorted(set(changed+new)):
    (out/(name.replace("/","__")+".gz")).write_bytes(gzip.compress((ROOT/name).read_bytes(),mtime=0))
history=ROOT/"b/history-strings-01";history.mkdir(exist_ok=False)
binary_pins={}
for name in ["test_native_retail_permanent_strings","test_native_retail_game_resource_registry",
 "test_native_retail_loaded_system_art","test_native_retail_game_session",
 "test_native_retail_saved_profile_records","test_native_retail_initial_reset_arcade"]:
    path=ROOT/"b"/(name+".exe");shutil.copyfile(path,history/path.name);binary_pins[name]=sha(path)
with (out/"PRESERVED.json").open("x",newline="\n") as f:
    json.dump(dict(binaries=binary_pins,history=str(history),
        source_manifest_sha256=sha(RECORD/"validation-01/SOURCE_FILES.json"),
        reason="Actual lifetime declaration shows a still-retained retired witness does not establish that raw Main remains alive."),f,indent=2);f.write("\n")
print("Preserved first source bytes and six measured binaries",flush=True)
