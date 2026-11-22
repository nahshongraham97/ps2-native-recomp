from pathlib import Path
import struct,json,hashlib
r=Path(__file__).resolve().parent;f=r.parents[2];parent=f.parents[2]
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
image=parent/'private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/modules/msl.irx';raw=image.read_bytes();assert sha(image)=='f62dfbcedd39bc92cb8b9c02bb685b956a10357f9181d7f8c2801d8eaff07af7'
o=struct.unpack_from('<I',raw,28)[0];size,count=struct.unpack_from('<HH',raw,42);segments=[struct.unpack_from('<8I',raw,o+i*size) for i in range(count)];segments=[s for s in segments if s[0]==1]
def extract(a,n):
 s=next(s for s in segments if s[2]<=a and a+n<=s[2]+s[4]);off=s[1]+a-s[2];return dict(vma=hex(a),size=n,offset=off,segment=list(s),bytes=raw[off:off+n].hex())
v=json.loads((r/'source-final/LISTING.json').read_text());assert v['language']=='MIPS:LE:32:default' and v['image_base']=='00000000'
rows=v['instructions'];assert all(x['instruction'] is not None for x in rows)
for row in rows:assert int(row['word'],16)==int.from_bytes(bytes.fromhex(extract(int(row['original_vma'],16),4)['bytes']),'little')
ranges=[(0xa49c,0xa598),(0x6330,0x6468),(0x6468,0x6534),(0x3fcc,0x4180),(0x9534,0x959c),(0x959c,0x9f1c),(0x9f1c,0xa0ec),(0xb7a8,0xb89c),(0xb8ac,0xb8d8),(0xbeb8,0xbf3c),(0xbf3c,0xbfbc),(0xbfbc,0xc0d8),(0xc1e8,0xc2f0),(0xc2f0,0xc53c),(0xba68,0xbb70)]
functions={}
for a,b in ranges:
 part=[x for x in rows if a<=int(x['original_vma'],16)<b];assert [int(x['original_vma'],16) for x in part]==list(range(a,b,4));functions[hex(a)]=dict(extract(a,b-a),instructions=len(part),full_contiguous=True)
segment=segments[0];code=bytes.fromhex(extract(0,segment[4])['bytes']);calls=[]
for a in range(0,len(code),4):
 w=struct.unpack_from('<I',code,a)[0]
 if w>>26==3 and (w&0x3ffffff)*4 in [0xa49c,0x6330,0xc2f0,0x288,0xbf3c,0xc1e8]:calls.append(dict(address=hex(a),target=hex((w&0x3ffffff)*4),words=extract(max(0,a-20),min(28,len(code)-max(0,a-20)))))
# Own Xbox section mapping and full original instruction words/byte lengths.
ximage=parent/'private-inputs/xbox-mka-20261121/default.xbe';xb=ximage.read_bytes();assert sha(ximage)=='78f99fca447fddc5c5116c09d27e6f1c04af8572dace94f559966588c6f5e393';base=struct.unpack_from('<I',xb,0x104)[0];count,table=struct.unpack_from('<II',xb,0x11c);sections=[struct.unpack_from('<10I',xb,table-base+i*56) for i in range(count)]
xf={}
for p in sorted(r.glob('xbox-source-*/*.asm')):
 items=[x.split('\t',2) for x in p.read_text().splitlines()];begin=int(items[0][0],16);end=int(items[-1][0],16)+len(bytes.fromhex(items[-1][1]));n=0
 for a,h,_ in items:
  a=int(a,16);n+=len(bytes.fromhex(h));s=next(s for s in sections if s[1]<=a and a+len(bytes.fromhex(h))<=s[1]+s[4]);off=s[3]+a-s[1];assert xb[off:off+len(bytes.fromhex(h))].hex()==h
 assert end-begin==n;xf[p.relative_to(r).as_posix()]=dict(begin=hex(begin),end_exclusive=hex(end),instructions=len(items),sha256=hashlib.sha256(bytes.fromhex(''.join(x[1] for x in items))).hexdigest(),full_contiguous=True)
ref=Path('../ps2-native-recomp-backup-20261110/runtime/src/iop/spu2.cpp').resolve();native=f/'runtime/src/native/host_voice.cpp'
manual=parent/'private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/.agents/skills/ps2-recomp/resources/pdfs/SPU2_Overview_Manual.pdf'
value=dict(status='PASS_NORMAL_SETUP_SOURCE_JOINS',module=dict(path=str(image),sha256=sha(image),language=v['language'],functions=functions,libsd_import=extract(0x268,0x60),calls=calls),xbox=dict(path=str(ximage),sha256=sha(ximage),functions=xf),manual=dict(path=str(manual),sha256=sha(manual),printed_pages=[24,25,26,45,71,77,78],version='6.0'),reference_only=dict(preserved_oracle_source=str(ref),sha256=sha(ref),native_generic_host_voice_sha256=sha(native)),limits=['No exact native timing-profile realization is proved by approximate rate tables','Oracle sustain127 behavior differs from documented infinity','Early orientation ranges ba78 and96c4 began inside functions;final original entry/full ranges supersede them','No donor envelope numerical equivalence established'])
p=r/'SOURCE_JOIN.json';render=json.dumps(value,indent=2)+'\n'
if p.exists():assert json.loads(p.read_text())==json.loads(render)
else:p.write_text(render,newline='\n')
print(value['status'],sum(x['instructions'] for x in functions.values()),'module words',sum(x['instructions'] for x in xf.values()),'Xbox instructions')
