from pathlib import Path
import json,subprocess
r=Path.cwd();d=r/'research/runs/parallel-saved-profile-child-20261122';a=json.loads((d/'COMMAND.json').read_text());a[-2]=str(d/'source-05');a[-1]='00514210';(d/'COMMAND_05.json').write_bytes((json.dumps(a,indent=2)+'\n').encode());p=subprocess.run(a,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(d/'export-05.log').write_bytes(p.stdout);print(p.returncode)
p=d/'join.py';p.write_bytes(p.read_bytes().replace(b"'source-04']",b"'source-04','source-05']"))
