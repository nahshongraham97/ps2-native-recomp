"""Authenticate full private EE listings against original ELF PT_LOAD bytes."""
from pathlib import Path
import struct, hashlib, json
r=Path(__file__).resolve().parent
f=r.parents[2]
elf=f.parents[2]/'research/inputs/SLUS_214.10'
b=elf.read_bytes()
assert hashlib.sha256(b).hexdigest()=='a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f'
o=struct.unpack_from('<I',b,28)[0]
n,z=struct.unpack_from('<HH',b,42)
segments=[struct.unpack_from('<8I',b,o+i*n) for i in range(z)]
segments=[s for s in segments if s[0]==1]
def extract(a,n):
    s=next(s for s in segments if s[2]<=a and a+n<=s[2]+s[4])
    off=s[1]+a-s[2]
    return dict(vma=hex(a),size=n,offset=off,segment=list(s),bytes=b[off:off+n].hex())
functions={}
for p in (r/'source-joined').glob('*.asm'):
    rows=[line.split('\t',2) for line in p.read_text().splitlines()]
    addresses=[int(line[0],16) for line in rows]
    assert addresses==list(range(addresses[0],addresses[-1]+4,4))
    entry=extract(addresses[0],len(rows)*4)
    assert entry['bytes']==''.join(line[1] for line in rows)
    functions[p.stem]=dict(entry,instructions=len(rows),full_contiguous=True)
assert {k:v['instructions'] for k,v in functions.items()}=={'00122740':226,'00122ad0':40,'00181a30':40,'001861b0':173,'0019a5e0':20,'0019a630':20}
data={hex(a):extract(a,n) for a,n in [(0x5b4728,16),(0x5fce60,64)]}
group=struct.unpack('<I',bytes.fromhex(extract(0x5b472c,4)['bytes']))[0]
data[hex(group)]=extract(group,48)
value=dict(image=str(elf.resolve()),sha256=hashlib.sha256(b).hexdigest(),language='r5900:LE:32:default',listing='private saved-project ExportSectionTimingEvidence',functions=functions,data=data)
existing=json.loads((r/'SOURCE_JOIN.json').read_text())
assert existing==value
print('PASS519 original instruction words and referenced group/name data')
