"""Decode bounded MSL constructor dependencies in a separate offline project."""
from pathlib import Path
import hashlib
import json
import struct
import subprocess
import time
import sys

record = Path(__file__).resolve().parent
fork = record.parents[2]
parent = fork.parents[2]
prior = record.parent / 'native-retail-sound-context-20261121'
image = parent / 'private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/modules/msl.irx'
data = image.read_bytes()
assert hashlib.sha256(data).hexdigest() == 'f62dfbcedd39bc92cb8b9c02bb685b956a10357f9181d7f8c2801d8eaff07af7'
project = parent / 'local-research/ghidra-msl-worker-20261121'
continuation = len(sys.argv) > 1
if not continuation:
    project.mkdir(exist_ok=False)
raw = project / 'msl-file-backed.bin'
offset = struct.unpack_from('<I', data, 28)[0]
stride, count = struct.unpack_from('<HH', data, 42)
segments = [struct.unpack_from('<8I', data, offset+i*stride) for i in range(count)]
segment, = [s for s in segments if s[0] == 1 and s[2] == 0]
if not continuation:
    raw.write_bytes(data[segment[1]:segment[1]+segment[4]])
argv = json.loads((prior / 'HEADLESS-02.json').read_text())['argv']
old_project = str(parent / 'local-research/ghidra-msl-context-20261121')
argv = [a.replace(old_project, str(project)) for a in argv]
argv[argv.index('MslContext')] = 'SoundWorker'
if continuation:
    argv[argv.index('-import')] = '-process'
    argv[argv.index(str(raw))] = raw.name
post = argv.index('-postScript')
round_id = sys.argv[1] if continuation else '01'
ranges = sys.argv[2:] if continuation else ['46f8:47d8', '5244:5318', '56f4:56fc', 'aa88:ab5c']
argv[post+2:argv.index('-log')] = [str(record / ('source-'+round_id)), *ranges]
argv[argv.index('-log')+1] = str(record / ('ghidra-'+round_id+'.log'))
argv[argv.index('-scriptlog')+1] = str(record / ('scripts-'+round_id+'.log'))
start = time.monotonic()
p = subprocess.run(argv, capture_output=True, text=True, creationflags=subprocess.CREATE_NO_WINDOW)
receipt = dict(argv=argv, exit=p.returncode, stdout=p.stdout, stderr=p.stderr,
    seconds=time.monotonic()-start, image_sha256=hashlib.sha256(data).hexdigest(),
    segment=segment, raw_sha256=hashlib.sha256(raw.read_bytes()).hexdigest())
(record / ('HEADLESS-'+round_id+'.json')).write_text(json.dumps(receipt, indent=2)+'\n', newline='\n')
print('offline MSL decode exit', p.returncode)
assert p.returncode == 0
