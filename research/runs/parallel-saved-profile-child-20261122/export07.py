from pathlib import Path
import json,subprocess
r=Path.cwd();d=r/'research/runs/parallel-saved-profile-child-20261122';a=json.loads((d/'COMMAND.json').read_text());a[-2]=str(d/'source-07');a[-1]='00245cb0,004f3c80';(d/'COMMAND_07.json').write_bytes((json.dumps(a,indent=2)+'\n').encode());p=subprocess.run(a,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(d/'export-07.log').write_bytes(p.stdout);print(p.returncode)
p=d/'join.py';p.write_bytes(p.read_bytes().replace(b"'source-06']",b"'source-06','source-07']"))
