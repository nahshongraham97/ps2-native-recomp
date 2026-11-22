from pathlib import Path
import json,subprocess
r=Path.cwd();d=r/'research/runs/parallel-renderer-plugin-0401-20261122';a=json.loads((d/'COMMAND.json').read_text());a=a[:a.index('-postScript')]+['-postScript','PluginRefs.java',str(d/'references.tsv')];(d/'REF_COMMAND.json').write_bytes((json.dumps(a,indent=2)+'\n').encode());p=subprocess.run(a,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(d/'ghidra-refs.log').write_bytes(p.stdout);print(p.returncode);print(p.stdout.decode(errors='replace')[-500:])
