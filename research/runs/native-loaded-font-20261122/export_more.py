"""Use the existing exclusive private EE session for the named unresolved consumers."""
from pathlib import Path
import gzip
import json
import subprocess
import sys
import time
RECORD = Path(__file__).resolve().parent
ROOT = RECORD.parents[2]
PARENT = ROOT.parents[2]
session = PARENT / "local-research/ghidra-agent-sessions/selected-owner-20261122-38f3086bc628417d8a93cef14d25e051/SESSION.json"
command = json.loads(session.read_text())["projects"]["ps2"]["headless_argv"]
command[command.index("-scriptPath")+1] = str(RECORD.parent / "native-selected-resource-owner-20261122")
output = RECORD / sys.argv[1]
command += ["-postScript", "ExportSelectedOwner.java", str(output), *sys.argv[2:]]
start = time.monotonic()
p = subprocess.run(command, cwd=PARENT, capture_output=True, timeout=300, creationflags=subprocess.CREATE_NO_WINDOW)
raw = p.stdout+p.stderr
with output.with_suffix(".log.gz").open("xb") as f:
    f.write(gzip.compress(raw, mtime=0))
with output.with_suffix(".command.json").open("x", newline="\n") as f:
    json.dump(dict(argv=command, exit=p.returncode, seconds=time.monotonic()-start), f, indent=2)
    f.write("\n")
print(raw.decode(errors="replace")[-1800:], flush=True)
assert p.returncode == 0 and (output / (sys.argv[-1]+".asm")).exists()
