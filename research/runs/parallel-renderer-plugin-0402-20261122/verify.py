from pathlib import Path
import json,struct,hashlib,re,gzip,subprocess
r=Path(__file__).resolve().parents[3];d=Path(__file__).parent;v=d/'validation-03';sha=lambda b:hashlib.sha256(b).hexdigest()
def raw(p):return p.read_bytes() if p.exists() else gzip.decompress(Path(str(p)+'.gz').read_bytes())
load=lambda p:json.loads(p.read_text())
j=load(d/'SOURCE_JOIN.json');b=(r/'../../../research/inputs/SLUS_214.10').resolve().read_bytes();assert sha(b)==j['elf_sha256'];po=struct.unpack_from('<I',b,28)[0];ps,pn=struct.unpack_from('<HH',b,42);ss=[struct.unpack_from('<8I',b,po+i*ps)for i in range(pn)]
def data(a,n):
 s=next(s for s in ss if s[0]==1 and s[2]<=a and a+n<=s[2]+s[4]);return b[s[1]+a-s[2]:s[1]+a-s[2]+n]
words=0
for name,f in j['functions'].items():
 rows=raw(d/name);assert sha(rows) in [f['sha256'],f['lf_sha256']];rows=[x.split('\t',2)for x in rows.decode().splitlines()];assert len(rows)==f['words'];start=int(f['begin'],16);assert len(rows)*4==int(f['end_exclusive'],16)-start
 for i,(a,h,ins) in enumerate(rows):assert int(a,16)==start+i*4 and data(int(a,16),4)==bytes.fromhex(h)
 words+=len(rows)
for f in j['data'].values():
 a=int(f['vma'],16)
 if f.get('loader_zero'):assert any(s[0]==1 and s[2]+s[4]<=a and a+f['size']<=s[2]+s[5] for s in ss)
 else:assert data(a,len(bytes.fromhex(f['bytes'])))==bytes.fromhex(f['bytes'])
assert words==779
x=load(d/'XBOX_JOIN.json');xb=(r/'../../../private-inputs/xbox-mka-20261121/default.xbe').resolve().read_bytes();assert sha(xb)==x['xbe_sha256']
for name,f in x['functions'].items():
 assert sha(raw(d/name))==f['sha256'] and not f['gap_bytes'];rows=[l.split('\t',2)for l in raw(d/name).decode().splitlines()];assert len(rows)==f['instructions']
 for (a,h,ins),(va,n,off)in zip(rows,f['mapped_instructions']):assert int(a,16)==va and xb[off:off+n]==bytes.fromhex(h)
for f in x['data'].values():assert xb[f['offset']:f['offset']+len(bytes.fromhex(f['bytes']))]==bytes.fromhex(f['bytes'])
files=load(v/'SOURCE_FILES.json');assert len(files)==860 and all(sha((r/p).read_bytes())==h for p,h in files.items());prior_files=load(d/'validation-01/SOURCE_FILES.json');assert [p for p in files if files[p]!=prior_files[p]]==['tests/test_retail_renderer_matrix.cpp']
clients={}
a=load(v/'matrix.json');assert a['exit']==0 and sha(Path(a['argv'][0]).read_bytes())==a['executable_sha256'] and sha(raw(v/'matrix.log'))==a['log_sha256'];clients['matrix']=a
for n in ['core','allocator','session']:
 a=load(d/'validation-01'/(n+'.json'));assert a['exit']==0 and sha(Path(a['argv'][0]).read_bytes())==a['executable_sha256'] and sha(raw(d/'validation-01'/(n+'.log')))==a['log_sha256'];clients[n]={'unchanged_prior01':a}
prior=load(r/'research/runs/parallel-renderer-engine-init-20261122/main-reentry-correction/validation-02/VERIFICATION.json')['required_names'];expected=set(prior)
for name in ['construct_core_plugin_0402(mka::retail::NativeRetailRendererCore&, mka::retail::RetailRendererCoreFailure&)','destroy_core_plugin_0402(mka::retail::NativeRetailRendererCore&)']:expected.remove('mka::retail::renderer_required::'+name)
for name in ['multiply_affines_19f680(mka::retail::RetailRendererAffine&, mka::retail::RetailRendererAffine const&, mka::retail::RetailRendererAffine const&, mka::retail::RetailRendererMatrixFailure&)','classify_matrix_1a0240(mka::retail::RetailRendererMatrixValue&, mka::retail::RetailRendererMatrixTolerances const&, mka::retail::RetailRendererMatrixFailure&)','invert_nonidentity_matrix_1a07a0(mka::retail::RetailRendererMatrixValue&, mka::retail::RetailRendererMatrixValue const&, mka::retail::RetailRendererMatrixFailure&)']:expected.add('mka::retail::renderer_required::'+name)
actual=[x.strip() for x in re.findall(r'undefined symbol: (.*)',raw(d/'validation-02/full-entry-negative.log').decode())];assert len(actual)==61 and set(actual)==expected
cache=(r/'build-renderer-0402/CMakeCache.txt').read_text();nm=Path(re.search(r'CMAKE_NM:FILEPATH=(.*)',cache)[1]);compiler=Path(re.search(r'CMAKE_CXX_COMPILER:STRING=(.*)',cache)[1]);args=[str(nm),'--demangle',str(r/'build-renderer-0402/test_native_retail_renderer_matrix.exe')];p=subprocess.run(args,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);assert p.returncode==0;(v/'symbols.log').write_bytes(p.stdout);symbols=p.stdout.decode(errors='replace');assert re.search(r' T SDL_InitSubSystem',symbols) and '__imp_SDL_InitSubSystem' not in symbols
for name in ['NativeRetailRendererMatrixDomain::set_tolerances','NativeRetailRendererMatrixDomain::inverse','construct_core_plugin_0402','destroy_core_plugin_0402']:assert name in symbols
header=load(d/'SHARED_HEADER.json');assert sha((r/'runtime/include/mka/retail_renderer_math.hpp').read_bytes())==header['sha256']
pins={}
for base in [r/'third_party/sdl2',r/'build-renderer-0402']:
 for p in base.rglob('*'):
  if p.is_file() and (base.name=='sdl2' or p.name in ['CMakeCache.txt','build.ninja','SDL2.dll'] or p.name.startswith('native_retail_') and p.suffix in ['.cpp','.hpp']):pins[str(p)]=sha(p.read_bytes())
for p in [nm,compiler,Path(load(d/'validation-01/configure.json')['argv'][0])]:pins[str(p)]=sha(p.read_bytes())
(v/'ADDITIONAL_PINS.json').write_bytes((json.dumps(pins,indent=2)+'\n').encode())
result={'status':'PASS_TYPED0402_OWNER_SETTER_DISPATCH_IDENTITYCOPY','base':'feb05096e43fb0ae37bac6407d2567569ddb03e0','source_files':len(files),'source_manifest_sha256':sha((v/'SOURCE_FILES.json').read_bytes()),'original_words':words,'complete_ee_listings':len(j['functions']),'xbox_listings':len(x['functions']),'source_join_sha256':sha((d/'SOURCE_JOIN.json').read_bytes()),'xbox_join_sha256':sha((d/'XBOX_JOIN.json').read_bytes()),'clients':clients,'remaining_required_names':actual,'compiler_sha256':sha(compiler.read_bytes()),'symbol_tool_sha256':sha(nm.read_bytes()),'additional_pins':len(pins),'additional_pins_sha256':sha((v/'ADDITIONAL_PINS.json').read_bytes()),'shared_header_sha256':header['sha256'],'limits':'Three exact native matrix math kernels remain required. Otherfactories/whole1B72B0/whole225D40 remain required;no EngineOpen/Start/renderer/game acceptance.'}
(v/'VERIFICATION.json').write_bytes((json.dumps(result,indent=2)+'\n').encode());print('PASS779_EEWORDS_3XBE_LISTINGS_860SOURCE_4CLIENTIDENTITIES_EXACT61_REQUIRED_NAMES')
