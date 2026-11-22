from pathlib import Path
import subprocess,json,hashlib,re,gzip
r=Path.cwd();d=r/'research/runs/parallel-native-font-zero-20261122';v=d/'validation-03';sha=lambda b:hashlib.sha256(b).hexdigest();cm=json.loads((d/'validation-03/build.json').read_text())['argv'][0]
a=[cm,'--build',str(r/'build-font-zero'),'--target','test_native_startup_missing_bindings','-j2'];p=subprocess.run(a,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(v/'full-entry-negative.log').write_bytes(p.stdout);(v/'full-entry-negative.json').write_text(json.dumps({'argv':a,'exit':p.returncode,'log_sha256':sha(p.stdout)},indent=2)+'\n');assert p.returncode!=0
names=[s.strip()for s in re.findall(r'undefined symbol: (.*)',p.stdout.decode(errors='replace'))];prior=json.loads((r/'research/runs/native-retail-profile-input-reset-20261122/clean-validation-01/VERIFICATION.json').read_text())['remaining_undefined_symbols'];assert len(names)==59 and set(names)==set(prior),(len(names),set(names)^set(prior))
cache=(r/'build-font-zero/CMakeCache.txt').read_text();nm=re.search(r'CMAKE_NM:FILEPATH=(.*)',cache)[1];cc=re.search(r'CMAKE_CXX_COMPILER:STRING=(.*)',cache)[1];clients={}
for n in ['cpu','section-existing']:
 x=json.loads((v/(n+'.json')).read_text());assert x['exit']==0 and sha(Path(x['argv'][0]).read_bytes())==x['executable_sha256'];assert sha((v/(n+'.log')).read_bytes())==x['log_sha256'];clients[n]=x
for n in ['all-atlases','atlas-existing']:
 x=json.loads((d/('validation-02/'+n+'.json')).read_text());assert x['exit']==0 and sha(Path(x['argv'][0]).read_bytes())==x['executable_sha256'];assert sha((d/('validation-02/'+n+'.log')).read_bytes())==x['log_sha256'];clients[n]={'exact_unchanged02':x}
files=json.loads((v/'SOURCE_FILES.json').read_text());old=json.loads((d/'validation-02/SOURCE_FILES.json').read_text());assert [p for p in files if files[p]!=old[p]]==['runtime/src/native/art_section_data.cpp','runtime/src/native/retail_system_art_cpu.cpp']
pins={}
for n in ['cpu','all-atlases','atlas-existing','section-existing']:
 x=clients[n].get('exact_unchanged02',clients[n]);a=[nm,'--demangle',x['argv'][0]];p=subprocess.run(a,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);assert p.returncode==0;(v/(n+'-symbols.log')).write_bytes(p.stdout);(v/(n+'-symbols.json')).write_text(json.dumps({'argv':a,'exit':p.returncode,'log_sha256':sha(p.stdout)},indent=2)+'\n');s=p.stdout.decode(errors='replace');assert 'ps2::fp::' not in s
 if n=='cpu':assert re.search(r' T SDL_InitSubSystem',s) and '__imp_SDL_InitSubSystem' not in s and 'prepare_system_art_cpu_section' in s and 'NativeArtSectionData::parse' in s
 if n in ['all-atlases','atlas-existing']:assert 'prepare_system_art_cpu_section' not in s and 'NativeArtSectionData::' not in s
for p in [Path(cc),Path(nm),Path(cm),r/'build-font-zero/CMakeCache.txt',r/'build-font-zero/build.ninja']:
 pins[str(p)]=sha(p.read_bytes())
for p in (r/'third_party/sdl2').rglob('*'):
 if p.is_file():pins[str(p)]=sha(p.read_bytes())
assert all(sha((r/p).read_bytes())==h for p,h in files.items())
asset=json.loads((d/'ASSET_SOURCE.json').read_text());assert sha(Path(asset['path']).read_bytes())==asset['sha256'];extract=r/'research/runs/native-retail-loose-assets-20261121/validation-01/extraction.json';j=json.loads(extract.read_text());rec=next(x for x in j['members']if x['filename']=='gameart.ssf');assert rec['sha256']==rec['written_sha256']==asset['sha256'];pins[str(extract)]=sha(extract.read_bytes())
(v/'ADDITIONAL_PINS.json').write_text(json.dumps(pins,indent=2)+'\n');(v/'VERIFICATION.json').write_text(json.dumps({'status':'PASS_PARTIAL_SYSTEM_ART_CPU_DATA_ONLY','base':'ea4e3c99','source_files':len(files),'source_manifest_sha256':sha((v/'SOURCE_FILES.json').read_bytes()),'original_EE_functions':22,'original_EE_full_words':2980,'original_asset_sha256':asset['sha256'],'actual_decodes':{'locale_sections':5,'type2_3_textures':105,'named_MET':45,'group_range_words':15},'clients':clients,'remaining_required_names':names,'additional_pins_sha256':sha((v/'ADDITIONAL_PINS.json').read_bytes()),'limits':'No source-loaded/converted/cache publication, GPU handle/realization, SCR_MANAGED execution or16FC70 completion. ENTIRE3011D0/3DE190 and original16FC70 remain required. No game/endpoint/frames/audio.'},indent=2)+'\n');print('PASS',len(files),'source',len(names),'required','pins',len(pins))
