"""Verify metadata/operands only against original data and real native catalog owners."""
from pathlib import Path
import sys,subprocess,os,json,hashlib,re,struct,time
r=Path(__file__).resolve().parent;f=r.parents[2];d=f.parents[2].parent/'dependencies'
b=f/'build-section-timing';o=r/sys.argv[1];o.mkdir(exist_ok=False)
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
compiler=d/'compiler/llvm-mingw-20261006-ucrt-x86_64/bin';cmake=d/'cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe'
env=dict(os.environ,PATH=str(compiler)+os.pathsep+os.environ['PATH'])
def run(name,argv,expected=0):
    argv=list(map(str,argv));t=time.time();p=subprocess.run(argv,cwd=f,env=env,capture_output=True,creationflags=subprocess.CREATE_NO_WINDOW)
    log=o/(name+'.log');log.write_bytes(p.stdout+p.stderr)
    (o/(name+'.json')).write_text(json.dumps(dict(argv=argv,exit=p.returncode,seconds=time.time()-t,executable_sha256=sha(argv[0]),log_sha256=sha(log)),indent=2)+'\n',newline='\n')
    if expected is not None:assert p.returncode==expected,(name,p.returncode)
    return p
proof=f/'research/runs/parallel-sound-parameters-source-20261122/SOURCE_JOIN.json'
assert sha(proof)=='b9ae4cbd43a28e407cd00c461212a8c3a5b3061431b47b6779c86c7147b58d5e'
v=json.loads(proof.read_text());words=struct.unpack('<9656I',bytes.fromhex(v['ps2']['data']['sound_parameters']['bytes']));cats=struct.unpack('<78I',bytes.fromhex(v['ps2']['data']['categories']['bytes']))
inc=f/'runtime/src/native/retail_sound_metadata_data.inc';text=inc.read_text();literals=[int(x,16) for x in re.findall(r'0x([0-9a-f]{8})u',text)]
assert literals[:9656]==list(words) and literals[9656:]==list(cats[::2])
gains=[int(x) for x in re.findall(r'\{0x[0-9a-f]{8}u,([0-9]+)\}',text)];assert gains==[x&255 for x in cats[1::2]]
identity=json.loads((r/'DATA_IDENTITY.json').read_text());assert sha(inc)==identity['generated_data_sha256'] and sha(r/'expected-metadata.tsv')==identity['expected_sha256']
rows=(r/'expected-metadata.tsv').read_text().splitlines()[1:];assert len(rows)==9656
for i,row in enumerate(rows):
    x=words[i];c=x>>25;out=(x>>20)&7;out=out if out<4 else out-8
    assert list(map(int,row.split()))==[i,x&1023,x>>13&127,c,cats[c*2+1]&255,x>>23&3,out,cats[c*2]]
content=f.parents[2]/'private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon'
assets={str(p):sha(p) for p in [content/'sndsps2/MSLASSET.MS2',content/'sndsps2/MSLASSET.MS4']}
run('build',[cmake,'--build',b,'--target','test_native_retail_sound_metadata','test_native_retail_profile_preferences','-j2'])
run('metadata',[b/'test_native_retail_sound_metadata.exe',content,r/'expected-metadata.tsv'])
run('existing-preferences',[b/'test_native_retail_profile_preferences.exe',content])
assert all(sha(p)==h for p,h in assets.items())
symbols=run('metadata-symbols',[compiler/'llvm-nm.exe','-C',b/'test_native_retail_sound_metadata.exe']).stdout
assert not re.search(rb'(?<![A-Za-z0-9_])(?:ps2|gs|spu2|sif|Cpu|recomp)::|\b(?:g_ram|g_spr)\b|SDL_OpenAudio',symbols)
for name in [b'resolve_retail_semantic_sound_operands(',b'NativeRetailSoundBankCatalog::observe(',b'NativeRetailGameSession::game_records_values(',b'AudioBankSession::load_state(']:assert name in symbols,name
p=run('full-entry-negative',[cmake,'--build',b,'--target','test_native_startup_missing_bindings','-j2'],None);assert p.returncode!=0
missing=re.findall(r'undefined symbol: ([^\r\n]+)',(p.stdout+p.stderr).decode(errors='replace'))
prior=json.loads((r.parent/'parallel-section-timing-20261122/validation-05/VERIFICATION.json').read_text())['remaining_undefined_symbols']
assert sorted(missing)==sorted(prior) and len(missing)==len(set(missing))==37
names=subprocess.check_output(['git','ls-files','runtime','recomp','cmake','tests','CMakeLists.txt'],cwd=f,text=True).splitlines()+['runtime/include/mka/retail_sound_metadata.hpp','runtime/src/native/retail_sound_metadata.cpp','runtime/src/native/retail_sound_metadata_data.inc','tests/test_retail_sound_metadata.cpp']
manifest={n:sha(f/n) for n in sorted(set(names))};(o/'SOURCE_FILES.json').write_text(json.dumps(manifest,indent=2)+'\n',newline='\n')
result=dict(status='PASS_NATIVE_SOUND_METADATA_OPERANDS',base_commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=f,text=True).strip(),source_manifest_sha256=sha(o/'SOURCE_FILES.json'),compiler_sha256=sha(compiler/'clang++.exe'),cache_sha256=sha(b/'CMakeCache.txt'),proof_sha256=sha(proof),assets_before_and_after=assets,remaining_undefined_symbols=missing,scope='9656 source-derived metadata rows;4 real catalog callback handles and existing Session preferences; no playback/arithmetic/envelope/endpoint/gameplay acceptance. Stale external-unload safety branch compiled but not injected.')
(o/'VERIFICATION.json').write_text(json.dumps(result,indent=2)+'\n',newline='\n');print(result['status'])
