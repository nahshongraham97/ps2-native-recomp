"""Bounded existing-program reference metadata; no new decompilation or runtime run."""
from pathlib import Path
import gzip,hashlib,json,subprocess,time
RECORD=Path(__file__).resolve().parent;ROOT=RECORD.parents[2];PARENT=ROOT.parents[2]
session=PARENT/"local-research/ghidra-agent-sessions/mode-scene-xbox-1c3c47b6b8d84fc5874adda06c554d02/SESSION.json"
argv=json.loads(session.read_text())["projects"]["xbox"]["headless_argv"]
argv[argv.index("-scriptPath")+1]=str(RECORD)
argv+=["-postScript","FindXboxProfileRefs.java",str(RECORD/"PROFILE_OWNER_REFS.tsv"),"00105020","0082eca8"]
start=time.monotonic();p=subprocess.run(argv,cwd=PARENT,capture_output=True,timeout=120,creationflags=subprocess.CREATE_NO_WINDOW)
raw=p.stdout+p.stderr
with (RECORD/"profile-refs.log.gz").open("xb") as f:f.write(gzip.compress(raw,mtime=0))
with (RECORD/"PROFILE_REFS_COMMAND.json").open("x",newline="\n") as f:
 json.dump(dict(argv=argv,exit=p.returncode,seconds=time.monotonic()-start,
   session_sha256=hashlib.sha256(session.read_bytes()).hexdigest(),
   script_sha256=hashlib.sha256((RECORD/"FindXboxProfileRefs.java").read_bytes()).hexdigest()),f,indent=2);f.write("\n")
assert p.returncode==0
assert (RECORD/"PROFILE_OWNER_REFS.tsv").is_file(),raw.decode(errors="replace")[-2500:]
print((RECORD/"PROFILE_OWNER_REFS.tsv").read_text(),flush=True)
