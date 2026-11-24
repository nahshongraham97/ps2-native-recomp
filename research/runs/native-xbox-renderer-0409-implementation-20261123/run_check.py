"""Bounded streamed 0409 checks with exclusive source/build/result evidence."""
from pathlib import Path
import ctypes, gzip, hashlib, json, os, subprocess, sys, threading, time

R = Path(__file__).resolve().parent
ROOT = R.parents[2]
PARENT = ROOT.parents[2]

name = sys.argv[1]
phase = sys.argv[2]
OUT = R / name
OUT.mkdir(parents=True, exist_ok=True)

def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def git(*a):
    return subprocess.check_output(["git", "-c", "core.autocrlf=false", *a], cwd=ROOT, text=True, timeout=15).strip()

names = git("ls-files", "runtime", "tests", "cmake", "recomp", "CMakeLists.txt").splitlines()
names += git("ls-files", "--others", "--exclude-standard", "--", "runtime", "tests", "cmake").splitlines()
pins = {n: sha(ROOT / n) for n in sorted(set(names))}
(OUT / "SOURCE_FILES.json").write_text(json.dumps(pins, indent=2) + "\n", newline="\n")
git("diff", "--check", "--", "runtime", "tests", "cmake")

deps = PARENT.parent / "dependencies"
bin_dir = deps / "compiler/llvm-mingw-20261006-ucrt-x86_64/bin"
cmake = deps / "cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe"
build = ROOT / "b"

env = dict(os.environ)
env["PATH"] = str(bin_dir) + os.pathsep + str(ROOT / "third_party/sdl2/bin") + os.pathsep + env["PATH"]
temp = Path(os.environ["LOCALAPPDATA"]) / "Temp/opencode"
assert temp.is_dir()
env["TEMP"] = env["TMP"] = str(temp)

short = ctypes.create_unicode_buffer(32768)
assert ctypes.windll.kernel32.GetShortPathNameW(str(build), short, len(short))

if phase == "build":
    argv = [str(cmake), "--build", short.value, "--target", *sys.argv[3:], "-j2"]
    deadline = 240
elif phase == "client":
    argv = [str(build / (sys.argv[3] + ".exe")), *sys.argv[4:]]
    deadline = 90
elif phase == "negative":
    argv = [str(cmake), "--build", short.value, "--target", "test_native_startup_missing_bindings", "-j2"]
    deadline = 90
elif phase == "symbols":
    argv = [str(bin_dir / "llvm-nm.exe"), "-C", str(build / (sys.argv[3] + ".exe"))]
    deadline = 30
else:
    raise ValueError("build/client/negative/symbols required")

print("START0409", phase, "deadline", deadline, "seconds", " ".join(argv), flush=True)
start = time.monotonic()
raw = bytearray()
p = subprocess.Popen(argv, cwd=ROOT, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)

def stream():
    for line in iter(p.stdout.readline, b""):
        raw.extend(line)
        if phase != "symbols":
            print(line.decode(errors="replace").rstrip(), flush=True)

t = threading.Thread(target=stream, daemon=True)
t.start()
timed_out = False
try:
    exit_code = p.wait(timeout=deadline)
except subprocess.TimeoutExpired:
    timed_out = True
    subprocess.run(["taskkill", "/PID", str(p.pid), "/T", "/F"], capture_output=True, timeout=10)
    exit_code = p.wait(timeout=10)
t.join(timeout=5)

with (OUT / "output.log.gz").open("xb") as f:
    f.write(gzip.compress(bytes(raw), mtime=0))

receipt = dict(
    status="TIMEOUT" if timed_out else "PASS" if exit_code == 0 else "NONZERO",
    argv=argv,
    exit=exit_code,
    deadline_seconds=deadline,
    seconds=time.monotonic() - start,
    executable_sha256=sha(Path(argv[0])),
    log_sha256=hashlib.sha256(raw).hexdigest(),
    source_commit=git("rev-parse", "HEAD"),
    tracked_status=git("status", "--porcelain", "-uno"),
    source_manifest_sha256=sha(OUT / "SOURCE_FILES.json"),
    compiler_sha256=sha(bin_dir / "clang++.exe"),
    cache_sha256=sha(build / "CMakeCache.txt"),
    runner_sha256=sha(Path(__file__)),
    process_temp=str(temp),
    phase=phase,
    game_run=False,
    visible_window=False,
    audio_endpoint=False,
    acceptance="OPEN"
)
(OUT / "RESULT.json").write_text(json.dumps(receipt, indent=2) + "\n", newline="\n")
assert pins == {n: sha(ROOT / n) for n in pins}
print(receipt["status"], phase, "exit", exit_code, "seconds", round(receipt["seconds"], 2), flush=True)
if timed_out or (exit_code != 0 and phase != "negative"):
    sys.exit(1)
