from pathlib import Path
import json,hashlib,gzip,subprocess,re
own=Path(__file__).resolve().parent;r=own.parents[2].parent/'ps2-native-recomp-renderer-engine';d=r/'research/runs/parallel-renderer-engine-init-20261122';c=d/'main-reentry-correction';v=c/'validation-02'
def raw(p):return p.read_bytes() if p.exists() else gzip.decompress(p.with_name(p.name+'.gz').read_bytes())
def sha(b):return hashlib.sha256(b).hexdigest()
def load(p):return json.loads(p.read_text())
def check(p,h):assert sha(raw(p))==h,str(p)
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=r,text=True).strip()=='feb05096e43fb0ae37bac6407d2567569ddb03e0'
assert not subprocess.check_output(['git','status','--porcelain','--untracked-files=no'],cwd=r,text=True).strip()
j=load(v/'VERIFICATION.json');files=load(v/'SOURCE_FILES.json')
for p,h in files.items():check(r/p,h)
check(v/'SOURCE_FILES.json',j['source_manifest_sha256']);check(v/'core.json',j['core_receipt_sha256'])
receipts={'core':load(v/'core.json')}
for n,x in j['reused_unchanged_after_test_assertion_only'].items():
 p=Path(x['receipt']);check(p,x['receipt_sha256']);receipts[n]=load(p);assert receipts[n]['executable_sha256']==x['binary_sha256']
for n,x in receipts.items():
 check(Path(x['argv'][0]),x['executable_sha256']);p=v if n=='core' else c/'validation-01';check(p/(n+'.log'),x['log_sha256']);assert x['exit']==0
archives=load(c/'ARCHIVES.json')
for x in archives:
 check(c/x['archive'],x['archive_sha256']);b=gzip.decompress((c/x['archive']).read_bytes());assert sha(b)==x['raw_sha256']
for p,x in load(c/'BEFORE.json')['files'].items():
 check(c/x['archive'],x['archive_sha256']);assert sha(gzip.decompress((c/x['archive']).read_bytes()))==x['raw_sha256']
 b=subprocess.check_output(['git','show','dcf002267aa40e07dac7dc6539d49152a7cd6942:'+p],cwd=r);assert b.replace(b'\n',b'\r\n')==gzip.decompress((c/x['archive']).read_bytes()) or b==gzip.decompress((c/x['archive']).read_bytes())
negative=load(v/'full-entry-negative.json');assert negative['exit']!=0;check(v/'full-entry-negative.log',negative['log_sha256'])
names=[s.strip() for s in re.findall(r'undefined symbol: (.*)',raw(v/'full-entry-negative.log').decode())];assert len(names)==60 and set(names)==set(j['required_names'])==set(load(d/'validation-06/VERIFICATION.json')['remaining_undefined_symbols'])
old=load(c/'validation-01/SOURCE_FILES.json');changed=[p for p,h in files.items() if old.get(p)!=h];assert changed==['tests/test_retail_renderer_core.cpp'],changed
assert load(v/'build-observation.json')['exit']==0
pins={}
for p in ['runtime/src/native/retail_renderer_core.cpp','tests/test_retail_renderer_core.cpp']:
 b=(r/p).read_bytes();pins[p]=sha(b);q=own/(Path(p).name+'.gz');q.write_bytes(gzip.compress(b,mtime=0))
result={'candidate':'feb05096e43fb0ae37bac6407d2567569ddb03e0','superseded_for_transfer':'27029e9825e48b532b69044d820e5f3df1c26cb5','source_files':len(files),'archives':len(archives),'clients':receipts,'validation02_changed_from01':changed,'required_names':len(names),'verification_sha256':sha((v/'VERIFICATION.json').read_bytes()),'snapshots':pins,'reviewer_execution':'Read-only source/hash/receipt checks only. No build/tests rerun.'}
(own/'AUTHENTICATION.json').write_bytes((json.dumps(result,indent=2)+'\n').encode());print('PASS_SCOPED_TRANSFER_AUTHENTICATION',len(files),len(archives),len(names))
