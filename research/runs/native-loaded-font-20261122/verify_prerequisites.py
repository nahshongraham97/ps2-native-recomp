"""Build and execute only affected prerequisite clients, retaining exact receipts."""
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

RECORD = Path(__file__).resolve().parent
ROOT = RECORD.parents[2]
PARENT = ROOT.parents[2]
OUT = RECORD / sys.argv[1]
OUT.mkdir(exist_ok=False)
DEPS = PARENT.parent / "dependencies"
COMPILER = DEPS / "compiler/llvm-mingw-20261006-ucrt-x86_64/bin"
CMAKE = DEPS / "cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe"
ENV = dict(os.environ)
ENV["PATH"] = str(COMPILER) + os.pathsep + ENV["PATH"]
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
def git(*args):
    return subprocess.check_output(["git", "-c", "core.autocrlf=false", *args], cwd=ROOT, text=True).strip()
def save(name, value):
    with (OUT / (name + ".json")).open("x", newline="\n") as f:
        json.dump(value, f, indent=2)
        f.write("\n")
def short(path):
    buffer = ctypes.create_unicode_buffer(32768)
    assert ctypes.windll.kernel32.GetShortPathNameW(str(path), buffer, len(buffer))
    return buffer.value
def run(name, argv, expected=0, timeout=600):
    argv = list(map(str, argv))
    started = time.monotonic()
    result = subprocess.run(argv, cwd=ROOT, env=ENV, capture_output=True,
                            timeout=timeout, creationflags=subprocess.CREATE_NO_WINDOW)
    raw = result.stdout + result.stderr
    (OUT / (name + ".log.gz")).write_bytes(gzip.compress(raw, mtime=0))
    receipt = dict(argv=argv, exit=result.returncode, seconds=time.monotonic()-started,
                   log_sha256=hashlib.sha256(raw).hexdigest(), source_commit=git("rev-parse", "HEAD"),
                   source_manifest_sha256=sha(OUT / "SOURCE_FILES.json"))
    if Path(argv[0]).is_file():
        receipt["executable_sha256"] = sha(Path(argv[0]))
    save(name, receipt)
    print(name, result.returncode, raw.decode(errors="replace")[-1200:], flush=True)
    if expected is not None:
        assert result.returncode == expected, name
    return raw.decode(errors="replace")

assert not git("status", "--porcelain", "-uno"), "Clean committed source required"
names = git("ls-files", "runtime", "recomp", "cmake", "tests", "CMakeLists.txt").splitlines()
pins = {name: sha(ROOT / name) for name in names}
save("SOURCE_FILES", pins)
sdk = ROOT / "third_party/sdl2"
if not sdk.exists():
    original = ROOT.parent / "ps2-native-recomp/third_party/sdl2"
    sdk_pins = {p.relative_to(original).as_posix(): sha(p) for p in original.rglob("*") if p.is_file()}
    shutil.copytree(original, sdk)
    assert sdk_pins == {p.relative_to(sdk).as_posix(): sha(p) for p in sdk.rglob("*") if p.is_file()}
    save("SDL_DEPENDENCY", dict(source=str(original), files=sdk_pins))
build = ROOT / "b"
if not build.exists():
    build.mkdir()
    run("configure", [CMAKE, "-S", short(ROOT), "-B", short(build), "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_CXX_COMPILER=" + str(COMPILER / "clang++.exe"),
        "-DCMAKE_MAKE_PROGRAM=" + str(DEPS / "ninja/ninja.exe"),
        "-DSDL2_DIR=" + str(sdk / "lib/cmake/SDL2"),
        "-DMKA_NATIVE_MEDIA_HEADERS=" + str(DEPS / "native-media/ffmpeg-7.1.4")])
assets = PARENT / "private-inputs/native-ps2-loose-20261121"
manifest = PARENT / "research/runs/native-retail-startup-20261120/connected-retail-entry-01/package-and-constructors-05/input-manifests/package-members.tsv"
plans = [
    ("test_native_retail_profile_permission", [OUT / "permission"]),
    ("test_native_retail_profile_entry_decision", []),
    ("test_native_retail_saved_profile_records", [OUT / "records"]),
    ("test_native_file_read_job", [assets, OUT / "io"]),
    ("test_native_file", []),
    ("test_native_retail_package", [assets, manifest]),
    ("test_native_gl_texture", [assets, OUT / "readbacks"]),
]
compile_only = ["test_native_movie_gl_device", "test_native_movie_pipeline", "test_native_movie_gpu_completion"]
run("build", [CMAKE, "--build", short(build), "--target", *[n for n, _ in plans], *compile_only, "-j2"])
clients = []
for name, args in plans:
    run(name, [build / (name + ".exe"), *args])
    clients.append(dict(name=name, sha256=sha(build / (name + ".exe")), executed=True))
for name in compile_only:
    clients.append(dict(name=name, sha256=sha(build / (name + ".exe")), executed=False))
prior = json.loads((ROOT / "research/runs/native-retail-saved-profile-records-20261122/clean-validation-01/VERIFICATION.json").read_text())["remaining_undefined_symbols"]
expected = sorted(n for n in prior if "observe_retail_profile_permission_from_4eee60(" not in n)
log = run("full-entry-negative", [CMAKE, "--build", short(build), "--target", "test_native_startup_missing_bindings", "-j2"], None)
missing = re.findall(r"undefined symbol: ([^\r\n]+)", log)
assert len(missing) == len(set(missing)) == len(expected) == 63 and sorted(missing) == expected
assert not (build / "test_native_startup_missing_bindings.exe").exists()
assert pins == {name: sha(ROOT / name) for name in names}
save("VERIFICATION", dict(status="PASS_COMMITTED_PREREQUISITES", commit=git("rev-parse", "HEAD"),
    clean_tracked_source=True, clients=clients, remaining_undefined_symbols=missing,
    compiler_sha256=sha(COMPILER / "clang++.exe"), cache_sha256=sha(build / "CMakeCache.txt"),
    actual_gpu_context_opened=True, visible_window=False, presentation=False, audio_endpoint=False,
    game_run=False, acceptance="OPEN",
    limits=["Unknown GL delete outcome is sealed but not fault-injected.",
            "Movie clients are compile-only; small movie upload runs in image client.",
            "No loaded-section/font implementation is included in this prerequisite check."]))
print("PASS committed prerequisite clients; native game acceptance OPEN", flush=True)
