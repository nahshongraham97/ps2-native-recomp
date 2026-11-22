"""Compile connected profile changes; execute only affected headless native checks."""
from pathlib import Path
import ctypes,gzip,hashlib,json,os,re,subprocess,sys,time
RECORD=Path(__file__).resolve().parent;ROOT=RECORD.parents[2];PARENT=ROOT.parents[2]
OUT=RECORD/sys.argv[1];OUT.mkdir(exist_ok=False)
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def git(*args):return subprocess.check_output(["git","-c","core.autocrlf=false",*args],cwd=ROOT,text=True).strip()
def save(name,value):
 with (OUT/(name+".json")).open("x",newline="\n") as f:json.dump(value,f,indent=2);f.write("\n")
if "--clean" in sys.argv[2:]:assert not git("status","--porcelain","-uno")
changed=["runtime/include/mka/retail_saved_profile_records.hpp",
         "runtime/src/native/retail_saved_profile_records.cpp","tests/test_retail_saved_profile_records.cpp"]
for n in changed:assert b"\r" not in (ROOT/n).read_bytes(),n
git("diff","--check","--",*changed)
names=git("ls-files","runtime","tests","cmake","recomp","CMakeLists.txt").splitlines()
pins={n:sha(ROOT/n) for n in names};save("SOURCE_FILES",pins)
baseline="2baccd4599a7c74afbcb96e6ee89e580ee810a68"
prior_manifest=ROOT/"research/runs/native-mode-frame-startup-20261122/validation-frame-clean-02/SOURCE_FILES.json"
prior_pins=json.loads(prior_manifest.read_text())
original={}
for n in changed:
 raw=subprocess.check_output(["git","show",baseline+":"+n],cwd=ROOT)
 original[n]=hashlib.sha256(raw).hexdigest();assert original[n]==prior_pins[n],n
save("BASELINE_SOURCE",dict(commit=baseline,original_blobs=original,prior_manifest_sha256=sha(prior_manifest),
  preservation="Exact prior bytes remain in Git and the prior measured source manifest; no normalization/repin."))
codec=["runtime/include/mka/retail_profile_image.hpp","runtime/src/native/retail_profile_image.cpp",
       "runtime/include/mka/native_save_store.hpp","runtime/src/native/native_save_store.cpp",
       "runtime/include/mka/retail_saved_profile_child.hpp","runtime/src/native/retail_saved_profile_child.cpp"]
for n in codec:assert pins[n]==prior_pins[n],n
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
 print(name,p.returncode,raw.decode(errors="replace")[-2300:] if "symbols" not in name else "",flush=True)
 if expected is not None:assert p.returncode==expected,name
 return p.returncode,raw.decode(errors="replace")
build=ROOT/"b";assert (build/"CMakeCache.txt").is_file()
short=ctypes.create_unicode_buffer(32768);assert ctypes.windll.kernel32.GetShortPathNameW(str(build),short,len(short))
empty=OUT/"fixtures/empty";empty.mkdir(parents=True)
plans=[("test_native_retail_saved_profile_records",[OUT/"fixtures/records"]),
       ("test_native_retail_game_session",[]),("test_native_retail_profile_image",[]),
       ("test_native_retail_profile_aggregate_main",[empty]),("test_native_retail_profile_input_reset",[]),
       ("test_native_retail_initial_reset_arcade",[empty]),("test_native_retail_main_frame_gates",[empty])]
run("build",[cmake,"--build",short.value,"--target",*[n for n,_ in plans],"-j2"])
for name,args in plans:run(name,[build/(name+".exe"),*args])
negative,log=run("full-entry-negative",[cmake,"--build",short.value,"--target","test_native_startup_missing_bindings","-j2"],None)
prior_result=json.loads((ROOT/"research/runs/native-mode-frame-startup-20261122/validation-frame-clean-02/VERIFICATION.json").read_text())
missing=re.findall(r"undefined symbol: ([^\r\n]+)",log)
assert negative!=0 and len(missing)==len(set(missing))==59
assert sorted(missing)==sorted(prior_result["remaining_undefined_symbols"])
assert not (build/"test_native_startup_missing_bindings.exe").exists()
_,symbols=run("profile-symbols",[compiler/"llvm-nm.exe","-C",build/"test_native_retail_saved_profile_records.exe"])
assert not re.search(r"(?<![A-Za-z0-9_])(?:ps2|gs|spu2|sif|Cpu|recomp)::|\b(?:g_ram|g_spr)\b|SDL_OpenAudio",symbols)
assert re.search(r"\bT SDL_InitSubSystem\b",symbols) and "__imp_SDL_InitSubSystem" not in symbols
for name in ["NativeRetailMotorKombatProfiles::profile(","NativeRetailSavedProfileRecords::bind_motor_kombat_profiles(",
             "NativeRetailSavedProfileRecords::active_player_for_scope("]:assert name in symbols,name
assert pins=={n:sha(ROOT/n) for n in pins}
config={}
for line in (build/"CMakeCache.txt").read_text().splitlines():
 if any(line.startswith(k) for k in ["CMAKE_BUILD_TYPE:","CMAKE_CXX_COMPILER:","CMAKE_CXX_FLAGS","PS2_GENERATED:"]):
  key,value=line.split("=",1);config[key]=value
save("VERIFICATION",dict(status="PASS_NATIVE_PER_MODE_PROFILE_OWNERSHIP_COMPONENTS",
 source_commit=git("rev-parse","HEAD"),tracked_status=git("status","--porcelain","-uno"),
 clean_tracked_source=not git("status","--porcelain","-uno"),source_manifest_sha256=sha(OUT/"SOURCE_FILES.json"),
 clients=[dict(name=n,sha256=sha(build/(n+".exe"))) for n,_ in plans],
 remaining_undefined_symbols=missing,resolved_bindings=[],configuration=config,
 compiler_sha256=sha(compiler/"clang++.exe"),cache_sha256=sha(build/"CMakeCache.txt"),
 verifier_sha256=sha(Path(__file__).resolve()),unchanged_codec_sources={n:pins[n] for n in codec},
 status_cases=256,source_projection="E5AD8..E5BB5 whole-profile/status ownership only",
 startup_child_field_payload="OPEN",whole_mode_initialization="OPEN",loaded_profile_import="OPEN",
 numerical_gate="OPEN",hd_gate="OPEN",game_run=False,visible_window=False,audio_endpoint=False,
 oracle_comparison="OPEN",acceptance="OPEN"))
print("PASS seven affected native clients/exact unchanged59 negative; full game and startup field payload OPEN",flush=True)
