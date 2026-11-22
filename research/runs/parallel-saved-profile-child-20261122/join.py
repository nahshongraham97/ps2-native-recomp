from pathlib import Path
import json,struct,hashlib
r=Path.cwd();d=r/'research/runs/parallel-saved-profile-child-20261122';b=(r.parents[2]/'research/inputs/SLUS_214.10').read_bytes();ph=struct.unpack_from('<I',b,28)[0];sz,n=struct.unpack_from('<HH',b,42);ss=[struct.unpack_from('<8I',b,ph+i*sz) for i in range(n)];sha=lambda b:hashlib.sha256(b).hexdigest()
def data(a,n):
 s=next(s for s in ss if s[0]==1 and s[2]<=a and a+n<=s[2]+s[4]);off=s[1]+a-s[2];return b[off:off+n],off
j={'ELF_sha256':sha(b),'functions':{}};words=0;missing=[]
for folder in ['source-01','source-02','source-03','source-04','source-05','source-06','source-07','source-08','source-09']:
 for p in (d/folder).glob('*.asm'):
  raw=p.read_bytes();rows=[z.split('\t',2) for z in raw.decode().splitlines()];first=int(rows[0][0],16);end=int(rows[-1][0],16)+4;found={int(a,16):h for a,h,_ in rows};omitted={}
  for a,h in found.items():assert data(a,4)[0]==bytes.fromhex(h)
  for a in range(first,end,4):
   if a not in found:
    h=data(a,4)[0].hex();omitted[hex(a)]={'bytes':h,'decode':'SLL zero,zero,0 (NOP)' if h=='00000000' else {0x245e84:'LW a1,0(s2) (direct entry after annulled likely-slot path)',0x4eea2c:'DADDU s5,zero,zero (move s5,zero)'}.get(a,'UNRESOLVED')}
    if h!='00000000' and omitted[hex(a)]['decode']=='UNRESOLVED':missing.append((p.name,hex(a),h))
  z,off=data(first,end-first);j['functions'][p.relative_to(d).as_posix()]={'begin':hex(first),'end_exclusive':hex(end),'listed_words':len(rows),'listing_sha256':sha(raw),'file_offset':off,'span_sha256':sha(z),'omitted':omitted};words+=len(rows)
j['listed_words']=words;j['nonzero_omission_blockers']=missing
(d/'SOURCE_JOIN.json').write_bytes((json.dumps(j,indent=2)+'\n').encode());print('JOIN',len(j['functions']),words,'nonzero omissions',missing)
