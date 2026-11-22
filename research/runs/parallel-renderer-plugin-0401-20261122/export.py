from pathlib import Path
import json,subprocess,hashlib
r=Path.cwd();d=r/'research/runs/parallel-renderer-plugin-0401-20261122';parent=r.parents[2]
s=json.loads((parent/'local-research/ghidra-agent-sessions/profile-review-365c6213b6b44889b939a552d2c16d89/SESSION.json').read_text());(d/'PRIVATE_SESSION.json').write_bytes((json.dumps(s,indent=2)+'\n').encode())
a=json.loads((r/'research/runs/parallel-renderer-engine-init-20261122/COMMAND.json').read_text());i=a.index('ghidra.app.util.headless.AnalyzeHeadless');a[i+1]=str(Path(s['projects']['ps2']['project']).parent);a[i+2]=Path(s['projects']['ps2']['project']).stem;a[a.index('-scriptPath')+1]=str(d);a[-2]=str(d/'source-01');a[-1]='001a3af0,001a3ad0,001a36c0,001a3600,001a3800,001a3760'
(d/'COMMAND.json').write_bytes((json.dumps(a,indent=2)+'\n').encode());p=subprocess.run(a,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(d/'ghidra-01.log').write_bytes(p.stdout);print(p.returncode);print(p.stdout.decode(errors='replace')[-1400:])
