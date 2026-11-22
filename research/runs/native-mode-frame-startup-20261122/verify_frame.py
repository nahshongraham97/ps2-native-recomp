"""Compile and test the actual native frame-storage binding and affected callers."""
from pathlib import Path
import ctypes,gzip,hashlib,json,os,re,subprocess,sys,time
RECORD=Path(__file__).resolve().parent;ROOT=RECORD.parents[2];PARENT=ROOT.parents[2]
OUT=RECORD/sys.argv[1];OUT.mkdir(exist_ok=False)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def git(*args):return subprocess.check_output(["git","-c","core.autocrlf=false",*args],cwd=ROOT,text=True).strip()
if "--clean" in sys.argv[2:]:assert not git("status","--porcelain","-uno")
new=["runtime/include/mka/retail_frame_work_storage.hpp","runtime/src/native/retail_frame_work_storage.cpp",
 "runtime/src/native/retail_frame_work_storage_main.cpp","tests/test_retail_frame_work_storage.cpp"]
changed=git("diff","--name-only","--","runtime","tests","cmake").splitlines()
for n in set(new+changed):assert b"\r" not in (ROOT/n).read_bytes(),n
subprocess.run(["git","-c","core.autocrlf=false","diff","--check","--","runtime","tests","cmake"],cwd=ROOT,check=True)
names=set(git("ls-files","runtime","tests","cmake","recomp","CMakeLists.txt").splitlines())|set(new)
pins={n:sha(ROOT/n) for n in sorted(names)}
def save(name,value):
    with (OUT/(name+".json")).open("x",newline="\n") as f:json.dump(value,f,indent=2);f.write("\n")
save("SOURCE_FILES",pins)
deps=PARENT.parent/"dependencies";compiler=deps/"compiler/llvm-mingw-20261006-ucrt-x86_64/bin"
cmake=deps/"cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe"
env=dict(os.environ);env["PATH"]=str(compiler)+os.pathsep+env["PATH"]
def run(name,argv,expected=0):
    argv=list(map(str,argv));start=time.monotonic()
    p=subprocess.run(argv,cwd=ROOT,env=env,capture_output=True,timeout=600,creationflags=subprocess.CREATE_NO_WINDOW)
    raw=p.stdout+p.stderr
    (OUT/(name+".log.gz")).write_bytes(gzip.compress(raw,mtime=0))
    save(name,dict(argv=argv,exit=p.returncode,seconds=time.monotonic()-start,
        executable_sha256=sha(Path(argv[0])),log_sha256=hashlib.sha256(raw).hexdigest(),
        source_manifest_sha256=sha(OUT/"SOURCE_FILES.json"),source_commit=git("rev-parse","HEAD")))
    print(name,p.returncode,raw.decode(errors="replace")[-2000:] if "symbols" not in name else "",flush=True)
    if expected is not None:assert p.returncode==expected,name
    return raw.decode(errors="replace")
build=ROOT/"b";buffer=ctypes.create_unicode_buffer(32768)
assert (build/"CMakeCache.txt").is_file()
assert ctypes.windll.kernel32.GetShortPathNameW(str(build),buffer,len(buffer))
empty=OUT/"fixtures/empty";empty.mkdir(parents=True)
assets=PARENT/"private-inputs/native-ps2-loose-20261121"
original=json.loads((ROOT/"research/runs/native-permanent-strings-20261122/ORIGINAL_STRING_EXPECTATIONS.json").read_text())
expected=OUT/"original-strings.tsv"
with expected.open("x",newline="\n") as f:
    for language,module in enumerate(original["modules"]):
        for v in module["values"]:f.write(f'{language}\t{v["index"]}\t{v["length"]}\t{v["sha256"]}\n')
plans=[("test_native_retail_frame_work_storage",[empty]),
       ("test_native_retail_permanent_strings",[assets,expected,OUT/"fixtures/strings"]),
       ("test_native_retail_loaded_system_art",[assets,OUT/"fixtures/font"]),
       ("test_native_retail_game_session",[])]
run("build",[cmake,"--build",buffer.value,"--target",*[n for n,_ in plans],"-j2"])
for n,args in plans:run(n,[build/(n+".exe"),*args])
prior=json.loads((ROOT/"research/runs/native-permanent-strings-20261122/validation-clean-03/VERIFICATION.json").read_text())["remaining_undefined_symbols"]
removed=[n for n in prior if "main_binding::initialize_retail_frame_work_buffers(" in n]
assert len(prior)==60 and len(removed)==1
log=run("full-entry-negative",[cmake,"--build",buffer.value,"--target","test_native_startup_missing_bindings","-j2"],None)
missing=re.findall(r"undefined symbol: ([^\r\n]+)",log)
assert len(missing)==len(set(missing))==59 and sorted(missing)==sorted(set(prior)-set(removed))
assert not (build/"test_native_startup_missing_bindings.exe").exists()
symbols=run("frame-symbols",[compiler/"llvm-nm.exe","-C",build/"test_native_retail_frame_work_storage.exe"])
assert not re.search(r"(?<![A-Za-z0-9_])(?:ps2|gs|spu2|sif|Cpu|recomp)::|\b(?:g_ram|g_spr)\b|SDL_OpenAudio",symbols)
assert re.search(r"\bT SDL_InitSubSystem\b",symbols) and "__imp_SDL_InitSubSystem" not in symbols
assert pins=={n:sha(ROOT/n) for n in pins}
save("VERIFICATION",dict(status="PASS_NATIVE_FRAME_WORK_STORAGE_COMPONENTS",source_commit=git("rev-parse","HEAD"),
    tracked_status=git("status","--porcelain","-uno"),clean_tracked_source=not git("status","--porcelain","-uno"),
    clients=[dict(name=n,sha256=sha(build/(n+".exe"))) for n,_ in plans],
    resolved_bindings=removed,remaining_undefined_symbols=missing,compiler_sha256=sha(compiler/"clang++.exe"),
    source_manifest_sha256=sha(OUT/"SOURCE_FILES.json"),cache_sha256=sha(build/"CMakeCache.txt"),
    verifier_sha256=sha(Path(__file__).resolve()),frame_client_gpu=False,font_client_hidden_gpu=True,
    visible_window=False,audio_endpoint=False,game_run=False,online="DEFERRED_REQUIRED_NAMES_UNRESOLVED",acceptance="OPEN"))
print("PASS four directly affected native clients/exact59 negative; no full game acceptance",flush=True)
