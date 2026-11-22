from pathlib import Path
import json,subprocess
r=Path.cwd();d=r/'research/runs/parallel-saved-profile-child-20261122';a=json.loads((d/'COMMAND.json').read_text());a[-2]=str(d/'source-03');a[-1]='00511f00,00515580,005155d0,005157f0,00515c40,00515e60,00515f70,00516220';(d/'COMMAND_03.json').write_bytes((json.dumps(a,indent=2)+'\n').encode());p=subprocess.run(a,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(d/'export-03.log').write_bytes(p.stdout);print(p.returncode);print(p.stdout.decode(errors='replace')[-250:])
