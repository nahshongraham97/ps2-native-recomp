from pathlib import Path
import json,subprocess
r=Path.cwd();d=r/'research/runs/parallel-saved-profile-child-20261122';a=json.loads((d/'COMMAND.json').read_text());a[-2]=str(d/'source-08');a[-1]='002460b0,0034d940,0034d9d0,0034daf0,0034db70,0034dc40,0034dd30,0034ddf0,003d0520,00243570,00243a20,00245bd0,004eede0,004eee20,004eee60,004eef00,004ef180,004ef1c0,003f0190,00170e50,00170e90';(d/'COMMAND_08.json').write_bytes((json.dumps(a,indent=2)+'\n').encode());p=subprocess.run(a,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(d/'export-08.log').write_bytes(p.stdout);print(p.returncode);print(p.stdout.decode(errors='replace')[-500:])
p=d/'join.py';p.write_bytes(p.read_bytes().replace(b"'source-07']",b"'source-07','source-08']"))
