"""Authenticate clean committed results, retain prior evidence, then commit/push only this record."""
from pathlib import Path
import gzip
import hashlib
import json
import subprocess
RECORD=Path(__file__).resolve().parent;ROOT=RECORD.parents[2];PARENT=ROOT.parents[2]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def git(*args):return subprocess.check_output(["git","-c","core.autocrlf=false",*args],cwd=ROOT,text=True).strip()
def save(path,value):
    with path.open("x",newline="\n") as f:json.dump(value,f,indent=2);f.write("\n")
assert git("branch","--show-current")=="main"
assert not git("diff","--cached","--name-only")
assert not git("diff","--name-only","--","runtime","tests","cmake","CMakeLists.txt")
out=RECORD/"validation-clean-03";result=json.loads((out/"VERIFICATION.json").read_text())
assert result["status"]=="PASS_NATIVE_PERMANENT_STRINGS_COMPONENTS"
assert result["clean_tracked_source"] is True and result["tracked_status"]==""
assert result["source_commit"]==git("rev-parse","HEAD")
expected=["test_native_retail_permanent_strings","test_native_retail_game_resource_registry",
 "test_native_retail_loaded_system_art","test_native_retail_game_session",
 "test_native_retail_saved_profile_records","test_native_retail_initial_reset_arcade"]
assert isinstance(result["clients"],list) and [c["name"] for c in result["clients"]]==expected
assert len(result["clients"])==6 and len(result["resolved_bindings"])==2
assert isinstance(result["remaining_undefined_symbols"],list) and len(result["remaining_undefined_symbols"])==60
assert sha(RECORD/"verify.py")==result["verification_tool_sha256"]
pins=json.loads((out/"SOURCE_FILES.json").read_text())
assert pins=={name:sha(ROOT/name) for name in pins}
committed={}
for name in pins:
    raw=subprocess.check_output(["git","show","HEAD:"+name],cwd=ROOT)
    assert raw==(ROOT/name).read_bytes(),name
    committed[name]=hashlib.sha256(raw).hexdigest()
for client in result["clients"]:
    name=client["name"];row=json.loads((out/(name+".json")).read_text())
    assert row["exit"]==0 and row["source_commit"]==result["source_commit"]
    assert row["executable_sha256"]==client["sha256"]==sha(ROOT/"b"/(name+".exe"))
    assert row["source_manifest_sha256"]==result["source_manifest_sha256"]
    raw=gzip.decompress((out/(name+".log.gz")).read_bytes())
    assert hashlib.sha256(raw).hexdigest()==row["log_sha256"]
asset=PARENT/"private-inputs/native-ps2-loose-20261121/art/gameart.ssf"
assert sha(asset)==result["original_gameart_sha256"]
assert not (ROOT/"b/test_native_startup_missing_bindings.exe").exists()
modified=git("diff","--name-only","f6a1aae55531f9133ac66b2a9bb46f3e9f6c54ce","HEAD","--","runtime","tests","cmake").splitlines()
assert all("/audio" not in n and "/sound" not in n and "/iop/" not in n for n in modified)
promotion=json.loads((RECORD/"PARENT_BEFORE_MAIN_PROMOTION.json").read_text())
old=Path(promotion["fork_root"])
assert promotion["dirty_pins"]=={n:sha(old/n) for n in promotion["dirty_pins"]}
old_git=subprocess.check_output(["git","rev-parse","--absolute-git-dir"],cwd=old,text=True).strip()
assert sha(Path(old_git)/"index")==promotion["index_sha256"]
save(RECORD/"CLEAN_AUTHENTICATION.json",dict(status="AUTHENTICATED_CLEAN_PERMANENT_STRING_COMPONENTS",
    runtime_source_commit=result["source_commit"],verification_sha256=sha(out/"VERIFICATION.json"),
    committed_source_sha256=committed,clients=result["clients"],
    original_gameart_sha256=sha(asset),original_expectations_sha256=sha(RECORD/"ORIGINAL_STRING_EXPECTATIONS.json"),
    preserved_dirty_draft_and_index_unchanged=True,audio_source_unchanged=True,game_acceptance="OPEN"))
summary=("Permanent-string source"+result["source_commit"][:7]+" passes all six directly affected clients freshly "
 "on a clean commit. Actual215660/215730 bindings, worker/record publication, source2170E0/216D80 data-only linking, "
 "215ED0 string lookup and215010/217FC0 retirement are implemented for the five original permanent-string modules "
 "and all390 original text values. Null/empty, malformed, allocation, tag order, global precedence, generation, "
 "Package/Main/escaped-owner lifetime checks pass. No MKO interpreter, guest pointer map, English fallback or "
 "successful parsed fixture is used. The60-name full-entry negative remains diagnostic; complete game, physical "
 "platform/input/audio, oracle comparison, general MKO functions/imports/rebind and acceptance remain OPEN.")
with (RECORD/"HANDOFF.md").open("x",newline="\n") as f:
    f.write("# Permanent-string implementation on main\n\n"+summary+"\n\n"
      "Active main worktree: `external/forks/retail-main-22/`. The older dirty primary remains unchanged on "
      "`preserved-main-draft-20261122`; do not overwrite or treat it as current main.\n\n"
      "## Source and verification\n\n"
      "- `SOURCE.md`, original descriptor/expectation records and private full-word joins pin the input and behavior.\n"
      "- `validation-clean-03/VERIFICATION.json` and `CLEAN_AUTHENTICATION.json` pin clean source, six binary/log "
      "identities, input bytes and the exact60 diagnostic names.\n"
      "- `validation-01/WRAPPER_TIMEOUT.json` preserves the host reporting failure after completed client rows; "
      "its authenticated rows are separate. The first implementation and binaries remain preserved before "
      "the retired-witness cleanup correction; `validation-02/` verifies that correction.\n"
      "- The new string client opens no window/GPU/audio. Only the existing never-visible font regression "
      "client uses its explicit WGL host. No game is launched.\n\n"
      "Owner: no action. Agent next: continue the earliest remaining connected startup obligations "
      "(`read_retail_online_boot_word` / `initialize_retail_online_stage`), then mode/scene/frame effects. "
      "No background task remains after publication.\n")
for name in ["WORK.md","SOURCE.md"]:
    path=RECORD/name;raw=path.read_bytes()
    with (RECORD/(name+".before-clean-publication.gz")).open("xb") as f:f.write(gzip.compress(raw,mtime=0))
    text=raw.decode().replace("\r\n","\n")
    if name=="SOURCE.md":
        text=text.replace("Clean committed verification still remains required.",
            "Clean committed source"+result["source_commit"][:7]+" now passes all six clients in validation-clean-03.")
    else:
        text=text.replace("Current stage: original-source recovery; no new implementation/build/game run.",
            "Current stage: clean compiled/tested-component permanent-string implementation; no game run.")
        text+="\n"+summary+"\nOwner: no action. Agent next: earliest remaining connected startup; no active background run.\n"
    path.write_text(text,encoding="utf-8",newline="\n")
record=RECORD.relative_to(ROOT).as_posix()
paths=sorted(set(git("ls-files","--others","--exclude-standard","--",record).splitlines()+
                 git("diff","--name-only","--",record).splitlines()))
assert paths and all(n.startswith(record+"/") for n in paths)
git("add","--",*paths)
assert sorted(git("diff","--cached","--name-only").splitlines())==paths
git("diff","--cached","--check")
for name in paths:assert subprocess.check_output(["git","show",":"+name],cwd=ROOT)==(ROOT/name).read_bytes(),name
print(git("commit","-m","Record clean permanent-string producer and lifecycle verification"),flush=True)
print(git("push","origin","main"),flush=True)
print("Published fork main",git("rev-parse","HEAD"),"runtime source",result["source_commit"],flush=True)
