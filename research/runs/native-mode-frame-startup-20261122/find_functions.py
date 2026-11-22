from pathlib import Path
import gzip,json,subprocess
RECORD=Path(__file__).resolve().parent;ROOT=RECORD.parents[2];PARENT=ROOT.parents[2]
session=PARENT/"local-research/ghidra-agent-sessions/mode-frame-20261122-6632aeac771042998a16bf70875625ef/SESSION.json"
argv=json.loads(session.read_text())["projects"]["ps2"]["headless_argv"]
argv[argv.index("-scriptPath")+1]=str(RECORD)
argv+=["-postScript","FindAliasFunctions.java",str(RECORD/"ALIAS_FUNCTIONS.tsv"),"0052bf74","00169500","0034d98c"]
p=subprocess.run(argv,cwd=PARENT,capture_output=True,timeout=180,creationflags=subprocess.CREATE_NO_WINDOW)
with (RECORD/"alias-functions.log.gz").open("xb") as f:f.write(gzip.compress(p.stdout+p.stderr,mtime=0))
assert p.returncode==0
print((RECORD/"ALIAS_FUNCTIONS.tsv").read_text(),flush=True)
