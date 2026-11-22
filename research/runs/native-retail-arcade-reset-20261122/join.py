"""Read-only original arcade reset/data authentication and native constant generation."""
from pathlib import Path
import hashlib,json,struct
r=Path(__file__).resolve().parent;fork=r.parents[2];parent=fork.parents[2]
raw=(parent/'private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/SLUS_214.10').read_bytes()
sha=lambda b:hashlib.sha256(b).hexdigest()
assert sha(raw)=='a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f'
po=struct.unpack_from('<I',raw,28)[0];pe,pn=struct.unpack_from('<HH',raw,42)
segments=[struct.unpack_from('<8I',raw,po+i*pe) for i in range(pn)]
def mapped(a,n):
 s=next(s for s in segments if s[0]==1 and s[2]<=a<a+n<=s[2]+s[4]);o=s[1]+a-s[2];return o,raw[o:o+n]
checks=[]
for path in sorted((r/'source-01').glob('*.asm')):
 addresses=[]
 for line in path.read_text().splitlines():
  address,b,decoded=line.split('\t',2);a=int(address,16);offset,original=mapped(a,4);assert original==bytes.fromhex(b);addresses.append(a)
 assert addresses==list(range(addresses[0],addresses[-1]+4,4))
 checks.append(dict(path=path.relative_to(fork).as_posix(),sha256=sha(path.read_bytes()),begin=hex(addresses[0]),end=hex(addresses[-1]+4),words=len(addresses),role='implemented reset' if path.stem=='00334700' else 'source support only; not newly implemented'))
o,data=mapped(0x5e5630,19*4);pointers=struct.unpack('<19I',data);assert len(set(pointers))==19
rows=[];flattened=[]
for idx,a in enumerate(pointers):
 offset,values=mapped(a,8*16);words=struct.unpack('<32I',values)
 rows.append(dict(index=idx,vma=hex(a),offset=offset,size=len(values),bytes=values.hex()));flattened.append(words)
inc='// Immutable retail ladder rows; original mapping is in SOURCE_JOIN.json.\n'
for idx,words in enumerate(flattened):
 inc+='    RetailArcadeLadder{{\n'
 for i in range(8):inc+='        RetailArcadeLadderRow{{'+', '.join('0x%08xu'%v for v in words[i*4:i*4+4])+'}},\n'
 inc+='    }},\n'
tsv='ladder\ttier\tword0\tword1\tword2\tword3\n'
for idx,words in enumerate(flattened):
 for tier in range(8):tsv+='\t'.join(map(str,[idx,tier,*words[tier*4:tier*4+4]]))+'\n'
def freeze(name,text):
 path=r/name;content=text.encode()
 if path.exists():assert path.read_bytes()==content,name
 else:path.write_bytes(content)
freeze('GENERATED_DATA.inc',inc);freeze('expected-ladders.tsv',tsv)
storage=[]
for a in (0x64ed90,0x64ed8c,0x64cc84,0x64ed94):
 s=next(s for s in segments if s[0]==1 and s[2]<=a<a+4<=s[2]+s[5]);file=a+4<=s[2]+s[4]
 birth=raw[s[1]+a-s[2]:s[1]+a-s[2]+4] if file else b'\0'*4
 assert int.from_bytes(birth,'little')==(0xffffffff if a==0x64cc84 else 0)
 storage.append(dict(vma=hex(a),size=4,file_backed=file,loader_zero=not file,birth_bytes=birth.hex(),segment=s))
so=struct.unpack_from('<I',raw,32)[0];se,sn=struct.unpack_from('<HH',raw,46)
reg=next(struct.unpack_from('<10I',raw,so+i*se) for i in range(sn) if struct.unpack_from('<I',raw,so+i*se+4)[0]==0x70000006)
gp=struct.unpack_from('<I',raw,reg[4]+20)[0];assert gp==0x6540f0
result=dict(status='PASS_ORIGINAL_ARCADE_RESET_WORDS_AND_DATA',elf_sha256=sha(raw),language='r5900:LE:32:default',gp=hex(gp),functions=checks,listed_words=sum(x['words'] for x in checks),implemented_words=20,storage=storage,
 table=dict(vma='0x5e5630',offset=o,bytes=data.hex(),pointers=[hex(a) for a in pointers],rows=rows),expected_corpus_sha256=sha(tsv.encode()),generated_data_sha256=sha(inc.encode()),
 semantics=['334710 progress-1;334718 actual6602E8zero;334720 call-delay tier0','185C30(19) consumes one sameSession draw;334724 low16 mask','33472C selectedindex precedes334740 activepointer; native static rows replace original pointers','Initial selectedindex is actual ELFffffffff;otherthreefields are BSS zero','334520/334540/334750 prove sharedmode17/tier8/stride16; support consumers not newly implemented'],
 limits=['Full reset effect only; not complete arcade progression/fighter/resource behavior','No font, archive runtime or nonowning-reference shadows imported'])
freeze('SOURCE_JOIN.json',json.dumps(result,indent=2)+'\n')
reference=parent/'research/runs/native-retail-startup-20261120/connected-retail-entry-01/native-retail-random-12/reference-01/reference-vectors.tsv'
records=[line.split('\t') for line in reference.read_text().splitlines()[2:]];chosen={}
for i,row in enumerate(records):
 if row[1]=='SEED' and int(row[5])<623 and i+2<len(records) and records[i+1][1]=='NEXT' and records[i+2][1]=='NEXT' and records[i+1][4]=='1':
  seed=int(row[2])
  if seed in (0,1,3735928559,1985229328) and seed not in chosen:
   first,second=int(records[i+1][3]),int(records[i+2][3]);chosen[seed]=dict(seed=seed,first=first,selected=((first&65535)*19)>>16,next_after_reset=second,reference_case=row[0],reference_seed_consumed=int(row[5]))
assert len(chosen)==4
rng=dict(reference=str(reference),reference_sha256=sha(reference.read_bytes()),cases=list(chosen.values()),phase_proof='All chosen reference seed consumed counts<623; seed resets cursor while retaining consumed. Firsttwo draws do not twist in either reference or native test phase.')
freeze('RNG_EXPECTATIONS.json',json.dumps(rng,indent=2)+'\n')
print(result['status'],result['listed_words'],'source words;19 unique ladders/152 rows; selected-index birthffffffff')
