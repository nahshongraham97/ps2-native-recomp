from pathlib import Path
import json,subprocess
r=Path.cwd();d=r/'research/runs/parallel-saved-profile-child-20261122';a=json.loads((d/'COMMAND.json').read_text());a[-2]=str(d/'source-04');a[-1]='00515240,00515310,00515400,00431030,00431040,005140f0,00514240,005125c0,00513e50,00132ec0';(d/'COMMAND_04.json').write_bytes((json.dumps(a,indent=2)+'\n').encode());p=subprocess.run(a,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(d/'export-04.log').write_bytes(p.stdout);print(p.returncode);print(p.stdout.decode(errors='replace')[-300:])
