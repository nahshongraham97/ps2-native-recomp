from pathlib import Path
import json,hashlib,gzip,subprocess,re,runpy
own=Path(__file__).resolve().parent;r=own.parents[2].parent/'ps2-native-recomp-renderer-engine';d=r/'research/runs/parallel-renderer-engine-init-20261122';v=d/'validation-06'
def raw(p):return p.read_bytes() if p.exists() else gzip.decompress(p.with_name(p.name+'.gz').read_bytes())
def sha(b):return hashlib.sha256(b).hexdigest()
def load(p):return json.loads(p.read_text())
def check(p,h):assert sha(raw(p))==h,str(p)
assert subprocess.run(['git','merge-base','--is-ancestor','dcf002267aa40e07dac7dc6539d49152a7cd6942','HEAD'],cwd=r).returncode==0
assert not subprocess.check_output(['git','status','--porcelain','--untracked-files=no'],cwd=r,text=True).strip()
a=load(d/'ARCHIVES.json')
for x in a:
 check(d/x['archive'],x['archive_sha256']);b=gzip.decompress((d/x['archive']).read_bytes());assert sha(b)==x['raw_sha256'] and len(b)==x['raw_bytes']
j=load(v/'VERIFICATION.json');files=load(v/'SOURCE_FILES.json')
for p,h in files.items():check(r/p,h)
committed=load(v/'COMMITTED_SOURCE_FILES.json')
for p,h in committed.items():assert sha(subprocess.check_output(['git','show','dcf002267aa40e07dac7dc6539d49152a7cd6942:'+p],cwd=r))==h,p
for p,h in load(v/'ADDITIONAL_PINS.json').items():check(Path(p),h)
for key,name in [('source_manifest_sha256','validation-06/SOURCE_FILES.json'),('source_join_sha256','SOURCE_JOIN.json'),('xbox_join_sha256','XBOX_WORD_JOIN.json'),('additional_pins_sha256','validation-06/ADDITIONAL_PINS.json'),('committed_source_manifest_sha256','validation-06/COMMITTED_SOURCE_FILES.json')]:check(d/name,j[key])
for name,c in j['clients'].items():
 check(Path(c['argv'][0]),c['executable_sha256']);log=d/'validation-03/content.log' if name.startswith('content_prior') else v/(name+'.log');check(log,c['log_sha256']);assert c['exit']==0
cache=r/'build-renderer-engine/CMakeCache.txt';check(cache,j['cache_sha256'])
for k,typ in [('CMAKE_CXX_COMPILER','compiler_sha256'),('CMAKE_NM','symbol_tool_sha256')]:check(Path(re.search(k+r':(?:STRING|FILEPATH)=(.*)',cache.read_text())[1]),j[typ])
# Read-only source-byte verifier inspected before execution; no build/test or writes.
runpy.run_path(str(d/'verify_source.py'))
prior=load(r/'research/runs/native-retail-main-frame-gates-20261122/validation-01/VERIFICATION.json')['remaining_undefined_symbols'];expected=set(prior);assert len(expected)==34
expected.remove('mka::retail::scene_bootstrap_required::continue_first_scene_at_220900(mka::retail::NativeRetailMainState&, mka::retail::StartupFailure&)')
expected.add('mka::retail::scene_bootstrap_required::continue_first_scene_at_225d40(mka::retail::NativeRetailMainState&, std::__1::shared_ptr<mka::retail::NativeRetailRendererCore>, mka::retail::StartupFailure&)')
for ident in ['0401','0402','0403','0404','0405','0406','0407','0408','0409','0412','040a','040b']:
 expected.add('mka::retail::renderer_required::construct_core_plugin_'+ident+'(mka::retail::NativeRetailRendererCore&, mka::retail::RetailRendererCoreFailure&)');expected.add('mka::retail::renderer_required::destroy_core_plugin_'+ident+'(mka::retail::NativeRetailRendererCore&)')
expected.add('mka::retail::renderer_required::register_object_extensions_1b72b0(mka::retail::NativeRetailRendererCore&, mka::retail::RetailRendererCoreFailure&)');expected.add('mka::retail::renderer_required::discard_object_extensions_after_core_init(mka::retail::NativeRetailRendererCore&)')
actual=[s.strip() for s in re.findall(r'undefined symbol: (.*)',raw(v/'full-entry-negative.log').decode())];assert len(actual)==60 and set(actual)==expected==set(j['remaining_undefined_symbols'])
sy=raw(v/'core-symbols.log').decode();assert re.search(r' T SDL_InitSubSystem',sy) and '__imp_SDL_InitSubSystem' not in sy
snapshot={}
for p in ['runtime/include/mka/retail_renderer_core.hpp','runtime/src/native/retail_renderer_core.cpp','runtime/src/native/retail_first_scene_core.cpp','runtime/src/native/content_volume.cpp','tests/test_retail_renderer_core.cpp']:
 b=(r/p).read_bytes();dest=own/'snapshot'/p;dest.parent.mkdir(parents=True,exist_ok=True);dest.with_suffix(dest.suffix+'.gz').write_bytes(gzip.compress(b,mtime=0));snapshot[p]=sha(b)
result={'candidate':'dcf002267aa40e07dac7dc6539d49152a7cd6942','base':j['base'],'archives':len(a),'raw_source_files':len(files),'index_source_files':len(committed),'additional_pins':len(load(v/'ADDITIONAL_PINS.json')),'EE_full_listings':28,'EE_words':999,'Xbox_functions':len(load(d/'XBOX_WORD_JOIN.json')['functions']),'authenticated_clients':list(j['clients']),'negative_names':len(actual),'snapshot':snapshot,'verification_receipt_sha256':sha((v/'VERIFICATION.json').read_bytes()),'executed_by_reviewer':'Byte/hash/listing/receipt checks only; no tests/builds/nm or gameplay. Delivery author executed saved clients.'}
(own/'AUTHENTICATION.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))



