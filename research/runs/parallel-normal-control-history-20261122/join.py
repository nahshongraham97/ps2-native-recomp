from pathlib import Path
import json,struct,hashlib,gzip
w=Path(__file__).resolve().parents[3];d=Path(__file__).parent;root=w.parents[2]
sources={'ee':root/'research/inputs/SLUS_214.10','msl':root/'private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/modules/msl.irx'}
h=lambda b:hashlib.sha256(b).hexdigest()
def raw(p):return p.read_bytes() if p.exists() else gzip.decompress(Path(str(p)+'.gz').read_bytes())
r={'images':{},'listings':{},'mismatches':[]}
for platform,p in sources.items():
 b=p.read_bytes();r['images'][platform]={'path':str(p),'sha256':h(b)};off=struct.unpack_from('<I',b,28)[0];stride,count=struct.unpack_from('<HH',b,42);loads=[struct.unpack_from('<8I',b,off+i*stride) for i in range(count)]
 dirs=[d/'ee'] if platform=='ee' else sorted(p for p in d.glob('msl-*') if p.is_dir())
 for directory in dirs:
  names=set(p.name.removesuffix('.gz') for p in directory.glob('*.tsv*'))
  for name in sorted(names):
   f=directory/name;data=raw(f);rows=[];holes=[]
   for line in data.decode().splitlines():
    a,word,ins=line.split('\t',2);a=int(a,16);segment=next(x for x in loads if x[0]==1 and x[2]<=a and a+4<=x[2]+x[4]);actual=b[segment[1]+a-segment[2]:segment[1]+a-segment[2]+4]
    if actual!=bytes.fromhex(word):r['mismatches'].append({'listing':str(f),'vma':hex(a),'original':actual.hex(),'listed':word})
    rows.append(a)
    if ins=='UNDECODED':holes.append({'vma':hex(a),'word':word})
   assert all(a==rows[0]+i*4 for i,a in enumerate(rows))
   r['listings'][f.relative_to(d).as_posix()]={'words':len(rows),'begin':hex(rows[0]),'end_exclusive':hex(rows[-1]+4),'sha256':h(data),'undecoded':holes}
 if platform=='ee':
  a=0x64df18;segment=next(x for x in loads if x[0]==1 and x[2]+x[4]<=a and a+4<=x[2]+x[5]);r['pause_latch_initial']={'vma':hex(a),'loader_zero_tail':True,'segment_vma':hex(segment[2]),'file_end':hex(segment[2]+segment[4]),'memory_end':hex(segment[2]+segment[5]),'value':0}
assert not r['mismatches']
assert r['images']['ee']['sha256']=='a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f'
assert r['images']['msl']['sha256']=='f62dfbcedd39bc92cb8b9c02bb685b956a10357f9181d7f8c2801d8eaff07af7'
r['scope']='Original word identities only; numeric/caller/native behavior is documented separately. Repeated views count as listed words,not unique coverage.'
(d/'SOURCE_JOIN.json').write_text(json.dumps(r,indent=2)+'\n');print('PASS_ORIGINAL_WORDS',len(r['listings']),sum(v['words'] for v in r['listings'].values()))
