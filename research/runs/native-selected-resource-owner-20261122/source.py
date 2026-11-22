"""Run an exclusive private EE export and record invocation/provenance."""
from pathlib import Path
import gzip
import hashlib
import json
import subprocess
import time

ROOT = Path(__file__).resolve().parents[3]
PARENT = ROOT.parents[2]
RECORD = Path(__file__).resolve().parent
SESSION = PARENT / "local-research/ghidra-agent-sessions/selected-owner-20261122-38f3086bc628417d8a93cef14d25e051/SESSION.json"
manifest = json.loads(SESSION.read_text())
argv = manifest["projects"]["ps2"]["headless_argv"]
argv[argv.index("-scriptPath") + 1] = str(RECORD)
argv += ["-postScript", "ExportSelectedOwner.java", str(RECORD / "source-01"),
         "0016fc70", "00300b60", "00300440", "0018da30", "00334700",
         "003de190", "00229180", "003015e0", "001a8680"]
start = time.monotonic()
result = subprocess.run(argv, cwd=PARENT, capture_output=True, timeout=300,
                        creationflags=subprocess.CREATE_NO_WINDOW)
raw = result.stdout + result.stderr
with (RECORD / "source-01.log.gz").open("xb") as f:
    f.write(gzip.compress(raw, mtime=0))
receipt = dict(argv=argv, exit=result.returncode, seconds=time.monotonic() - start,
               session=str(SESSION), session_sha256=hashlib.sha256(SESSION.read_bytes()).hexdigest(),
               log_sha256=hashlib.sha256(raw).hexdigest(), visible_window=False, audio_endpoint=False)
with (RECORD / "source-01-command.json").open("x", newline="\n") as f:
    json.dump(receipt, f, indent=2)
    f.write("\n")
print(raw.decode(errors="replace")[-5000:], flush=True)
assert result.returncode == 0
assert (RECORD / "source-01/001a8680.asm").is_file(), "Export did not complete"
