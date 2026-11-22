"""Authenticate actual profile verifier rows and publish only approved authored paths."""
from pathlib import Path
import gzip,hashlib,json,re,subprocess,sys
RECORD=Path(__file__).resolve().parent;ROOT=RECORD.parents[2];PARENT=ROOT.parents[2]
APPROVED=["runtime/include/mka/retail_saved_profile_records.hpp",
          "runtime/src/native/retail_saved_profile_records.cpp","tests/test_retail_saved_profile_records.cpp"]
CLIENTS=["test_native_retail_saved_profile_records","test_native_retail_game_session",
         "test_native_retail_profile_image","test_native_retail_profile_aggregate_main",
         "test_native_retail_profile_input_reset","test_native_retail_initial_reset_arcade",
         "test_native_retail_main_frame_gates"]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def git(*args):return subprocess.check_output(["git","-c","core.autocrlf=false",*args],cwd=ROOT,text=True).strip()
def save(name,value):
 with (RECORD/name).open("x",newline="\n") as f:json.dump(value,f,indent=2);f.write("\n")
def authenticate(folder,clean=False):
 out=RECORD/folder
 def read(name):return json.loads((out/(name+".json")).read_text())
 result=read("VERIFICATION");manifest=sha(out/"SOURCE_FILES.json")
 assert result["status"]=="PASS_NATIVE_PER_MODE_PROFILE_OWNERSHIP_COMPONENTS"
 assert result["source_commit"]==git("rev-parse","HEAD")
 assert result["source_manifest_sha256"]==manifest
 assert isinstance(result["clients"],list) and [c["name"] for c in result["clients"]]==CLIENTS
 assert len(result["clients"])==7 and result["status_cases"]==256
 assert isinstance(result["resolved_bindings"],list) and result["resolved_bindings"]==[]
 assert isinstance(result["remaining_undefined_symbols"],list) and len(result["remaining_undefined_symbols"])==59
 assert result["verifier_sha256"]==sha(RECORD/"verify_profile_ownership.py")
 pins=read("SOURCE_FILES");assert pins=={n:sha(ROOT/n) for n in pins}
 prior=ROOT/"research/runs/native-mode-frame-startup-20261122/validation-frame-clean-02"
 prior_pins=json.loads((prior/"SOURCE_FILES.json").read_text())
 assert pins.keys()==prior_pins.keys()
 assert sorted(n for n in pins if pins[n]!=prior_pins[n])==sorted(APPROVED)
 assert isinstance(result["unchanged_codec_sources"],dict)
 assert all(pins[n]==digest==prior_pins[n] for n,digest in result["unchanged_codec_sources"].items())
 def row(name):
  value=read(name);raw=gzip.decompress((out/(name+".log.gz")).read_bytes())
  assert value["source_commit"]==result["source_commit"] and value["source_manifest_sha256"]==manifest
  assert isinstance(value["exit"],int) and hashlib.sha256(raw).hexdigest()==value["log_sha256"]
  assert sha(Path(value["argv"][0]))==value["executable_sha256"]
  return value,raw.decode(errors="replace")
 assert row("build")[0]["exit"]==0
 for client in result["clients"]:
  value,text=row(client["name"])
  assert value["exit"]==0 and value["executable_sha256"]==client["sha256"] and "PASS" in text
 value,negative=row("full-entry-negative");assert value["exit"]!=0
 missing=re.findall(r"undefined symbol: ([^\r\n]+)",negative)
 old=json.loads((prior/"VERIFICATION.json").read_text())["remaining_undefined_symbols"]
 assert len(missing)==len(set(missing))==59 and sorted(missing)==sorted(old)
 assert result["remaining_undefined_symbols"]==missing
 value,symbols=row("profile-symbols");assert value["exit"]==0
 assert not re.search(r"(?<![A-Za-z0-9_])(?:ps2|gs|spu2|sif|Cpu|recomp)::|\b(?:g_ram|g_spr)\b|SDL_OpenAudio",symbols)
 assert re.search(r"\bT SDL_InitSubSystem\b",symbols) and "__imp_SDL_InitSubSystem" not in symbols
 assert result["cache_sha256"]==sha(ROOT/"b/CMakeCache.txt")
 assert not (ROOT/"b/test_native_startup_missing_bindings.exe").exists()
 data=json.loads((RECORD/"PROFILE_OWNERSHIP_DATA.json").read_text())
 image=(PARENT/"private-inputs/xbox-mka-20261121/default.xbe").read_bytes()
 assert hashlib.sha256(image).hexdigest()==data["xbe_sha256"]
 for n,digest in data["evidence_sha256"].items():assert sha(RECORD/n)==digest,n
 m=data["projection_mapping"]
 assert image[m["file_offset"]:m["file_offset"]+m["length"]].hex()==data["projection_bytes_hex"]
 if clean:
  assert result["clean_tracked_source"] is True and result["tracked_status"]==""
  assert not git("status","--porcelain","-uno")
  for n,digest in pins.items():
   raw=subprocess.check_output(["git","show",result["source_commit"]+":"+n],cwd=ROOT)
   assert hashlib.sha256(raw).hexdigest()==digest,n
 return dict(status="AUTHENTICATED_NATIVE_PER_MODE_PROFILE_OWNERSHIP",runtime_source_commit=result["source_commit"],
  clean_committed_source=clean,verification_sha256=sha(out/"VERIFICATION.json"),source_manifest_sha256=manifest,
  clients=result["clients"],source_files=len(pins),unchanged_codec_sources=result["unchanged_codec_sources"],
  profile_source_data_sha256=sha(RECORD/"PROFILE_OWNERSHIP_DATA.json"),resolved_bindings=[],
  remaining_undefined_symbols=missing,startup_child_field_payload="OPEN",whole_mode_initialization="OPEN",
  numerical_gate="OPEN",hd_gate="OPEN",oracle_comparison="OPEN",game_run=False,acceptance="OPEN")
assert git("branch","--show-current")=="main"
assert not git("diff","--cached","--name-only")
action=sys.argv[1]
if action=="authenticate-clean":
 save("PROFILE_CLEAN_AUTHENTICATION.json",authenticate("validation-profile-clean-02",clean=True))
 print("Authenticated clean committed per-mode ownership and all seven exact client/log identities",flush=True)
 sys.exit(0)
if action=="checkpoint":
 assert git("rev-parse","HEAD")=="2baccd4599a7c74afbcb96e6ee89e580ee810a68"
 assert sorted(git("diff","--name-only","--","runtime","tests","cmake","CMakeLists.txt").splitlines())==sorted(APPROVED)
 assert not git("ls-files","--others","--exclude-standard","--","runtime","tests","cmake")
 save("PROFILE_PRECOMMIT_AUTHENTICATION.json",authenticate("validation-profile-01"))
 code=APPROVED
 message="Reconcile same-Session four-player kart profile ownership without changing PS2 saves"
elif action=="finish":
 auth=json.loads((RECORD/"PROFILE_CLEAN_AUTHENTICATION.json").read_text())
 assert auth["clean_committed_source"] is True and auth["runtime_source_commit"]==git("rev-parse","HEAD")
 assert auth["verification_sha256"]==sha(RECORD/"validation-profile-clean-02/VERIFICATION.json")
 assert not git("diff","--name-only","--","runtime","tests","cmake","CMakeLists.txt")
 code=[];message="Record clean per-mode profile owner verification and remaining payload gate"
else:raise ValueError("Expected checkpoint, authenticate-clean or finish")
record=RECORD.relative_to(ROOT).as_posix()
paths=sorted(set(code+git("ls-files","--others","--exclude-standard","--",record).splitlines()+
                 git("diff","--name-only","--",record).splitlines()))
assert paths and all(n in code or n.startswith(record+"/") for n in paths)
assert not any("/private-kart-modules/" in n or Path(n).suffix in {".mko",".ssf",".bin",".exe"} for n in paths)
for n in paths:
 if Path(n).suffix in {".py",".java",".md",".json",".asm",".tsv",".txt",".hpp",".cpp"}:
  assert b"\r" not in (ROOT/n).read_bytes(),n
git("add","--",*paths)
assert sorted(git("diff","--cached","--name-only").splitlines())==paths
git("diff","--cached","--check")
for n in paths:assert subprocess.check_output(["git","show",":"+n],cwd=ROOT)==(ROOT/n).read_bytes(),n
print(git("commit","-m",message).splitlines()[0],flush=True)
print(git("push","origin","main"),flush=True)
print("Published",git("rev-parse","HEAD"),flush=True)
