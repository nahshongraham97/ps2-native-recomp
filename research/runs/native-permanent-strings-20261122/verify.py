"""Build/check real permanent strings and directly affected native callers."""
from pathlib import Path
import ctypes
import gzip
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time
RECORD=Path(__file__).resolve().parent
ROOT=RECORD.parents[2];PARENT=ROOT.parents[2]
OUT=RECORD/sys.argv[1];OUT.mkdir(exist_ok=False)
clean="--clean" in sys.argv[2:]
only_new="--only-new" in sys.argv[2:]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def git(*args):return subprocess.check_output(["git","-c","core.autocrlf=false",*args],cwd=ROOT,text=True).strip()
def save(name,value):
    with (OUT/(name+".json")).open("x",newline="\n") as f:json.dump(value,f,indent=2);f.write("\n")
if clean:assert not git("status","--porcelain","-uno"),"Clean committed source required"
new=["runtime/include/mka/retail_permanent_string_data.hpp","runtime/src/native/retail_permanent_string_data.cpp",
     "runtime/include/mka/retail_permanent_strings.hpp","runtime/src/native/retail_permanent_strings.cpp",
     "tests/test_retail_permanent_strings.cpp"]
changed=git("diff","--name-only","--","runtime","tests","cmake").splitlines()
for name in sorted(set(new+changed)):assert b"\r" not in (ROOT/name).read_bytes(),name
subprocess.run(["git","-c","core.autocrlf=false","diff","--check","--","runtime","tests","cmake"],cwd=ROOT,check=True)
names=set(git("ls-files","runtime","tests","cmake","recomp","CMakeLists.txt").splitlines())|set(new)
pins={n:sha(ROOT/n) for n in sorted(names)};save("SOURCE_FILES",pins)
assets=PARENT/"private-inputs/native-ps2-loose-20261121"
asset_sha=sha(assets/"art/gameart.ssf")
assert asset_sha=="9900120ff8782b16e0d73ee885c8f4df6c82545b915dd10105e6694a03658a39"
original=json.loads((RECORD/"ORIGINAL_STRING_EXPECTATIONS.json").read_text())
expected=OUT/"original-strings.tsv"
with expected.open("x",newline="\n") as f:
    for language,module in enumerate(original["modules"]):
        for value in module["values"]:f.write(f'{language}\t{value["index"]}\t{value["length"]}\t{value["sha256"]}\n')
DEPS=PARENT.parent/"dependencies"
COMPILER=DEPS/"compiler/llvm-mingw-20261006-ucrt-x86_64/bin"
CMAKE=DEPS/"cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe"
environment=dict(os.environ);environment["PATH"]=str(COMPILER)+os.pathsep+environment["PATH"]
def short(path):
    buffer=ctypes.create_unicode_buffer(32768)
    assert ctypes.windll.kernel32.GetShortPathNameW(str(path),buffer,len(buffer))
    return buffer.value
def run(name,argv,expected_exit=0):
    argv=list(map(str,argv));start=time.monotonic()
    p=subprocess.run(argv,cwd=ROOT,env=environment,capture_output=True,timeout=600,creationflags=subprocess.CREATE_NO_WINDOW)
    raw=p.stdout+p.stderr
    (OUT/(name+".log.gz")).write_bytes(gzip.compress(raw,mtime=0))
    save(name,dict(argv=argv,exit=p.returncode,seconds=time.monotonic()-start,
        executable_sha256=sha(Path(argv[0])),source_commit=git("rev-parse","HEAD"),
        source_manifest_sha256=sha(OUT/"SOURCE_FILES.json"),log_sha256=hashlib.sha256(raw).hexdigest()))
    print(name,p.returncode,raw.decode(errors="replace")[-2200:] if "symbols" not in name else "",flush=True)
    if expected_exit is not None:assert p.returncode==expected_exit,name
    return raw.decode(errors="replace")
sdk=ROOT/"third_party/sdl2"
if not sdk.exists():
    source=ROOT.parent/"ps2-native-recomp-selected-owner/third_party/sdl2"
    sdk_pins={p.relative_to(source).as_posix():sha(p) for p in source.rglob("*") if p.is_file()}
    assert sdk_pins
    shutil.copytree(source,sdk)
    assert sdk_pins=={p.relative_to(sdk).as_posix():sha(p) for p in sdk.rglob("*") if p.is_file()}
    save("SDL_DEPENDENCY",dict(source=str(source),files=sdk_pins))
build=ROOT/"b"
if not build.exists():
    build.mkdir();(build/".gitignore").write_text("*\n",newline="\n")
    run("configure",[CMAKE,"-S",short(ROOT),"-B",short(build),"-G","Ninja","-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_CXX_COMPILER="+str(COMPILER/"clang++.exe"),"-DCMAKE_MAKE_PROGRAM="+str(DEPS/"ninja/ninja.exe"),
        "-DSDL2_DIR="+str(sdk/"lib/cmake/SDL2"),"-DMKA_NATIVE_MEDIA_HEADERS="+str(DEPS/"native-media/ffmpeg-7.1.4")])
empty=OUT/"fixtures/empty";empty.mkdir(parents=True)
plans=[
 ("test_native_retail_permanent_strings",[assets,expected,OUT/"fixtures/strings"]),
 ("test_native_retail_game_resource_registry",[]),
 ("test_native_retail_loaded_system_art",[assets,OUT/"fixtures/font"]),
 ("test_native_retail_game_session",[]),
 ("test_native_retail_saved_profile_records",[OUT/"fixtures/profiles"]),
 ("test_native_retail_initial_reset_arcade",[empty]),
]
if only_new:plans=plans[:1]
run("build",[CMAKE,"--build",short(build),"--target",*[n for n,_ in plans],"-j2"])
for name,args in plans:run(name,[build/(name+".exe"),*args])
prior=json.loads((ROOT/"research/runs/native-loaded-font-20261122/validation-owner-clean-03/VERIFICATION.json").read_text())["remaining_undefined_symbols"]
resolved=[n for n in prior if "loaded_art_required::queue_permanent_strings_215660(" in n or "loaded_art_required::complete_permanent_strings_215730(" in n]
assert len(prior)==62 and len(resolved)==2
expected_symbols=sorted(set(prior)-set(resolved))
log=run("full-entry-negative",[CMAKE,"--build",short(build),"--target","test_native_startup_missing_bindings","-j2"],None)
missing=re.findall(r"undefined symbol: ([^\r\n]+)",log)
assert len(missing)==len(set(missing))==len(expected_symbols)==60 and sorted(missing)==expected_symbols
assert not (build/"test_native_startup_missing_bindings.exe").exists()
symbols=run("permanent-symbols",[COMPILER/"llvm-nm.exe","-C",build/"test_native_retail_permanent_strings.exe"])
assert not re.search(r"(?<![A-Za-z0-9_])(?:ps2|gs|spu2|sif|Cpu|recomp)::|\b(?:g_ram|g_spr)\b|SDL_OpenAudio",symbols)
assert re.search(r"\bT SDL_InitSubSystem\b",symbols) and "__imp_SDL_InitSubSystem" not in symbols
assert pins=={n:sha(ROOT/n) for n in pins} and sha(assets/"art/gameart.ssf")==asset_sha
save("VERIFICATION",dict(status="PASS_NATIVE_PERMANENT_STRINGS_COMPONENTS",source_commit=git("rev-parse","HEAD"),
    tracked_status=git("status","--porcelain","-uno"),clean_tracked_source=not git("status","--porcelain","-uno"),
    clients=[dict(name=n,sha256=sha(build/(n+".exe"))) for n,_ in plans],original_strings=390,
    source_manifest_sha256=sha(OUT/"SOURCE_FILES.json"),original_gameart_sha256=asset_sha,
    compiler_sha256=sha(COMPILER/"clang++.exe"),cache_sha256=sha(build/"CMakeCache.txt"),
    verification_tool_sha256=sha(Path(__file__).resolve()),resolved_bindings=resolved,remaining_undefined_symbols=missing,
    focused_new_client_only=only_new,
    new_string_client_gpu=False,other_font_client_hidden_gpu=True,visible_window=False,presentation=False,audio_endpoint=False,
    game_run=False,oracle_comparison="OPEN",acceptance="OPEN"))
print("PASS",len(plans),"connected clients/all390 original strings/exact60 negative; whole-game acceptance OPEN",flush=True)
