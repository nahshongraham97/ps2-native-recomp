from pathlib import Path
import json,subprocess
r=Path.cwd();d=r/'research/runs/parallel-saved-profile-child-20261122';a=json.loads((d/'COMMAND.json').read_text());a[-2]=str(d/'source-06');a[-1]='0050d140,005103a0';(d/'COMMAND_06.json').write_bytes((json.dumps(a,indent=2)+'\n').encode());p=subprocess.run(a,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(d/'export-06.log').write_bytes(p.stdout);print(p.returncode);print(p.stdout.decode(errors='replace')[-250:])
p=d/'join.py';p.write_bytes(p.read_bytes().replace(b"'source-05']",b"'source-05','source-06']"))
