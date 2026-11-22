"""Authenticate completed client rows after a reporting-wrapper deadline; never rerun them."""
from pathlib import Path
import gzip
import hashlib
import json
import re
import shutil
import subprocess
import sys
RECORD=Path(__file__).resolve().parent;ROOT=RECORD.parents[2];PARENT=ROOT.parents[2]
OUT=RECORD/sys.argv[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def git(*args):return subprocess.check_output(["git","-c","core.autocrlf=false",*args],cwd=ROOT,text=True).strip()
def read(name):return json.loads((OUT/(name+".json")).read_text())
def log(name):
    raw=gzip.decompress((OUT/(name+".log.gz")).read_bytes())
    assert hashlib.sha256(raw).hexdigest()==read(name)["log_sha256"],name
    return raw.decode(errors="replace")
pins=read("SOURCE_FILES");manifest_sha=sha(OUT/"SOURCE_FILES.json")
assert pins=={n:sha(ROOT/n) for n in pins}
names=["test_native_retail_permanent_strings","test_native_retail_game_resource_registry",
 "test_native_retail_loaded_system_art","test_native_retail_game_session",
 "test_native_retail_saved_profile_records","test_native_retail_initial_reset_arcade"]
clients=[]
for name in names:
    result=read(name)
    assert result["exit"]==0 and result["source_manifest_sha256"]==manifest_sha
    assert result["executable_sha256"]==sha(ROOT/"b"/(name+".exe"))
    log(name);clients.append(dict(name=name,sha256=result["executable_sha256"]))
prior=json.loads((ROOT/"research/runs/native-loaded-font-20261122/validation-owner-clean-03/VERIFICATION.json").read_text())["remaining_undefined_symbols"]
resolved=[n for n in prior if "loaded_art_required::queue_permanent_strings_215660(" in n or "loaded_art_required::complete_permanent_strings_215730(" in n]
missing=re.findall(r"undefined symbol: ([^\r\n]+)",log("full-entry-negative"))
assert read("full-entry-negative")["exit"]!=0 and len(resolved)==2
assert len(missing)==len(set(missing))==60 and sorted(missing)==sorted(set(prior)-set(resolved))
symbols=log("permanent-symbols")
assert read("permanent-symbols")["exit"]==0
assert not re.search(r"(?<![A-Za-z0-9_])(?:ps2|gs|spu2|sif|Cpu|recomp)::|\b(?:g_ram|g_spr)\b|SDL_OpenAudio",symbols)
assert re.search(r"\bT SDL_InitSubSystem\b",symbols) and "__imp_SDL_InitSubSystem" not in symbols
assert not (ROOT/"b/test_native_startup_missing_bindings.exe").exists()
asset=PARENT/"private-inputs/native-ps2-loose-20261121/art/gameart.ssf"
assert sha(asset)=="9900120ff8782b16e0d73ee885c8f4df6c82545b915dd10105e6694a03658a39"
with (RECORD/"verify.before-wrapper-correction.py.gz").open("xb") as f:
    f.write(gzip.compress((RECORD/"verify.py").read_bytes(),mtime=0))
with (OUT/"WRAPPER_TIMEOUT.json").open("x",newline="\n") as f:
    json.dump(dict(status="HOST_WRAPPER_TIMEOUT_AFTER_COMPLETED_ROWS",host_timeout_ms=120000,
        host_exit=143,component_failures=False,summary_missing=True,
        scope="Compile, six client rows, exact60 negative and nm all completed; this read-only recovery does not rerun them."),f,indent=2);f.write("\n")
with (OUT/"RECOVERED_VERIFICATION.json").open("x",newline="\n") as f:
    json.dump(dict(status="AUTHENTICATED_COMPLETED_PERMANENT_STRING_COMPONENT_ROWS",
        source_commit=git("rev-parse","HEAD"),tracked_status=git("status","--porcelain","-uno"),
        clients=clients,original_strings=390,source_manifest_sha256=manifest_sha,
        original_gameart_sha256=sha(asset),resolved_bindings=resolved,remaining_undefined_symbols=missing,
        wrapper_status="HOST_WRAPPER_TIMEOUT",rerun=False,game_run=False,acceptance="OPEN"),f,indent=2);f.write("\n")
print("Authenticated six completed component rows and exact60 negative; wrapper timeout remains separately recorded",flush=True)
