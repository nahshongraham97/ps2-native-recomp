"""Reproducible original PS2/Xbox source and bounded data join; no game execution."""
from pathlib import Path
import struct,hashlib,json,collections
r=Path(__file__).resolve().parent
f=r.parents[2];parent=f.parents[2]
sha=lambda b:hashlib.sha256(b).hexdigest()
ps2=parent/'research/inputs/SLUS_214.10';ee=ps2.read_bytes()
assert sha(ee)=='a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f'
ph=struct.unpack_from('<I',ee,28)[0];stride,count=struct.unpack_from('<HH',ee,42)
segments=[struct.unpack_from('<8I',ee,ph+i*stride) for i in range(count)]
segments=[s for s in segments if s[0]==1]
def extract(a,n):
    s=next(s for s in segments if s[2]<=a and a+n<=s[2]+s[4]);off=s[1]+a-s[2]
    return dict(vma=hex(a),size=n,offset=off,segment=list(s),bytes=ee[off:off+n].hex())
functions={}
for p in sorted(r.glob('source-*/*.asm')):
    rows=[v.split('\t',2) for v in p.read_text().splitlines()]
    addresses=[int(row[0],16) for row in rows]
    begin,end=min(addresses),max(addresses)+4
    missing=sorted(set(range(begin,end,4))-set(addresses))
    for a,raw,_ in rows: assert extract(int(a,16),4)['bytes']==raw
    # These omitted words are between completed control-transfer blocks, not
    # call delay slots: source-03 listings show preceding unconditional branch
    # and its separate present delay instruction. Original0 decodes as SLL r0,r0,0.
    assert all(extract(a,4)['bytes']=='00000000' for a in missing)
    functions[p.relative_to(r).as_posix()]=dict(extract(begin,end-begin),listed_instructions=len(rows),omitted_nop_words=[hex(a) for a in missing],complete_words_joined=True)
data={name:extract(a,n) for name,a,n in [('request_defaults',0x5f7320,56),('bank_catalog',0x59bc30,344*40),('sound_parameters',0x5c0d20,0x25b8*4),('categories',0x5e2940,39*8),('preference_defaults',0x5be650,88)]}
#6B6F60 is loader-zero storage, not file-backed data.
s=next(s for s in segments if s[2]<=0x6b6f60 and 0x6b6f78<=s[2]+s[5]);assert 0x6b6f60>=s[2]+s[4]
data['live_preferences_birth']=dict(vma='0x6b6f60',size=24,loader_zero=True,segment=list(s))
prior={}
for name,entry in [('EE-12C440.json','0012c440'),('EE-315C70.json','00315c70')]:
    p=f/'research/runs/native-retail-play-admission-20261122'/name;v=json.loads(p.read_text());rows=v['items'];assert not v['has_more'] and v['total']==len(rows)
    assert bytes.fromhex(''.join(x['bytes'] for x in rows))==bytes.fromhex(functions['source-01/'+entry+'.asm']['bytes'])
    prior[str(p.relative_to(f))]=sha(p.read_bytes())
p=f/'research/runs/native-retail-play-admission-20261122/EE-ORIENTATION.json';prior[str(p.relative_to(f))]=sha(p.read_bytes())
words=struct.unpack('<'+str(0x25b8)+'I',bytes.fromhex(data['sound_parameters']['bytes']))
cats=struct.unpack('<78I',bytes.fromhex(data['categories']['bytes']))
stats={n:dict(collections.Counter(fn(w) for w in words)) for n,fn in [('selector',lambda w:w&1023),('percent',lambda w:w>>13&127),('priority',lambda w:w>>23&3),('output',lambda w:((w>>20&7)^4)-4),('category',lambda w:w>>25&127)]}
assert max(stats['percent'])<=100 and max(stats['category'])<39
stats['reachable_gain_indices']=sorted({cats[2*(w>>25&127)+1]&255 for w in words})
assert stats['reachable_gain_indices']==[0,1,2,3,4]
# Independent shift/lane enumeration of EE extraction against direct bit fields.
mask=(1<<64)-1
for h in range(65536):
    assert (((h<<55)&mask)>>62)==((h>>7)&3)
for byte in range(256):
    signed=byte if byte<128 else byte-256
    shifted=(signed<<57)&mask; signed_shifted=shifted if shifted<1<<63 else shifted-(1<<64)
    assert (signed_shifted>>61)==(((byte>>4&7)^4)-4)
xpath=parent/'private-inputs/xbox-mka-20261121/default.xbe';xb=xpath.read_bytes();assert sha(xb)=='78f99fca447fddc5c5116c09d27e6f1c04af8572dace94f559966588c6f5e393'
base=struct.unpack_from('<I',xb,0x104)[0];count,table=struct.unpack_from('<II',xb,0x11c)
sections=[struct.unpack_from('<10I',xb,table-base+i*56) for i in range(count)]
def xextract(a,n):
    s=next(s for s in sections if s[1]<=a and a+n<=s[1]+s[4]);off=s[3]+a-s[1]
    return dict(vma=hex(a),size=n,offset=off,section=list(s),bytes=xb[off:off+n].hex())
xfuncs={}
for p in sorted(r.glob('xbox-*/*.asm')):
    rows=[v.split('\t',2) for v in p.read_text().splitlines()]
    for a,raw,_ in rows: assert xextract(int(a,16),len(bytes.fromhex(raw)))['bytes']==raw
    begin=int(rows[0][0],16);end=int(rows[-1][0],16)+len(bytes.fromhex(rows[-1][1]));covered=sum(len(bytes.fromhex(x[1])) for x in rows)
    assert end-begin==covered
    xfuncs[p.relative_to(r).as_posix()]=dict(xextract(begin,end-begin),instructions=len(rows),full_contiguous=True)
xdata={n:xextract(a,z) for n,a,z in [('request_defaults',0x502a08,56),('sound_parameters',0x56cbe0,0x25b8*4),('preference_defaults_prefix',0x5684b0,24),('categories',0x5783c8,39*8)]}
comparisons={n:xdata[n]['bytes']==data[n]['bytes'] for n in ['request_defaults','sound_parameters','categories']}
comparisons['preference_defaults_prefix']=xdata['preference_defaults_prefix']['bytes']==data['preference_defaults']['bytes'][:48]
value=dict(status='PASS_SOURCE_ONLY_PARAMETER_JOINS',ps2=dict(image=str(ps2),sha256=sha(ee),language='r5900:LE:32:default',functions=functions,data=data,reused_evidence=prior),xbox=dict(image=str(xpath),sha256=sha(xb),language='x86:LE:32:default',functions=xfuncs,data=xdata,equal_to_ps2=comparisons),sound_table_statistics=stats,checks=['all original listed instruction bytes and complete bounds joined','omitted PS2 zero words independently decoded as unreachable NOPs','65536 halfwords and256 signed-byte extraction cases'],scope='source-derived only; no native implementation/gameplay test')
p=r/'SOURCE_JOIN.json';render=json.dumps(value,indent=2)+'\n'
if p.exists():assert json.loads(p.read_text())==json.loads(render)
else:p.write_text(render,newline='\n')
print(value['status'],len(functions),'EE listings',len(xfuncs),'Xbox listings',comparisons)
