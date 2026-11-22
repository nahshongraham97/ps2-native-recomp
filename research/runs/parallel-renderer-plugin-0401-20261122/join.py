from pathlib import Path
import json,hashlib,struct
r=Path.cwd();d=r/'research/runs/parallel-renderer-plugin-0401-20261122';parent=r.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest()
e=(parent/'research/inputs/SLUS_214.10').read_bytes();ph=struct.unpack_from('<I',e,28)[0];sz,n=struct.unpack_from('<HH',e,42);loads=[struct.unpack_from('<8I',e,ph+i*sz) for i in range(n)]
def ee(a,n):
 s=next(s for s in loads if s[0]==1 and s[2]<=a and a+n<=s[2]+s[4]);off=s[1]+a-s[2];return e[off:off+n],off
j={'ELF_sha256':sha(e),'full_listings':{}};total=0
for folder in ['source-01','source-02']:
 for p in (d/folder).glob('*.asm'):
  b=p.read_bytes();rows=[x.split('\t',2) for x in b.decode().splitlines()];a=int(rows[0][0],16);end=int(rows[-1][0],16)+4;assert len(rows)*4==end-a,p
  for i,(s,h,ins) in enumerate(rows):assert int(s,16)==a+4*i and ee(a+4*i,4)[0]==bytes.fromhex(h)
  data,off=ee(a,end-a);j['full_listings'][p.relative_to(d).as_posix()]={'begin':hex(a),'end_exclusive':hex(end),'words':len(rows),'sha256':sha(b),'bytes_sha256':sha(data),'file_offset':off};total+=len(rows)
# Independently authenticate actual caller registration row and prior original source.
p=r/'research/runs/parallel-renderer-engine-init-20261122/source-01/001a66a0.asm';b=p.read_bytes();rows=[x.split('\t',2) for x in b.decode().splitlines()]
for a,h,ins in rows:assert ee(int(a,16),4)[0]==bytes.fromhex(h)
j['registration']={'path':p.relative_to(r).as_posix(),'sha256':sha(b),'words':len(rows)};j['total_words']=total
xb=(parent/'private-inputs/xbox-mka-20261121/default.xbe').read_bytes();base=struct.unpack_from('<I',xb,0x104)[0];count,table=struct.unpack_from('<II',xb,0x11c);sections=[struct.unpack_from('<5I',xb,table-base+i*56) for i in range(count)]
def xbox(a,n):
 s=next(s for s in sections if s[1]<=a and a+n<=s[1]+s[4]);off=s[3]+a-s[1];return xb[off:off+n],off
j['Xbox_sha256']=sha(xb);j['Xbox_listings']={};xtotal=0
for folder in ['xbox-01','xbox-02']:
 for p in (d/folder).glob('*.asm'):
  b=p.read_bytes();rows=[x.split('\t',2) for x in b.decode().splitlines()];spans=[];gaps=[];end=None
  for a,h,ins in rows:
   va=int(a,16);data=bytes.fromhex(h);actual,off=xbox(va,len(data));assert actual==data
   if end is not None and va!=end:gaps.append({'begin':hex(end),'end':hex(va),'bytes':xbox(end,va-end)[0].hex()})
   spans.append([va,len(data),off]);end=va+len(data)
  j['Xbox_listings'][p.relative_to(d).as_posix()]={'instructions':len(rows),'sha256':sha(b),'mapped_spans':spans,'gaps':gaps};xtotal+=len(rows)
j['Xbox_instructions']=xtotal
j['BSS']={}
for name,a in [('0401 source module extension identity',0x64e4b0),('0401 source instance count',0x64e4b4)]:
 s=next(s for s in loads if s[0]==1 and s[2]+s[4]<=a and a+4<=s[2]+s[5]);j['BSS'][name]={'vma':hex(a),'size':4,'initial':'loader zero tail','PT_LOAD':s}
j['Xbox_alignment_decode']='2C1756:8DA42400000000 = LEA ESP,[ESP+0];2C175D:8D4900 = LEA ECX,[ECX+0]. Identity alignment only,no flags change. Original bytes explicitly joined.'
(d/'SOURCE_JOIN.json').write_bytes((json.dumps(j,indent=2)+'\n').encode());print('PASS',total,'EE words',xtotal,'Xbox instructions');print('gaps',[(p,x['gaps']) for p,x in j['Xbox_listings'].items() if x['gaps']])
