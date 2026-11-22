from pathlib import Path
import json,hashlib,gzip,struct,re,subprocess
review=Path(__file__).parent
root=review.parents[2].parent/'ps2-native-recomp-input-poll'
# Sibling isolated candidate; override only for relocated read-only review.
root=Path(r'C:\Users\nahsh\mk port\Armageddon-backup\armageddon-reimplementation\external\forks\ps2-native-recomp-input-poll')
d=root/'research/runs/parallel-controller-role-poll-20261122';v=d/'validation-09'
sha=lambda b:hashlib.sha256(b).hexdigest()
read=lambda p:p.read_bytes()
load=lambda p:json.loads(p.read_text())
commit='ad5dafda117839d720070416f32c5f0835cb0867'
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=root).decode().strip()==commit
assert not subprocess.check_output(['git','diff','HEAD','--','runtime','tests','cmake'],cwd=root)
j=load(d/'SOURCE_JOIN.json');b=Path(j['elf']).read_bytes();assert sha(b)==j['sha256']
phoff=struct.unpack_from('<I',b,28)[0];psz,pn=struct.unpack_from('<HH',b,42);loads=[struct.unpack_from('<8I',b,phoff+i*psz) for i in range(pn)]
count=0
for name,f in j['functions'].items():
 raw=gzip.decompress(read(d/'ee'/(name+'.gz')));assert sha(raw)==f['sha256'];lines=raw.decode().splitlines();assert len(lines)==f['words'];start=int(f['begin'],16);end=int(f['end_exclusive'],16);assert end-start==len(lines)*4
 for i,line in enumerate(lines):
  address,h,ins=line.split('\t',2);a=int(address,16);assert a==start+i*4
  p=next(p for p in loads if p[0]==1 and p[2]<=a<p[2]+p[4]);assert b[p[1]+a-p[2]:p[1]+a-p[2]+4]==bytes.fromhex(h)
  if ins=='UNDECODED':assert h=='00000000'
  count+=1
assert count==712
shoff=struct.unpack_from('<I',b,32)[0];ss,sn=struct.unpack_from('<HH',b,46);calls={k:[] for k in j['calls']}
for i in range(sn):
 sh=struct.unpack_from('<10I',b,shoff+i*ss)
 if not sh[2]&4:continue
 for off in range(sh[4],sh[4]+sh[5]-3,4):
  word=struct.unpack_from('<I',b,off)[0];pc=sh[3]+off-sh[4]
  if word>>26==3:
   target=hex(((pc+4)&0xf0000000)|((word&0x3ffffff)<<2))
   if target in calls:calls[target].append(hex(pc))
assert calls==j['calls']
pins={}
for name in ['SOURCE_FILES.json','ADDITIONAL_PINS.json']:
 values=load(v/name);assert isinstance(values,dict)
 for n,h in values.items():
  p=Path(n);p=p if p.is_absolute() else root/p;assert p.is_file() and sha(read(p))==h,(n,h)
 pins[name]={'entries':len(values),'sha256':sha(read(v/name))}
archives=load(d/'ARCHIVES.json')
for a in archives.values():
 raw=read(d/a['archive']);assert sha(raw)==a['archive_sha256'];assert sha(gzip.decompress(raw))==a['original_sha256']
clients={}
for n in ['input_admission','input_poll','input_selection','input_assignments']:
 a=load(v/(n+'.json'));assert a['exit']==0 and a['source_manifest_sha256']==pins['SOURCE_FILES.json']['sha256'];assert sha(read(Path(a['argv'][0])))==a['executable_sha256'];log=gzip.decompress(read(v/(n+'.log.gz')));assert sha(log)==a['log_sha256'];assert b'PASS' in log
 clients[n]={'executable_sha256':a['executable_sha256'],'log_sha256':a['log_sha256'],'receipt_sha256':sha(read(v/(n+'.json')))}
for n in ['build','full-entry-negative','symbols','imports']:
 a=load(v/(n+'.json'));assert sha(gzip.decompress(read(v/(n+'.log.gz'))))==a['log_sha256']
 if 'tool_sha256' in a:assert sha(read(Path(a['argv'][0])))==a['tool_sha256']
negative=gzip.decompress(read(v/'full-entry-negative.log.gz')).decode();symbols=[s.strip() for s in re.findall(r'undefined symbol: (.*)',negative)];assert len(symbols)==37 and symbols==load(v/'VERIFICATION.json')['required_missing_symbols']
nm=gzip.decompress(read(v/'symbols.log.gz')).decode();assert re.search(r' T SDL_InitSubSystem',nm)
result={'status':'PASS_BOUNDED_SOURCE_AND_SAVED_RESULTS_NO_CONCRETE_FINDING','candidate':commit,'base':load(v/'VERIFICATION.json')['base'],'original_words':count,'complete_functions':len(j['functions']),'caller_census':calls,'pins':pins,'archive_pairs':len(archives),'clients':clients,'negative_link_count':len(symbols),'candidate_verification_sha256':sha(read(v/'VERIFICATION.json')),'review_limits':'Saved results authenticated; no build/client reruns or full2214D0/Windows/disconnect/fighter acceptance. Historical37 not current34.'}
(review/'REVIEW.json').write_bytes((json.dumps(result,indent=2)+'\n').encode());print(json.dumps(result,indent=2))
