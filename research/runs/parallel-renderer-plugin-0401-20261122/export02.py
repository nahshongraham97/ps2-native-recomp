from pathlib import Path
import json,subprocess
r=Path.cwd();d=r/'research/runs/parallel-renderer-plugin-0401-20261122';a=json.loads((d/'COMMAND.json').read_text());a[-2]=str(d/'source-02');a[-1]='001a3880,001a3a50,001a3a90';(d/'COMMAND_02.json').write_bytes((json.dumps(a,indent=2)+'\n').encode());p=subprocess.run(a,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(d/'ghidra-02.log').write_bytes(p.stdout);print(p.returncode)
