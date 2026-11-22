"""Authenticate mode results, preserve the reporting correction, and publish scoped changes."""
from pathlib import Path
import gzip,hashlib,json,re,subprocess,sys
RECORD=Path(__file__).resolve().parent;ROOT=RECORD.parents[2]
TEST="tests/test_retail_saved_profile_records.cpp"
APPROVED=["runtime/include/mka/retail_saved_profile_child.hpp","runtime/include/mka/retail_saved_profile_records.hpp",
 "runtime/src/native/retail_saved_profile_records.cpp","runtime/src/native/retail_saved_profile_bindings.cpp",TEST]
CLIENTS=["test_native_retail_saved_profile_records","test_native_retail_saved_profile_child",
 "test_native_retail_game_session","test_native_retail_profile_image","test_native_retail_profile_aggregate_main",
 "test_native_retail_profile_input_reset","test_native_retail_initial_reset_arcade","test_native_retail_main_frame_gates"]
OLD=b"child+240 payload/whole kart/gameplay remain required"
NEW=b"resolved startup fields; whole kart/time-writer/gameplay remain required"
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def git(*args):return subprocess.check_output(["git","-c","core.autocrlf=false",*args],cwd=ROOT,text=True).strip()
def save(name,value):
 with (RECORD/name).open("x",newline="\n") as f:json.dump(value,f,indent=2);f.write("\n")
def authenticate(folder,clean=False):
 out=RECORD/folder
 def read(n):return json.loads((out/(n+".json")).read_text())
 r=read("VERIFICATION");pins=read("SOURCE_FILES");manifest=sha(out/"SOURCE_FILES.json")
 assert r["status"]=="PASS_NATIVE_MODE_STARTUP_AND_RESOLVED_FIELDS"
 assert r["source_commit"]==git("rev-parse","HEAD") and r["source_manifest_sha256"]==manifest
 assert r["verifier_sha256"]==sha(RECORD/"verify_mode_fields.py")
 assert isinstance(r["clients"],list) and [c["name"] for c in r["clients"]]==CLIENTS and len(r["clients"])==8
 assert isinstance(r["resolved_bindings"],list) and len(r["resolved_bindings"])==1
 assert isinstance(r["remaining_undefined_symbols"],list) and len(r["remaining_undefined_symbols"])==58
 assert isinstance(r["mode_allocation_cuts"],int) and r["mode_allocation_cuts"]==64
 assert pins=={n:sha(ROOT/n) for n in pins}
 def row(n):
  v=read(n);raw=gzip.decompress((out/(n+".log.gz")).read_bytes())
  assert v["source_manifest_sha256"]==manifest and v["source_commit"]==r["source_commit"]
  assert hashlib.sha256(raw).hexdigest()==v["log_sha256"] and v["executable_sha256"]==sha(Path(v["argv"][0]))
  return v,raw.decode(errors="replace")
 assert row("build")[0]["exit"]==0
 for c in r["clients"]:
  v,text=row(c["name"]);assert v["exit"]==0 and v["executable_sha256"]==c["sha256"] and "PASS" in text
 v,text=row("full-entry-negative");assert v["exit"]!=0
 before=json.loads((RECORD/"validation-profile-clean-02/VERIFICATION.json").read_text())["remaining_undefined_symbols"]
 removed=[n for n in before if "main_binding::initialize_retail_mode_objects(" in n]
 missing=re.findall(r"undefined symbol: ([^\r\n]+)",text)
 assert len(before)==59 and len(removed)==1 and r["resolved_bindings"]==removed
 assert len(missing)==len(set(missing))==58 and sorted(missing)==sorted(set(before)-set(removed))
 assert r["remaining_undefined_symbols"]==missing
 v,text=row("mode-symbols");assert v["exit"]==0
 assert not re.search(r"(?<![A-Za-z0-9_])(?:ps2|gs|spu2|sif|Cpu|recomp)::|\b(?:g_ram|g_spr)\b|SDL_OpenAudio",text)
 assert re.search(r"\bT SDL_InitSubSystem\b",text) and "__imp_SDL_InitSubSystem" not in text
 assert not (ROOT/"b/test_native_startup_missing_bindings.exe").exists()
 assert r["source_field_contract_sha256"]==sha(RECORD/"MODE_FIELD_CONTRACT.json")
 assert isinstance(r["unchanged_codec_sources"],dict)
 prior=json.loads((RECORD/"validation-profile-clean-02/SOURCE_FILES.json").read_text())
 assert sorted(n for n in pins if pins[n]!=prior[n])==sorted(APPROVED)
 assert all(pins[n]==v==prior[n] for n,v in r["unchanged_codec_sources"].items())
 if clean:
  assert r["clean_tracked_source"] is True and r["tracked_status"]=="" and not git("status","--porcelain","-uno")
  for n,digest in pins.items():
   assert hashlib.sha256(subprocess.check_output(["git","show",r["source_commit"]+":"+n],cwd=ROOT)).hexdigest()==digest,n
 return dict(status="AUTHENTICATED_NATIVE_MODE_STARTUP_AND_FIELDS",runtime_source_commit=r["source_commit"],
  clean_committed_source=clean,verification_sha256=sha(out/"VERIFICATION.json"),source_manifest_sha256=manifest,
  clients=r["clients"],source_files=len(pins),mode_allocation_cuts=r["mode_allocation_cuts"],
  resolved_bindings=removed,remaining_undefined_symbols=missing,
  unchanged_codec_sources=r["unchanged_codec_sources"],numerical_gate="OPEN",hd_gate="OPEN",
  packed_time_arithmetic="UNADOPTED",whole_race="OPEN",oracle_comparison="OPEN",acceptance="OPEN")
def stage_and_commit(code,message):
 assert not git("diff","--cached","--name-only")
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
assert git("branch","--show-current")=="main"
action=sys.argv[1]
if action=="freeze":
 value=authenticate("validation-mode-01")
 raw=(ROOT/TEST).read_bytes();assert raw.count(OLD)==1
 with (RECORD/"test_records.before-mode-label.cpp.gz").open("xb") as f:f.write(gzip.compress(raw,mtime=0))
 save("MODE_PRECOMMIT_AUTHENTICATION.json",value)
 save("MODE_REPORTING_CORRECTION.json",dict(status="REPORTING_ONLY_CORRECTION_PENDING_COMPILE",
  measured_test_sha256=hashlib.sha256(raw).hexdigest(),old_text=OLD.decode(),new_text=NEW.decode(),
  assertions_changed=False,initial_component_results="PASS; stale second PASS label is preserved, not interpreted as current scope"))
 print("Preserved actual passing results and measured test bytes before label-only correction",flush=True)
elif action=="publish":
 auth=json.loads((RECORD/"MODE_PRECOMMIT_AUTHENTICATION.json").read_text())
 assert auth["runtime_source_commit"]==git("rev-parse","HEAD")=="98d5eba185d64eeecaa105dee6c7d8a3cb533259"
 pins=json.loads((RECORD/"validation-mode-01/SOURCE_FILES.json").read_text())
 raw=gzip.decompress((RECORD/"test_records.before-mode-label.cpp.gz").read_bytes())
 assert hashlib.sha256(raw).hexdigest()==pins[TEST] and raw.replace(OLD,NEW)==(ROOT/TEST).read_bytes()
 for n,digest in pins.items():
  if n!=TEST:assert sha(ROOT/n)==digest,n
 assert sorted(git("diff","--name-only","--","runtime","tests","cmake","CMakeLists.txt").splitlines())==sorted(APPROVED)
 stage_and_commit(APPROVED,"Connect real retail mode startup to resolved live profile fields")
 print("START clean committed verification: existing incremental build, eight headless clients",flush=True)
 subprocess.run([sys.executable,"-u",str(RECORD/"verify_mode_fields.py"),"validation-mode-clean-02","--clean"],cwd=ROOT,check=True)
 save("MODE_CLEAN_AUTHENTICATION.json",authenticate("validation-mode-clean-02",clean=True))
 print(git("push","origin","main"),flush=True)
 print("Published clean verified runtime",git("rev-parse","HEAD"),"; evidence commit next",flush=True)
elif action=="finish":
 auth=json.loads((RECORD/"MODE_CLEAN_AUTHENTICATION.json").read_text())
 assert auth["clean_committed_source"] is True and auth["runtime_source_commit"]==git("rev-parse","HEAD")
 assert auth["verification_sha256"]==sha(RECORD/"validation-mode-clean-02/VERIFICATION.json")
 assert not git("diff","--name-only","--","runtime","tests","cmake","CMakeLists.txt")
 stage_and_commit([],"Record clean mode-startup and live-field verification")
 print(git("push","origin","main"),flush=True)
 print("Published evidence",git("rev-parse","HEAD"),"runtime",auth["runtime_source_commit"],flush=True)
else:raise ValueError("Expected freeze, publish or finish")
