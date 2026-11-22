from pathlib import Path
import json,gzip,hashlib,subprocess,struct,re
src=Path(r'C:\Users\nahsh\mk port\Armageddon-backup\armageddon-reimplementation\external\forks\ps2-native-recomp-renderer-0401');d=src/'research/runs/parallel-renderer-plugin-0401-20261122';v=d/'validation-06';out=Path('research/runs/parallel-renderer-0401-review-20261122');out.mkdir(parents=True,exist_ok=True)
sha=lambda b:hashlib.sha256(b).hexdigest();pins={}
def check(p,h):
 b=p.read_bytes();assert sha(b)==h,str(p);pins[str(p)]=h;return b
files=json.loads((v/'SOURCE_FILES.json').read_text());index=json.loads((v/'COMMITTED_SOURCE_FILES.json').read_text())
for p,h in files.items():
 raw=check(src/p,h);b=subprocess.check_output(['git','-C',str(src),'show','fab3aee9:'+p]);assert sha(b)==index[p] and (raw==b or raw.replace(b'\r\n',b'\n')==b),p
for x in json.loads((d/'ARCHIVES.json').read_text()):
 b=check(d/x['archive'],x['archive_sha256']);raw=gzip.decompress(b);assert len(raw)==x['raw_bytes'] and sha(raw)==x['raw_sha256']
for p,h in json.loads((v/'ADDITIONAL_PINS.json').read_text()).items():check(Path(p),h)
j=json.loads((d/'SOURCE_JOIN.json').read_text());parent=src.parents[2];elf=check(parent/'research/inputs/SLUS_214.10',j['ELF_sha256']);xb=check(parent/'private-inputs/xbox-mka-20261121/default.xbe',j['Xbox_sha256'])
for p,x in j['full_listings'].items():
 b=check(d/p,x['sha256']);rows=[s.split('\t',2) for s in b.decode().splitlines()];assert len(rows)==x['words'];off=x['file_offset'];assert sha(elf[off:off+len(rows)*4])==x['bytes_sha256']
 for i,(a,h,_) in enumerate(rows):assert int(a,16)==int(x['begin'],16)+i*4 and bytes.fromhex(h)==elf[off+i*4:off+i*4+4]
for p,x in j['Xbox_listings'].items():
 rows=[s.split('\t',2) for s in check(d/p,x['sha256']).decode().splitlines()];assert len(rows)==len(x['mapped_spans'])
 for (a,h,_),(va,n,off) in zip(rows,x['mapped_spans']):assert int(a,16)==va and bytes.fromhex(h)==xb[off:off+n]
receipt=json.loads((v/'VERIFICATION.json').read_text());clients={'core':receipt['core'],**receipt['unchanged_allocator_session_from04']}
for n,x in clients.items():
 assert x['exit']==0;check(Path(x['argv'][0]),x['executable_sha256']);lp=(v if n=='core' else d/'validation-04')/(n+'.log.gz');assert sha(gzip.decompress(lp.read_bytes()))==x['log_sha256']
negative=gzip.decompress((v/'full-entry-negative.log.gz').read_bytes());names=[s.strip() for s in re.findall(r'undefined symbol: (.*)',negative.decode(errors='replace'))];assert names==receipt['all_required'] and len(names)==62
assert sha((src/'runtime/include/mka/retail_renderer_math.hpp').read_bytes())=='c88e4e9041d522cb258b3260ceda4365b52ad07be2b30ff660e68b8f5c1cb55e'
result={'status':'PASS_BOUNDED_INDEPENDENT_REVIEW_NO_FINDING','commit':'fab3aee9c3b5edcce771affb93581f8628ce0b91','source_files':len(files),'EE_words':j['total_words'],'Xbox_instructions':j['Xbox_instructions'],'clients':clients,'negative_names':len(names),'source_join_sha256':sha((d/'SOURCE_JOIN.json').read_bytes()),'verification_sha256':sha((v/'VERIFICATION.json').read_bytes()),'no_build_or_execution':True}
(out/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n');(out/'PINS.json').write_text(json.dumps(pins,indent=2)+'\n');print(result['status'],len(files),j['total_words'],j['Xbox_instructions'],len(pins))
