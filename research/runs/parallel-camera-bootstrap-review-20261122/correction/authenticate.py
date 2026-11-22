from pathlib import Path
import hashlib,json,gzip,re,subprocess
r=Path(__file__).resolve().parent;f=r.parents[3];target=f.parent/'ps2-native-recomp-scene-continuation';record=target/'research/runs/parallel-scene-bootstrap-continuation-20261122';c=record/'owner-boundary-correction';vdir=c/'validation-01';commit='feb629e8fcd4dabefef1cb687393459475d3c74f';base='f2020665dcd02145cb9d65df926ac53e2c0303b2'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=target,text=True).strip()==commit
assert subprocess.check_output(['git','rev-parse',commit+'^'],cwd=target,text=True).strip()==base
assert not subprocess.check_output(['git','status','--porcelain','-uno'],cwd=target,text=True).strip()
v=json.loads((vdir/'VERIFICATION.json').read_text());assert v['status']=='PASS_CORRECTED_CAMERA_DOMAIN_OWNER_BOUNDARY' and v['base_commit']==base
pins=json.loads((vdir/'SOURCE_FILES.json').read_text());assert len(pins)==v['source_files']==839 and sha(vdir/'SOURCE_FILES.json')==v['source_manifest_sha256'];assert all(sha(target/n)==h for n,h in pins.items())
old=json.loads((record/'validation-05/SOURCE_FILES.json').read_text());old.update(json.loads((record/'validation-05/SUPPLEMENTAL_UNCHANGED_INPUTS.json').read_text()));assert set(old)==set(pins)
changed=sorted(n for n,h in old.items() if pins[n]!=h);assert changed==['runtime/include/mka/retail_camera_world.hpp','tests/test_retail_camera_bootstrap.cpp']
prior=json.loads((c/'BEFORE.json').read_text());assert prior['base']==base
for n,h in prior['pins'].items():
 assert sha(c/'source-before'/n)==h==old[n]
 frozen=subprocess.check_output(['git','show',base+':'+n],cwd=target);assert hashlib.sha256(frozen).hexdigest()==h
# Exact target header: compile private traits against this frozen copy later.
header=subprocess.check_output(['git','show',commit+':runtime/include/mka/retail_camera_world.hpp'],cwd=target);assert hashlib.sha256(header).hexdigest()==pins['runtime/include/mka/retail_camera_world.hpp'];p=r/'frozen/mka/retail_camera_world.hpp';p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(header)
build=target/'build-camera-continuation';clients={};receipts={}
for name,item in v['clients'].items():
 exe=build/(item['target']+'.exe');receipt=json.loads((vdir/(name+'.json')).read_text());raw=gzip.decompress((vdir/(name+'.log.gz')).read_bytes());assert receipt['exit']==0 and receipt['executable_sha256']==item['sha256']==sha(exe);assert receipt['source_manifest_sha256']==v['source_manifest_sha256'] and hashlib.sha256(raw).hexdigest()==receipt['log_sha256'] and b'PASS ' in raw
 sym=json.loads((vdir/(name+'-symbols.json')).read_text());log=gzip.decompress((vdir/(name+'-symbols.log.gz')).read_bytes());assert sym['exit']==0 and hashlib.sha256(log).hexdigest()==sym['log_sha256'] and sym['source_manifest_sha256']==v['source_manifest_sha256'];assert not re.search(rb'(?<![A-Za-z0-9_])(?:ps2|gs|spu2|sif|Cpu|recomp)::|\b(?:g_ram|g_spr)\b|SDL_OpenAudio',log)
 if name=='camera':assert b' T SDL_InitSubSystem' in log and b'__imp_SDL_InitSubSystem' not in log
 assert 'reused_from' not in receipt;clients[name]=dict(target=item['target'],binary_sha256=sha(exe),execution_receipt_sha256=sha(vdir/(name+'.json')),execution_log_sha256=receipt['log_sha256'],symbols_receipt_sha256=sha(vdir/(name+'-symbols.json')),symbols_log_sha256=sym['log_sha256'])
for name in ['build','full-entry-negative']:
 receipt=json.loads((vdir/(name+'.json')).read_text());raw=gzip.decompress((vdir/(name+'.log.gz')).read_bytes());assert hashlib.sha256(raw).hexdigest()==receipt['log_sha256'] and receipt['source_manifest_sha256']==v['source_manifest_sha256'];assert sha(receipt['argv'][0])==receipt['executable_sha256']
 if name=='build':
  assert receipt['exit']==0
  for item in v['clients'].values():assert item['target'] in receipt['argv']
 else:
  assert receipt['exit']!=0;missing=re.findall(r'undefined symbol: ([^\r\n]+)',raw.decode());previous=json.loads((record/'validation-05/VERIFICATION.json').read_text())['remaining_undefined_symbols'];assert len(missing)==len(set(missing))==37 and sorted(missing)==sorted(previous)==sorted(v['remaining_undefined_symbols']);assert any('continue_first_scene_at_220900' in n for n in missing) and any('continue_scene_reset_at_125d60' in n for n in missing);assert not any('initialize_retail_first_scene(' in n or 'continue_scene_reset_at_181a30(' in n for n in missing)
 receipts[name]=dict(receipt_sha256=sha(vdir/(name+'.json')),log_sha256=receipt['log_sha256'],exit=receipt['exit'])
tools=f.parents[2].parent/'dependencies/compiler/llvm-mingw-20261006-ucrt-x86_64/bin';assert sha(tools/'clang++.exe')==v['compiler_sha256'];assert sha(build/'CMakeCache.txt')==v['cache_sha256']
# Only source ownership restriction/assertions changed;reuse previous original
# function review after matching its exact saved join identity.
old_audit=json.loads((r.parent/'AUDIT.json').read_text());assert sha(record/'SOURCE_JOIN.json')==old_audit['source_join_sha256']
result=dict(status='PASS_CAMERA_OWNER_CORRECTION_SAVED_IDENTITIES',target_commit=commit,parent_commit=base,source_manifest_sha256=sha(vdir/'SOURCE_FILES.json'),source_count=len(pins),changed_sources=changed,source_before_sha256=sha(c/'BEFORE.json'),author_result_sha256=sha(vdir/'VERIFICATION.json'),clients=clients,receipts=receipts,prior_original_source_review_sha256=sha(r.parent/'AUDIT.json'),frozen_header_sha256=hashlib.sha256(header).hexdigest(),scope='Independent saved-result/source audit;no delivery builds/runs. Runtime interpretation limited to original bounded camera/count/link component,all required suffixes still unbound.')
(r/'AUDIT.json').write_text(json.dumps(result,indent=2)+'\n',newline='\n');print(result['status'])
