"""Read-only independent source and saved-result review of frozenf202;no candidate builds."""
from pathlib import Path
import hashlib,json,gzip,subprocess,struct,re
r=Path(__file__).resolve().parent;f=r.parents[2];target=f.parent/'ps2-native-recomp-scene-continuation';record=target/'research/runs/parallel-scene-bootstrap-continuation-20261122';commit='f2020665dcd02145cb9d65df926ac53e2c0303b2'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=target,text=True).strip()==commit
delivery_dirty=subprocess.check_output(['git','status','--porcelain','-uno'],cwd=target,text=True).strip()
def check_frozen_source(n,h):
 p=target/n
 if sha(p)==h:return True
 frozen=subprocess.check_output(['git','show',commit+':'+n],cwd=target)
 return hashlib.sha256(frozen).hexdigest()==h
a=json.loads((record/'validation-04/VERIFICATION.json').read_text());v=json.loads((record/'validation-05/VERIFICATION.json').read_text());assert v['status']=='PASS_CONNECTED_CAMERA_BOOTSTRAP_COMPONENTS_FINAL'
assert sha(record/'validation-05/SOURCE_FILES.json')==v['source_manifest_sha256']
inputs=json.loads((record/'validation-05/SOURCE_FILES.json').read_text());assert len(inputs)==v['measured_source_count']==689
assert all(check_frozen_source(n,h) for n,h in inputs.items())
supp=json.loads((record/'validation-05/SUPPLEMENTAL_UNCHANGED_INPUTS.json').read_text());assert len(supp)==v['supplemental_unchanged_input_count']==150 and all(check_frozen_source(n,h) for n,h in supp.items())
assert sha(record/'validation-05/SUPPLEMENTAL_UNCHANGED_INPUTS.json')==v['supplemental_manifest_sha256']
elf=f.parents[2]/'research/inputs/SLUS_214.10';blob=elf.read_bytes();assert sha(elf)=='a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f'
o=struct.unpack_from('<I',blob,28)[0];size,count=struct.unpack_from('<HH',blob,42);segments=[struct.unpack_from('<8I',blob,o+i*size) for i in range(count)];segments=[s for s in segments if s[0]==1]
proof=json.loads((record/'SOURCE_JOIN.json').read_text());assert sha(record/'SOURCE_JOIN.json')==v['source_join_sha256'] and proof['language']=='r5900:LE:32:default'
words=0
for entry in proof['functions']:
 p=target/entry['path'];assert sha(p)==entry['sha256'];begin=int(entry['begin'],16);end=int(entry['end'],16);s=next(s for s in segments if s[2]<=begin and end<=s[2]+s[4]);off=s[1]+begin-s[2];assert off==entry['offset'] and blob[off:off+end-begin].hex()==entry['original_bytes'];rows=[line.split('\t',2) for line in p.read_text().splitlines()];addresses=[int(row[0],16) for row in rows];assert len(rows)==entry['words']
 for x,h,_ in rows:
  at=int(x,16);raw=blob[s[1]+at-s[2]:s[1]+at-s[2]+4];assert raw.hex()==h
 missing=set(range(begin,end,4))-set(addresses);assert missing=={int(x,16) for x in entry['omitted']}
 for at in missing:assert blob[s[1]+at-s[2]:s[1]+at-s[2]+4]==bytes(4)
 words+=len(rows)
for b in proof['storage_birth']:
 at=int(b['vma'],16);s=next(s for s in segments if s[2]<=at and at+b['size']<=s[2]+s[5]);assert at>=s[2]+s[4] and b['loader_zero'] and not b['file_backed']
assert sha(record/'SOURCE_JOIN.before-producer-context.json')==v['source_join_before_producer_context_sha256']==a['source_join_sha256']
build=target/'build-camera-continuation';clients={}
for target_name,receipt_path in [('test_native_retail_camera_bootstrap','validation-05/camera'),('test_native_retail_scene_bootstrap_prefix','validation-04/prefix'),('test_native_retail_main_timing','validation-04/stopwatch'),('test_native_retail_game_session','validation-04/session')]:
 c=json.loads((record/(receipt_path+'.json')).read_text());raw=gzip.decompress((record/(receipt_path+'.log.gz')).read_bytes());binary=build/(target_name+'.exe');assert c['exit']==0 and sha(binary)==c['executable_sha256']==v['clients'][target_name]['binary_sha256'];assert hashlib.sha256(raw).hexdigest()==c['log_sha256'];assert c['source_manifest_sha256']==(v['source_manifest_sha256'] if 'validation-05' in receipt_path else a['source_manifest_sha256']);assert b'PASS ' in raw
 symbols=gzip.decompress((record/v['clients'][target_name]['symbols_log']).read_bytes());assert hashlib.sha256(symbols).hexdigest()==v['clients'][target_name]['log_sha256'];assert not re.search(rb'(?<![A-Za-z0-9_])(?:ps2|gs|spu2|sif|Cpu|recomp)::|\b(?:g_ram|g_spr)\b|SDL_OpenAudio',symbols)
 clients[target_name]=dict(binary_sha256=sha(binary),execution_receipt_sha256=sha(record/(receipt_path+'.json')),execution_log_sha256=c['log_sha256'],fresh='validation-05' in receipt_path)
for reused in v['reused_executions']:assert sha(target/reused['receipt'])==reused['receipt_sha256']
negative=json.loads((record/'validation-04/full-entry-negative.json').read_text());raw=gzip.decompress((record/'validation-04/full-entry-negative.log.gz').read_bytes());assert negative['exit']!=0 and hashlib.sha256(raw).hexdigest()==negative['log_sha256'];names=re.findall(r'undefined symbol: ([^\r\n]+)',raw.decode());assert len(names)==len(set(names))==37 and sorted(names)==sorted(v['remaining_undefined_symbols']);assert sha(record/'validation-04/full-entry-negative.json')==v['reused_negative_link']['receipt_sha256']
assert sha(build/'libmka_native_retail_startup.a')==v['production_archive_sha256']
compiler=f.parents[2].parent/'dependencies/compiler/llvm-mingw-20261006-ucrt-x86_64/bin';assert sha(compiler/'clang++.exe')==v['compiler_sha256'];assert sha(compiler/'llvm-nm.exe')==v['clients']['test_native_retail_camera_bootstrap']['tool_sha256'];assert sha(build/'CMakeCache.txt')==v['build_cache_sha256']
result=dict(status='PASS_SAVED_SOURCE_RESULT_IDENTITIES_WITH_ONE_OWNERSHIP_FINDING',target_commit=commit,author_fix_dirty_paths=delivery_dirty,measured_sources=len(inputs),supplemental_sources=len(supp),original_functions=len(proof['functions']),original_listed_words=words,source_join_sha256=sha(record/'SOURCE_JOIN.json'),target_verification_sha256=sha(record/'validation-05/VERIFICATION.json'),clients=clients,remaining_named_effects=names,limits='Read-only saved-source/results review;no candidate build/run;public constructor/copy ownership escape requires correction;no renderer/full-game acceptance')
(r/'AUDIT.json').write_text(json.dumps(result,indent=2)+'\n',newline='\n');print(result['status'])
