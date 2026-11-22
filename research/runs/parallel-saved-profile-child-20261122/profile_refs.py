from pathlib import Path
import json,subprocess
r=Path.cwd();d=r/'research/runs/parallel-saved-profile-child-20261122';a=json.loads((d/'COMMAND.json').read_text());a=a[:a.index('-postScript')]+['-postScript','ProfileFieldRefs.java',str(d/'profile-pointer-refs.tsv')];(d/'POINTER_REF_COMMAND.json').write_bytes((json.dumps(a,indent=2)+'\n').encode());p=subprocess.run(a,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(d/'pointer-refs.log').write_bytes(p.stdout);print(p.returncode)
