from pathlib import Path
import json,hashlib,subprocess,re,gzip
r=Path(__file__).resolve().parents[3];d=Path(__file__).parent;v=d/'validation-06';raw=lambda p:p.read_bytes() if p.exists() else gzip.decompress(p.with_name(p.name+'.gz').read_bytes())
sha=lambda p:hashlib.sha256(raw(p)).hexdigest()
subprocess.run(['python',str(d/'verify_source.py')],check=True)
files=json.loads((v/'SOURCE_FILES.json').read_text());assert all(sha(r/p)==h for p,h in files.items())
clients={}
for n in ['core','allocator','session']:
 a=json.loads((v/(n+'.json')).read_text());assert a['exit']==0 and a['executable_sha256']==sha(Path(a['argv'][0]));assert a['log_sha256']==sha(v/(n+'.log'));clients[n]=a
# Existing file owner client remains unchanged since validation03. No rerun.
a=json.loads((d/'validation-03/content.json').read_text());assert a['exit']==0 and a['executable_sha256']==sha(Path(a['argv'][0]));clients['content_prior03_unchanged_dependencies']=a
old=json.loads((d/'validation-03/SOURCE_FILES.json').read_text())
for p in ['runtime/include/mka/content_volume.hpp','runtime/src/native/content_volume.cpp','runtime/include/mka/native_file.hpp','runtime/src/native/native_file.cpp','tests/test_native_content_volume_owner.cpp','cmake/native_game.cmake']:assert old[p]==files[p]
prior=json.loads((r/'research/runs/native-retail-main-frame-gates-20261122/validation-01/VERIFICATION.json').read_text())['remaining_undefined_symbols'];assert len(prior)==34
expected=set(prior);expected.remove('mka::retail::scene_bootstrap_required::continue_first_scene_at_220900(mka::retail::NativeRetailMainState&, mka::retail::StartupFailure&)')
expected.add('mka::retail::scene_bootstrap_required::continue_first_scene_at_225d40(mka::retail::NativeRetailMainState&, std::__1::shared_ptr<mka::retail::NativeRetailRendererCore>, mka::retail::StartupFailure&)')
for id in ['0401','0402','0403','0404','0405','0406','0407','0408','0409','0412','040a','040b']:
 expected.add('mka::retail::renderer_required::construct_core_plugin_'+id+'(mka::retail::NativeRetailRendererCore&, mka::retail::RetailRendererCoreFailure&)')
 expected.add('mka::retail::renderer_required::destroy_core_plugin_'+id+'(mka::retail::NativeRetailRendererCore&)')
expected.add('mka::retail::renderer_required::register_object_extensions_1b72b0(mka::retail::NativeRetailRendererCore&, mka::retail::RetailRendererCoreFailure&)')
expected.add('mka::retail::renderer_required::discard_object_extensions_after_core_init(mka::retail::NativeRetailRendererCore&)')
negative=raw(v/'full-entry-negative.log').decode();actual=[s.strip() for s in re.findall(r'undefined symbol: (.*)',negative)];assert len(actual)==60 and set(actual)==expected,(len(actual),set(actual)^expected)
cache=(r/'build-renderer-engine/CMakeCache.txt').read_text();nm=re.search(r'CMAKE_NM:FILEPATH=(.*)',cache)[1];compiler=re.search(r'CMAKE_CXX_COMPILER:STRING=(.*)',cache)[1]
args=[nm,'--demangle',str(r/'build-renderer-engine/test_native_retail_renderer_core.exe')];p=subprocess.run(args,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);assert p.returncode==0;(v/'core-symbols.log').write_bytes(p.stdout)
sy=p.stdout.decode(errors='replace');assert re.search(r' T SDL_InitSubSystem',sy) and '__imp_SDL_InitSubSystem' not in sy
for text in ['NativeRetailRendererCore::register_factory','NativeRetailRendererCore::construct_error_domain','NativeRetailRendererErrorDomain::take','continue_first_scene_at_1a6dd0','construct_core_plugin_040d']:assert text in sy
pins=json.loads((v/'ADDITIONAL_PINS.json').read_text());assert all(sha(Path(p))==h for p,h in pins.items())
result={'status':'PASS_BOUNDED_NATIVE_CORE_REGISTRY_AND_TWO_REAL_FACTORIES','base':'44d3d02bb205fad839634ffcfedcd9db5b943b15','source_files':len(files),'source_manifest_sha256':sha(v/'SOURCE_FILES.json'),'source_join_sha256':sha(d/'SOURCE_JOIN.json'),'xbox_join_sha256':sha(d/'XBOX_WORD_JOIN.json'),'compiler_sha256':sha(Path(compiler)),'symbol_tool_sha256':sha(Path(nm)),'cache_sha256':sha(r/'build-renderer-engine/CMakeCache.txt'),'clients':clients,'remaining_undefined_symbols':actual,'remaining_count':60,'limits':'Core registration/data+ErrorDomain+040D only. Twelve factory pairs/whole1B72B0 setup+ownedcleanup/entire225D40 suffix are required. Other constructors and tails are explicit test fixtures. No platformopen/start/GPU/game acceptance.'}
result['additional_pins_sha256']=sha(v/'ADDITIONAL_PINS.json');result['additional_pins_count']=len(pins);result['read_probe_input_sha256']=sha(d/'XBOX_WORD_JOIN.json');result['alignment_annotation_sha256']=sha(d/'XBOX_ALIGNMENT.json')
result['committed_source_manifest_sha256']=sha(v/'COMMITTED_SOURCE_FILES.json');result['source_newline_pairs_sha256']=sha(v/'SOURCE_NEWLINE_PAIRS.json');result['source_newline_pairs']=len(json.loads((v/'SOURCE_NEWLINE_PAIRS.json').read_text()))
(v/'VERIFICATION.json').write_bytes((json.dumps(result,indent=2)+'\n').encode());print('PASS856_SOURCE_PINS_3_CURRENT_CLIENTS_CONTENT03_UNCHANGED_DEPS_EXACT60_REQUIRED_NAMES')
