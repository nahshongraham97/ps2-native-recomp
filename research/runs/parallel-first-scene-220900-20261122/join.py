from pathlib import Path
import hashlib,json,struct
r=Path(__file__).resolve().parent;f=r.parents[2];elf=f.parents[2]/'research/inputs/SLUS_214.10';b=elf.read_bytes();sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();assert sha(elf)=='a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f'
o=struct.unpack_from('<I',b,28)[0];sz,n=struct.unpack_from('<HH',b,42);segments=[struct.unpack_from('<8I',b,o+i*sz) for i in range(n)];segments=[s for s in segments if s[0]==1]
def get(a,n):
 s=next(s for s in segments if s[2]<=a and a+n<=s[2]+s[4]);off=s[1]+a-s[2];return dict(vma=hex(a),size=n,offset=off,segment=list(s),bytes=b[off:off+n].hex())
keep={'00220900','00220950','00220970','002209a0','002209b0','001a1890','001a6dd0','00311390','00429a90','00429c50','00429d20','004295b0','004293f0','0042c1d0','0042bb00','0042c080','001a1a30','001a1b30','001a2020','00127260'}
expected={0x429b54:(0,'unreachable SLL zero,zero,0 after B429B4C with present delay429B50'),0x42c150:(0x102d,'unreachable DADDU v0,zero,zero after B42C148 with present delay42C14C'),0x42c154:(0x10000002,'unreachable BEQ zero,zero,target42C160;PC+4+signext(2)*4'),0x42c158:(0,'unreachable SLL zero,zero,0 in42C154 delay slot')}
functions={}
for p in sorted(r.glob('source-*/*.asm')):
 if p.stem not in keep:continue
 rows=[x.split('\t',2) for x in p.read_text().splitlines()];a=[int(x[0],16) for x in rows];begin,end=min(a),max(a)+4;missing=sorted(set(range(begin,end,4))-set(a));assert all(x in expected for x in missing)
 for at,h,_ in rows:assert get(int(at,16),4)['bytes']==h
 for at in missing:assert int.from_bytes(bytes.fromhex(get(at,4)['bytes']),'little')==expected[at][0]
 functions[p.relative_to(r).as_posix()]=dict(get(begin,end-begin),listed_words=len(rows),missing_decoded={hex(x):expected[x] for x in missing})
assert len(functions)==20
# Preserve actual runtime table birth separately from the stores220900 performs.
for a in [0x6aa750,0x64ec40]:
 s=next(s for s in segments if s[2]<=a<s[2]+s[5]);assert a>=s[2]+s[4]
data=dict(permanent_name=get(0x631898,16),callbacks='malloc2209B0/free2209A0/realloc220950/calloc220970;installed in that order by220900;copied by1A1890',one_mib_consumer='1A6DD0 stores69F3E4;1A1B30 passes to1A1A30;1A1A30 mallocs an arena and1A3BE0 prepares suballocation. Native owner has no such pool ceiling.')
assert bytes.fromhex(data['permanent_name']['bytes']).startswith(b'Permanent heap\x00')
v=dict(status='PASS_FIRST_SCENE_ALLOCATOR_ORIGINAL_WORDS',image=str(elf),sha256=sha(elf),language='r5900:LE:32:default',functions=functions,data=data,limits=['225D40 exploratory Ghidra body is disjoint and not a full-span proof;not implemented','Entire1A6DD0 renderer init and later127260 effects remain required','Original source calloc has deliberate32-bit product/round wrapping;ordinary native allocation capacity remains host-managed','No Xbox donor adoption needed for these directly verified portable allocation algorithms'])
p=r/'SOURCE_JOIN.json';text=json.dumps(v,indent=2)+'\n'
if p.exists():assert json.loads(p.read_text())==json.loads(text)
else:p.write_text(text,newline='\n')
print(v['status'],sum(x['listed_words'] for x in functions.values()))
