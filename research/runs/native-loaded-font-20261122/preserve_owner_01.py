"""Preserve the failed first owner check before correcting source/test expectations."""
from pathlib import Path
import gzip
import hashlib
import json
import shutil
RECORD=Path(__file__).resolve().parent
ROOT=RECORD.parents[2]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
pins=json.loads((RECORD/"validation-owner-01/SOURCE_FILES.json").read_text())
assert pins=={n:sha(ROOT/n) for n in pins}
out=RECORD/"before-owner-02";out.mkdir(exist_ok=False)
paths=json.loads((RECORD/"OWNER_PATHS.json").read_text())["paths"]
for name in paths:
    (out/(name.replace("/","__")+".gz")).write_bytes(gzip.compress((ROOT/name).read_bytes(),mtime=0))
history=ROOT/"b/history-owner-01";history.mkdir(exist_ok=False)
binary_pins={}
for name in ["test_native_retail_loaded_system_art","test_native_retail_system_art_cpu",
             "test_native_retail_game_session","test_native_retail_initial_reset_arcade"]:
    path=ROOT/"b"/(name+".exe")
    shutil.copyfile(path,history/path.name);binary_pins[name]=sha(path)
with (out/"PRESERVED.json").open("x",newline="\n") as f:
    json.dump(dict(first_result="FAIL_TEST_LAST_OWNER_EXPECTATION",binaries=binary_pins,private_history=str(history),
        source_manifest_sha256=sha(RECORD/"validation-owner-01/SOURCE_FILES.json"),
        cause="NativeRetailGameRecordReset::session_ is a real strong owner; clearing game_session alone is not final Session release.",
        original_failure_receipt="validation-owner-01/test_native_retail_loaded_system_art.json"),f,indent=2);f.write("\n")
print("Preserved exact first sources and four private binaries; failed ownership expectation retained")
