"""Verify actual mode startup and live resolved fields on the connected native owner."""
from pathlib import Path
import ctypes,gzip,hashlib,json,os,re,subprocess,sys,time
RECORD=Path(__file__).resolve().parent;ROOT=RECORD.parents[2];PARENT=ROOT.parents[2]
OUT=RECORD/sys.argv[1];OUT.mkdir(exist_ok=False)
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def git(*args):return subprocess.check_output(["git","-c","core.autocrlf=false",*args],cwd=ROOT,text=True).strip()
def save(name,value):
 with (OUT/(name+".json")).open("x",newline="\n") as f:json.dump(value,f,indent=2);f.write("\n")
if "--clean" in sys.argv[2:]:assert not git("status","--porcelain","-uno")
changed=["runtime/include/mka/retail_saved_profile_child.hpp","runtime/include/mka/retail_saved_profile_records.hpp",
 "runtime/src/native/retail_saved_profile_records.cpp","runtime/src/native/retail_saved_profile_bindings.cpp",
 "tests/test_retail_saved_profile_records.cpp"]
for n in changed:assert b"\r" not in (ROOT/n).read_bytes(),n
git("diff","--check","--",*changed)
names=git("ls-files","runtime","tests","cmake","recomp","CMakeLists.txt").splitlines()
pins={n:sha(ROOT/n) for n in names};save("SOURCE_FILES",pins)
prior=RECORD/"validation-profile-clean-02";prior_pins=json.loads((prior/"SOURCE_FILES.json").read_text())
assert pins.keys()==prior_pins.keys()
assert sorted(n for n in pins if pins[n]!=prior_pins[n])==sorted(changed)
codec=["runtime/include/mka/retail_profile_image.hpp","runtime/src/native/retail_profile_image.cpp",
       "runtime/include/mka/native_save_store.hpp","runtime/src/native/native_save_store.cpp",
       "runtime/src/native/retail_saved_profile_child.cpp"]
for n in codec:assert pins[n]==prior_pins[n],n
baseline="98d5eba185d64eeecaa105dee6c7d8a3cb533259";original={}
for n in changed:
 raw=subprocess.check_output(["git","show",baseline+":"+n],cwd=ROOT)
 original[n]=hashlib.sha256(raw).hexdigest();assert original[n]==prior_pins[n],n
save("BASELINE_SOURCE",dict(commit=baseline,original_blobs=original,
 prior_source_manifest_sha256=sha(prior/"SOURCE_FILES.json"),preservation="Exact old bytes retained in Git/prior measured manifest."))
data=json.loads((RECORD/"MODE_FIELD_CONTRACT.json").read_text())
assert data["native_projection"]["motor_counters"]=="statistics.mode_pairs[2]"
assert data["native_projection"]["motor_results"]=="statistics.timed_results[0..4]"
deps=PARENT.parent/"dependencies";compiler=deps/"compiler/llvm-mingw-20261006-ucrt-x86_64/bin"
cmake=deps/"cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe"
env=dict(os.environ);env["PATH"]=str(compiler)+os.pathsep+env["PATH"]
def run(name,argv,expected=0):
 argv=list(map(str,argv));start=time.monotonic()
 p=subprocess.run(argv,cwd=ROOT,env=env,capture_output=True,timeout=600,creationflags=subprocess.CREATE_NO_WINDOW)
 raw=p.stdout+p.stderr
 with (OUT/(name+".log.gz")).open("xb") as f:f.write(gzip.compress(raw,mtime=0))
 save(name,dict(argv=argv,exit=p.returncode,seconds=time.monotonic()-start,executable_sha256=sha(Path(argv[0])),
  log_sha256=hashlib.sha256(raw).hexdigest(),source_manifest_sha256=sha(OUT/"SOURCE_FILES.json"),
  source_commit=git("rev-parse","HEAD")))
 print(name,p.returncode,raw.decode(errors="replace")[-2400:] if "symbols" not in name else "",flush=True)
 if expected is not None:assert p.returncode==expected,name
 return p.returncode,raw.decode(errors="replace")
build=ROOT/"b";assert (build/"CMakeCache.txt").is_file()
short=ctypes.create_unicode_buffer(32768);assert ctypes.windll.kernel32.GetShortPathNameW(str(build),short,len(short))
empty=OUT/"fixtures/empty";empty.mkdir(parents=True)
plans=[("test_native_retail_saved_profile_records",[OUT/"fixtures/records"]),
 ("test_native_retail_saved_profile_child",[]),("test_native_retail_game_session",[]),
 ("test_native_retail_profile_image",[]),("test_native_retail_profile_aggregate_main",[empty]),
 ("test_native_retail_profile_input_reset",[]),("test_native_retail_initial_reset_arcade",[empty]),
 ("test_native_retail_main_frame_gates",[empty])]
run("build",[cmake,"--build",short.value,"--target",*[n for n,_ in plans],"-j2"])
cuts=None
for name,args in plans:
 _,text=run(name,[build/(name+".exe"),*args])
 if name=="test_native_retail_saved_profile_records":
  found=re.findall(r"counter wrap and(\d+) allocation cuts",text);assert len(found)==1
  cuts=int(found[0]);assert 4<=cuts<128
negative,text=run("full-entry-negative",[cmake,"--build",short.value,"--target","test_native_startup_missing_bindings","-j2"],None)
before=json.loads((prior/"VERIFICATION.json").read_text())["remaining_undefined_symbols"]
removed=[n for n in before if "main_binding::initialize_retail_mode_objects(" in n]
missing=re.findall(r"undefined symbol: ([^\r\n]+)",text)
assert len(before)==59 and len(removed)==1 and negative!=0
assert len(missing)==len(set(missing))==58 and sorted(missing)==sorted(set(before)-set(removed))
assert not (build/"test_native_startup_missing_bindings.exe").exists()
_,symbols=run("mode-symbols",[compiler/"llvm-nm.exe","-C",build/"test_native_retail_saved_profile_records.exe"])
assert not re.search(r"(?<![A-Za-z0-9_])(?:ps2|gs|spu2|sif|Cpu|recomp)::|\b(?:g_ram|g_spr)\b|SDL_OpenAudio",symbols)
assert re.search(r"\bT SDL_InitSubSystem\b",symbols) and "__imp_SDL_InitSubSystem" not in symbols
for n in ["main_binding::initialize_retail_mode_objects(","NativeRetailPublishedProfileFields::motor_statistics(",
 "NativeRetailSavedProfileRecords::initialize_mode_objects(","NativeRetailSavedProfileRecords::increment_motor_counter("]:
 assert n in symbols,n
assert pins=={n:sha(ROOT/n) for n in pins}
configuration={}
for line in (build/"CMakeCache.txt").read_text().splitlines():
 if any(line.startswith(k) for k in ["CMAKE_BUILD_TYPE:","CMAKE_CXX_COMPILER:","CMAKE_CXX_FLAGS","PS2_GENERATED:"]):
  k,v=line.split("=",1);configuration[k]=v
save("VERIFICATION",dict(status="PASS_NATIVE_MODE_STARTUP_AND_RESOLVED_FIELDS",
 source_commit=git("rev-parse","HEAD"),tracked_status=git("status","--porcelain","-uno"),
 clean_tracked_source=not git("status","--porcelain","-uno"),source_manifest_sha256=sha(OUT/"SOURCE_FILES.json"),
 clients=[dict(name=n,sha256=sha(build/(n+".exe"))) for n,_ in plans],
 resolved_bindings=removed,remaining_undefined_symbols=missing,mode_allocation_cuts=cuts,
 source_field_contract_sha256=sha(RECORD/"MODE_FIELD_CONTRACT.json"),
 unchanged_codec_sources={n:pins[n] for n in codec},configuration=configuration,
 compiler_sha256=sha(compiler/"clang++.exe"),cache_sha256=sha(build/"CMakeCache.txt"),
 verifier_sha256=sha(Path(__file__).resolve()),startup_field_contract="SOURCE_RESOLVED_AND_COMPONENT_TESTED",
 whole_race="OPEN",packed_time_arithmetic="UNADOPTED",loaded_profile_import="OPEN",
 numerical_gate="OPEN",hd_gate="OPEN",oracle_comparison="OPEN",
 visible_window=False,audio_endpoint=False,game_run=False,acceptance="OPEN"))
print("PASS eight affected clients; exact59->58 resolves only real Main mode-object startup; whole game OPEN",flush=True)
