from pathlib import Path
import json,hashlib,struct,gzip,subprocess,re
out=Path(__file__).parent;r=out.parents[2].parent/'ps2-native-recomp';clean=r.parent/'clean-arcade-22';d=r/'research/runs/native-retail-arcade-reset-20261122';v=d/'clean-validation-01';commit='7bf4ec5a613ca1d69abe9cb19bb47fb75ffeaa78'
sha=lambda b:hashlib.sha256(b).hexdigest()
raw=lambda p:p.read_bytes() if p.exists() else gzip.decompress(p.with_name(p.name+'.gz').read_bytes())
load=lambda p:json.loads(p.read_text())
j=load(d/'SOURCE_JOIN.json');elf=(r/'../../../research/inputs/SLUS_214.10').resolve().read_bytes();assert sha(elf)==j['elf_sha256']
po=struct.unpack_from('<I',elf,28)[0];ps,pn=struct.unpack_from('<HH',elf,42);segments=[struct.unpack_from('<8I',elf,po+i*ps) for i in range(pn)]
def data(a,n):
 s=next(s for s in segments if s[0]==1 and s[2]<=a and a+n<=s[2]+s[4]);off=s[1]+a-s[2];return elf[off:off+n]
words=0
for f in j['functions']:
 p=r/f['path'];lines=p.read_text().splitlines();start=int(f['begin'],16);assert len(lines)==f['words'] and sha(p.read_bytes())==f['sha256']
 for i,line in enumerate(lines):
  a,h,_=line.split('\t',2);assert int(a,16)==start+i*4 and data(int(a,16),4)==bytes.fromhex(h)
 words+=len(lines)
assert words==360 and j['implemented_words']==20
pointers=struct.unpack('<19I',data(0x5e5630,76));assert len(set(pointers))==19;values=[w for p in pointers for w in struct.unpack('<32I',data(p,128))]
inc=(clean/'runtime/src/native/retail_arcade_ladder_data.inc').read_text();native=[int(v,16) for v in re.findall(r'0x([0-9a-f]{8})u',inc)];assert native==values and len(values)==608
rows=(d/'expected-ladders.tsv').read_text().splitlines()[1:];assert len(rows)==152
for i,row in enumerate(rows):
 a=list(map(int,row.split('\t')));assert a[:2]==[i//8,i%8] and a[2:]==values[i*4:i*4+4]
for f in j['storage']:
 a=int(f['vma'],16);s=next(s for s in segments if s[0]==1 and s[2]<=a and a+4<=s[2]+s[5]);actual=data(a,4) if a+4<=s[2]+s[4] else b'\0'*4;assert actual.hex()==f['birth_bytes'];assert int.from_bytes(actual,'little')==(0xffffffff if a==0x64cc84 else 0)
rng=load(d/'RNG_EXPECTATIONS.json');ref=Path(rng['reference']);assert sha(ref.read_bytes())==rng['reference_sha256'];records=[l.split('\t') for l in ref.read_text().splitlines()[2:]];phase=[]
for native_start,c in enumerate(rng['cases']):
 matches=[]
 for i,row in enumerate(records[:-2]):
  if row[1]=='SEED' and int(row[2])==c['seed'] and row[0]==c['reference_case'] and int(row[5])==c['reference_seed_consumed'] and records[i+1][1]==records[i+2][1]=='NEXT':
   first,second=records[i+1:i+3]
   if first[0]==second[0]==row[0] and first[4]=='1' and second[4]=='2':matches.append((row,first,second))
 assert matches;row,first,second=matches[0];consumed=int(row[5]);assert consumed<623 and int(first[5])==consumed+1 and int(second[5])==consumed+2
 assert int(first[3])==c['first'] and int(second[3])==c['next_after_reset'] and ((int(first[3])&65535)*19)>>16==c['selected']
 assert native_start*2<623;phase.append({'seed':c['seed'],'reference_pre_consumed':consumed,'native_case_pre_consumed':native_start*2,'both_draws_before_twist':True})
manifest=load(v/'SOURCE_FILES.json');receipt=load(v/'VERIFICATION.json');assert receipt['source_commit']==commit and receipt['clean_source'] and len(receipt['clients'])==6 and len(receipt['remaining_undefined_symbols'])==33
assert sha((v/'SOURCE_FILES.json').read_bytes())==receipt['source_manifest_sha256']
blobs={}
for line in subprocess.check_output(['git','ls-tree','-r',commit,'--','runtime','recomp','cmake','tests','CMakeLists.txt'],cwd=r).decode().splitlines():
 meta,path=line.split('\t');blobs[path]=meta.split()[-1]
assert blobs.keys()==manifest.keys() and len(manifest)==856
for name,h in manifest.items():
 b=(clean/name).read_bytes();assert sha(b)==h and hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()==blobs[name]
clients={}
for c in receipt['clients']:
 n=c['name'];a=load(v/(n+'.json'));assert a['exit']==0 and a['source_commit']==commit and a['executable_sha256']==c['sha256']==sha(Path(a['argv'][0]).read_bytes());log=raw(v/(n+'.log'));assert sha(log)==a['log_sha256'] and b'PASS' in log;clients[n]={'exe':c['sha256'],'log':sha(log)}
actual=[s.strip() for s in re.findall(r'undefined symbol: (.*)',raw(v/'full-entry-negative.log').decode())];assert actual==receipt['remaining_undefined_symbols']==load(d/'REQUIRED_EFFECTS_EXPECTED.json')['remaining_undefined_symbols'];assert not any('reset_selected_resource_334700' in s for s in actual)
result={'status':'PASS_BOUNDED_RESET_REVIEW_NO_CONCRETE_FINDING','source_commit':commit,'source_words':words,'implemented_words':20,'pointer_rows':[19,152],'source_blobs':len(manifest),'clients':clients,'rng_phase':phase,'reference_sha256':rng['reference_sha256'],'required_names':len(actual),'receipt_sha256':sha((v/'VERIFICATION.json').read_bytes()),'limits':'Saved clean artifacts authenticated,no rebuild/client execution. First-two-vector proof is before-twist only;doesnot reverify whole RNG or arcade progression.'}
(out/'REVIEW.json').write_bytes((json.dumps(result,indent=2)+'\n').encode());print('PASS360_WORDS_19POINTERS_152ROWS_856BLOBS_6SAVEDCLIENTS_33NAMES_RNG_PHASE')
