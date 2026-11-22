from pathlib import Path
import json,subprocess
r=Path.cwd();d=r/'research/runs/parallel-saved-profile-child-20261122';a=json.loads((d/'COMMAND.json').read_text());a[-2]=str(d/'source-02');a[-1]='00151dc0,0026f070,00515450,005117c0,0050f140';(d/'COMMAND_02.json').write_bytes((json.dumps(a,indent=2)+'\n').encode());p=subprocess.run(a,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(d/'export-02.log').write_bytes(p.stdout);print(p.returncode);print(p.stdout.decode(errors='replace')[-350:])
