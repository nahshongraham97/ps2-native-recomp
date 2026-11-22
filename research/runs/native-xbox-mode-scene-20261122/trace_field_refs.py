"""Reusable bounded references from this session's existing private Xbox program."""
from pathlib import Path
import gzip,hashlib,json,subprocess,sys,time
RECORD=Path(__file__).resolve().parent;ROOT=RECORD.parents[2];PARENT=ROOT.parents[2]
name=sys.argv[1];addresses=sys.argv[2:];assert addresses
session=PARENT/"local-research/ghidra-agent-sessions/mode-scene-xbox-1c3c47b6b8d84fc5874adda06c554d02/SESSION.json"
argv=json.loads(session.read_text())["projects"]["xbox"]["headless_argv"]
argv[argv.index("-scriptPath")+1]=str(RECORD)
argv+=["-postScript","FindXboxProfileRefs.java",str(RECORD/(name+".tsv")),*addresses]
start=time.monotonic();p=subprocess.run(argv,cwd=PARENT,capture_output=True,timeout=120,creationflags=subprocess.CREATE_NO_WINDOW)
raw=p.stdout+p.stderr
with (RECORD/(name+".log.gz")).open("xb") as f:f.write(gzip.compress(raw,mtime=0))
with (RECORD/(name+".command.json")).open("x",newline="\n") as f:
 json.dump(dict(argv=argv,exit=p.returncode,seconds=time.monotonic()-start,
   session_sha256=hashlib.sha256(session.read_bytes()).hexdigest(),
   script_sha256=hashlib.sha256((RECORD/"FindXboxProfileRefs.java").read_bytes()).hexdigest()),f,indent=2);f.write("\n")
assert p.returncode==0
path=RECORD/(name+".tsv");assert path.is_file(),raw.decode(errors="replace")[-2000:]
print(path.read_text(),flush=True)
