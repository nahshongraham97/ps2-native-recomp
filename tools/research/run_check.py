"""Bounded streamed checks with exclusive source/build/result evidence."""
from pathlib import Path
import ctypes, gzip, hashlib, io, json, os, subprocess, sys, threading, time

TOOL_DIR = Path(__file__).resolve().parent
# Resolve ROOT, PARENT, and BACKUP
if (TOOL_DIR.parents[1] / "runtime").is_dir():
    ROOT = TOOL_DIR.parents[1]
    PARENT = ROOT.parents[2]
    BACKUP = PARENT.parent
elif (TOOL_DIR.parent / "runtime").is_dir():
    ROOT = TOOL_DIR.parent
    PARENT = ROOT
    BACKUP = PARENT.parent
else:
    ROOT = Path.cwd()
    PARENT = ROOT
    BACKUP = ROOT.parent

if len(sys.argv) == 1:
    name = "b/run_check_all"
    phase = "all"
elif len(sys.argv) == 2 and sys.argv[1] in ("all", "all_clients", "--all"):
    name = "b/run_check_all"
    phase = "all"
elif len(sys.argv) == 2:
    name = sys.argv[1]
    phase = "all"
else:
    name = sys.argv[1]
    phase = sys.argv[2]

p_name = Path(name)
if p_name.is_absolute():
    OUT = p_name
elif ("/" in name or "\\" in name) and (ROOT / name).parent.exists():
    OUT = (ROOT / name).resolve()
else:
    OUT = (Path.cwd() / name).resolve()

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

deps = BACKUP / "dependencies"
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
    "test_native_retail_display_movie_presentation": {
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
    "movie_presentation": "test_native_retail_display_movie_presentation",
    "display_movie": "test_native_retail_display_movie_presentation",
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
elif phase in ("all", "all_clients"):
    import shutil
    start_total = time.monotonic()
    client_results = []
    failed = 0
    all_raw = bytearray()
    print("START_CHECK all 15 component clients...", flush=True)
    for exe_name, spec in KNOWN_CLIENTS.items():
        if "clean_fixtures" in spec:
            for fix_dir in spec["clean_fixtures"](OUT):
                if fix_dir.is_dir():
                    shutil.rmtree(fix_dir)
        c_args = spec["args"](ROOT, OUT)
        for arg in c_args:
            if arg.endswith((".tsv", ".json")):
                arg_p = ROOT / arg if not Path(arg).is_absolute() else Path(arg)
                if not arg_p.exists():
                    raise FileNotFoundError(f"Required fixture file not found: {arg_p}")
        exe_path = build / f"{exe_name}.exe"
        if not exe_path.exists():
            raise FileNotFoundError(f"Built executable not found: {exe_path}. Build it first.")
        c_argv = [str(exe_path), *c_args]
        sub = subprocess.run(c_argv, cwd=ROOT, env=env, capture_output=True, text=True)
        all_raw.extend(sub.stdout.encode("utf-8", errors="replace"))
        all_raw.extend(sub.stderr.encode("utf-8", errors="replace"))
        short_name = exe_name.replace("test_native_retail_renderer_", "").replace("test_native_retail_", "")
        if sub.returncode == 0:
            print(f"PASS: {short_name:15s} (exit 0)", flush=True)
            client_results.append(dict(target=exe_name, exit=0, status="PASS"))
        else:
            print(f"FAIL: {short_name:15s} (exit {sub.returncode})", flush=True)
            print("  stdout:", sub.stdout.strip())
            print("  stderr:", sub.stderr.strip())
            failed += 1
            client_results.append(dict(target=exe_name, exit=sub.returncode, status="FAIL"))

    total_time = time.monotonic() - start_total
    buf = io.BytesIO()
    with gzip.GzipFile(filename="", mode="wb", fileobj=buf, mtime=0) as gz:
        gz.write(bytes(all_raw))
    (OUT / "output.log.gz").write_bytes(buf.getvalue())

    receipt = dict(
        status="PASS" if failed == 0 else "FAIL",
        clients_passed=len(KNOWN_CLIENTS) - failed,
        clients_total=len(KNOWN_CLIENTS),
        seconds=total_time,
        client_results=client_results,
        source_commit=git("rev-parse", "HEAD"),
        tracked_status=git("status", "--porcelain", "-uno"),
        phase="all",
        game_run=False,
        visible_window=False,
        audio_endpoint=False,
        acceptance="OPEN"
    )
    (OUT / "RESULT.json").write_text(json.dumps(receipt, indent=2) + "\n", newline="\n")
    print(f"\nTotal: {len(KNOWN_CLIENTS) - failed}/{len(KNOWN_CLIENTS)} passed (in {round(total_time, 2)}s).", flush=True)
    if failed != 0:
        sys.exit(1)
    sys.exit(0)
elif phase == "negative":
    argv = [str(cmake), "--build", short.value, "--target", "test_native_startup_missing_bindings", "-j2"]
    deadline = 90
elif phase == "headless":
    build_argv = [str(cmake), "--build", short.value, "--target", "test_native_headless_entry_run", "-j2"]
    print("START_BUILD headless", " ".join(build_argv), flush=True)
    b_res = subprocess.run(build_argv, cwd=ROOT, env=env, capture_output=True, text=True)
    if b_res.returncode != 0:
        print("FAIL: Headless build failed\n", b_res.stdout, b_res.stderr)
        sys.exit(1)
    exe_path = build / "test_native_headless_entry_run.exe"
    argv = [str(exe_path)]
    deadline = 90
elif phase == "symbols":
    argv = [str(bin_dir / "llvm-nm.exe"), "-C", str(build / (sys.argv[3] + ".exe"))]
    deadline = 30
else:
    raise ValueError("build/client/negative/symbols required")

print("START_CHECK", phase, "deadline", deadline, "seconds", " ".join(argv), flush=True)
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

is_skip = (b"[headless-gate] SKIP:" in raw)
receipt = dict(
    status="TIMEOUT" if timed_out else "SKIP" if is_skip else "PASS" if exit_code == 0 else "NONZERO",
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
