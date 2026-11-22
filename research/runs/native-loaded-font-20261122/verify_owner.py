"""Compile connected owner/callers and run never-visible, silent affected checks."""
from pathlib import Path
import ctypes
import gzip
import hashlib
import json
import os
import re
import subprocess
import sys
import time
RECORD=Path(__file__).resolve().parent
ROOT=RECORD.parents[2]
PARENT=ROOT.parents[2]
OUT=RECORD/sys.argv[1];OUT.mkdir(exist_ok=False)
with_prerequisites="--with-prerequisites" in sys.argv[2:]
DEPS=PARENT.parent/"dependencies"
COMPILER=DEPS/"compiler/llvm-mingw-20261006-ucrt-x86_64/bin"
CMAKE=DEPS/"cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe"
env=dict(os.environ);env["PATH"]=str(COMPILER)+os.pathsep+env["PATH"]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def git(*args):return subprocess.check_output(["git","-c","core.autocrlf=false",*args],cwd=ROOT,text=True).strip()
if with_prerequisites:
    assert not git("status","--porcelain","-uno"),"Final verification requires clean committed source"
def save(name,value):
    with (OUT/(name+".json")).open("x",newline="\n") as f:json.dump(value,f,indent=2);f.write("\n")
names=set(git("ls-files","runtime","recomp","tests","cmake","CMakeLists.txt").splitlines())
authored=json.loads((RECORD/"OWNER_PATHS.json").read_text())["paths"]
for name in authored:
    assert b"\r" not in (ROOT/name).read_bytes(),name
subprocess.run(["git","-c","core.autocrlf=false","diff","--check","--",*authored],cwd=ROOT,check=True)
names.update(authored)
pins={name:sha(ROOT/name) for name in sorted(names)}
save("SOURCE_FILES",pins)
def run(name,argv,expected=0):
    argv=list(map(str,argv));start=time.monotonic()
    p=subprocess.run(argv,cwd=ROOT,env=env,capture_output=True,timeout=600,creationflags=subprocess.CREATE_NO_WINDOW)
    raw=p.stdout+p.stderr
    (OUT/(name+".log.gz")).write_bytes(gzip.compress(raw,mtime=0))
    save(name,dict(argv=argv,exit=p.returncode,seconds=time.monotonic()-start,
        executable_sha256=sha(Path(argv[0])),log_sha256=hashlib.sha256(raw).hexdigest(),
        source_manifest_sha256=sha(OUT/"SOURCE_FILES.json"),base_commit=git("rev-parse","HEAD")))
    tail="" if "symbols" in name or name=="required-descendants" else raw.decode(errors="replace")[-1800:]
    print(name,p.returncode,tail,flush=True)
    if expected is not None:assert p.returncode==expected,name
    return raw.decode(errors="replace")
build=ROOT/"b"
short=ctypes.create_unicode_buffer(32768)
assert ctypes.windll.kernel32.GetShortPathNameW(str(build),short,len(short))
plans=[
    ("test_native_retail_loaded_system_art",[PARENT/"private-inputs/native-ps2-loose-20261121",OUT/"font-fixtures"]),
    ("test_native_retail_system_art_cpu",[PARENT/"private-inputs/native-ps2-loose-20261121",OUT/"cpu-errors"]),
    ("test_native_retail_game_session",[]),
    ("test_native_retail_initial_reset_arcade",[OUT/"empty-content"]),
]
if with_prerequisites:
    assets=PARENT/"private-inputs/native-ps2-loose-20261121"
    member_manifest=PARENT/"research/runs/native-retail-startup-20261120/connected-retail-entry-01/package-and-constructors-05/input-manifests/package-members.tsv"
    plans.extend([
        ("test_native_retail_profile_permission",[OUT/"permission"]),
        ("test_native_retail_profile_entry_decision",[]),
        ("test_native_retail_saved_profile_records",[OUT/"records"]),
        ("test_native_file_read_job",[assets,OUT/"io"]),
        ("test_native_file",[]),
        ("test_native_retail_package",[assets,member_manifest]),
        ("test_native_gl_texture",[assets,OUT/"readbacks"]),
    ])
(OUT/"empty-content").mkdir()
run("build",[CMAKE,"--build",short.value,"--target",*[n for n,_ in plans],"-j2"])
for name,args in plans:run(name,[build/(name+".exe"),*args])
prior=json.loads((RECORD/"validation-prerequisites-01/VERIFICATION.json").read_text())["remaining_undefined_symbols"]
resolved=[n for n in prior if any(token in n for token in [
    "main_binding::initialize_retail_section_service(","main_binding::initialize_retail_post_scene_sections(",
    "initial_reset_required::configure_selected_resource_zero_16fc70("])]
assert len(resolved)==3
archive=run("required-descendants",[COMPILER/"llvm-nm.exe","-C",build/"libmka_native_retail_startup.a"])
new=[]
for name in ["queue_permanent_strings_215660","complete_permanent_strings_215730"]:
    found=set(re.findall(r"\bU (mka::retail::loaded_art_required::"+name+r"\([^\r\n]+)",archive))
    assert len(found)==1,(name,found)
    new.extend(found)
expected=sorted((set(prior)-set(resolved))|set(new))
log=run("full-entry-negative",[CMAKE,"--build",short.value,"--target","test_native_startup_missing_bindings","-j2"],None)
missing=re.findall(r"undefined symbol: ([^\r\n]+)",log)
assert len(missing)==len(set(missing))==len(expected)==62 and sorted(missing)==expected
assert not (build/"test_native_startup_missing_bindings.exe").exists()
symbols=run("owner-symbols",[COMPILER/"llvm-nm.exe","-C",build/"test_native_retail_loaded_system_art.exe"])
assert not re.search(r"(?<![A-Za-z0-9_])(?:ps2|gs|spu2|sif|Cpu|recomp)::|\b(?:g_ram|g_spr)\b|SDL_OpenAudio",symbols)
assert pins=={n:sha(ROOT/n) for n in pins}
save("VERIFICATION",dict(status="PASS_CONNECTED_NATIVE_LOADED_SYSTEM_ART_COMPONENTS",
    source_commit=git("rev-parse","HEAD"),tracked_status=git("status","--porcelain","-uno"),
    clean_tracked_source=not git("status","--porcelain","-uno"),with_prerequisites=with_prerequisites,
    clients=[dict(name=n,sha256=sha(build/(n+".exe"))) for n,_ in plans],
    resolved_bindings=resolved,new_required_descendants=new,remaining_undefined_symbols=missing,
    source_manifest_sha256=sha(OUT/"SOURCE_FILES.json"),cache_sha256=sha(build/"CMakeCache.txt"),
    verification_tool_sha256=sha(Path(__file__).resolve()),
    compiler_sha256=sha(COMPILER/"clang++.exe"),actual_gpu_context_opened=True,
    visible_window=False,presentation=False,audio_endpoint=False,game_run=False,acceptance="OPEN"))
print("PASS connected native owner components; required managed descendants/full-game acceptance OPEN",flush=True)
