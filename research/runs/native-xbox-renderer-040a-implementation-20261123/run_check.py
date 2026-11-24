"""Bounded streamed 0409 checks with exclusive source/build/result evidence."""
from pathlib import Path
import ctypes, gzip, hashlib, io, json, os, subprocess, sys, threading, time

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

KNOWN_CLIENTS = {
    "test_native_retail_renderer_allocator": {
        "args": lambda root, out: ["."],
    },
    "test_native_retail_renderer_core": {
        "args": lambda root, out: ["research/runs/parallel-renderer-engine-init-20261122"],
    },
    "test_native_retail_renderer_matrix": {
        "args": lambda root, out: ["."],
    },
    "test_native_retail_renderer_frames": {
        "args": lambda root, out: ["."],
    },
    "test_native_retail_renderer_streams": {
        "args": lambda root, out: [str(out / "fixtures")],
        "clean_fixtures": lambda out: [out / "fixtures"],
    },
    "test_native_retail_renderer_cameras": {
        "args": lambda root, out: [".", "research/runs/native-xbox-renderer-0405-20261122/depth-expected.tsv"],
    },
    "test_native_retail_renderer_images": {
        "args": lambda root, out: [str(out / "fixtures"), "research/runs/native-xbox-renderer-0406-implementation-20261122/gamma-one-expected.json"],
        "clean_fixtures": lambda out: [out / "fixtures"],
    },
    "test_native_retail_renderer_raster_registration": {
        "args": lambda root, out: ["."],
    },
    "test_native_retail_renderer_rasters": {
        "args": lambda root, out: [],
    },
    "test_native_retail_renderer_textures": {
        "args": lambda root, out: [],
    },
    "test_native_retail_renderer_texture_dictionaries": {
        "args": lambda root, out: [],
    },
    "test_native_retail_renderer_world": {
        "args": lambda root, out: [],
    },
    "test_native_retail_renderer_materials": {
        "args": lambda root, out: [],
    },
    "test_native_retail_renderer_geometry": {
        "args": lambda root, out: [],
    },
}

CLIENT_ALIASES = {
    "allocator": "test_native_retail_renderer_allocator",
    "core": "test_native_retail_renderer_core",
    "matrix": "test_native_retail_renderer_matrix",
    "frames": "test_native_retail_renderer_frames",
    "streams": "test_native_retail_renderer_streams",
    "cameras": "test_native_retail_renderer_cameras",
    "images": "test_native_retail_renderer_images",
    "raster_registration": "test_native_retail_renderer_raster_registration",
    "rasters": "test_native_retail_renderer_rasters",
    "textures": "test_native_retail_renderer_textures",
    "texture_dictionaries": "test_native_retail_renderer_texture_dictionaries",
    "dictionaries": "test_native_retail_renderer_texture_dictionaries",
    "world": "test_native_retail_renderer_world",
    "materials": "test_native_retail_renderer_materials",
    "geometry": "test_native_retail_renderer_geometry",
}

if phase == "build":
    argv = [str(cmake), "--build", short.value, "--target", *sys.argv[3:], "-j2"]
    deadline = 240
elif phase == "client":
    if len(sys.argv) < 4:
        raise ValueError(f"client target name required. Known targets: {sorted(KNOWN_CLIENTS.keys())}")
    target_arg = sys.argv[3]
    exe_name = CLIENT_ALIASES.get(target_arg, target_arg)
    if exe_name not in KNOWN_CLIENTS and not (build / f"{exe_name}.exe").exists():
        raise ValueError(f"Unknown client target '{target_arg}'. Known targets: {sorted(KNOWN_CLIENTS.keys())}")

    spec = KNOWN_CLIENTS.get(exe_name)
    if len(sys.argv) > 4:
        import shutil
        for arg in sys.argv[4:]:
            p = Path(arg)
            if p.is_dir() and "fixtures" in p.name:
                shutil.rmtree(p)
        client_args = sys.argv[4:]
    elif spec:
        import shutil
        if "clean_fixtures" in spec:
            for fix_dir in spec["clean_fixtures"](OUT):
                if fix_dir.is_dir():
                    shutil.rmtree(fix_dir)
        client_args = spec["args"](ROOT, OUT)
    else:
        client_args = []

    # Validate required fixture files exist
    for arg in client_args:
        if arg.endswith((".tsv", ".json")):
            arg_p = ROOT / arg if not Path(arg).is_absolute() else Path(arg)
            if not arg_p.exists():
                raise FileNotFoundError(f"Required fixture file not found: {arg_p}")

    exe_path = build / f"{exe_name}.exe"
    if not exe_path.exists():
        raise FileNotFoundError(f"Built executable not found: {exe_path}. Build it first.")
    argv = [str(exe_path), *client_args]
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

buf = io.BytesIO()
with gzip.GzipFile(filename="", mode="wb", fileobj=buf, mtime=0) as gz:
    gz.write(bytes(raw))
(OUT / "output.log.gz").write_bytes(buf.getvalue())

receipt = dict(
    status="TIMEOUT" if timed_out else "PASS" if exit_code == 0 else "NONZERO",
    argv=argv,
    exit=exit_code,
    deadline_seconds=deadline,
    seconds=time.monotonic() - start,
    executable_sha256=sha(Path(argv[0])),
    uncompressed_log_sha256=hashlib.sha256(raw).hexdigest(),
    log_sha256=hashlib.sha256(raw).hexdigest(),
    log_stream_order="stdout_stderr_interleaved",
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
