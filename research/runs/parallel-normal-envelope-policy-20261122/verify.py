from pathlib import Path
import subprocess,sys,os,json,re,hashlib
r=Path(__file__).resolve().parent;f=r.parents[2];d=f.parents[2].parent/'dependencies';b=f/'build-section-timing';o=r/sys.argv[1];o.mkdir(exist_ok=False)
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
compiler=d/'compiler/llvm-mingw-20261006-ucrt-x86_64/bin';cmake=d/'cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe';env=dict(os.environ,PATH=str(compiler)+os.pathsep+os.environ['PATH'])
def run(name,argv,expected=0):
 argv=list(map(str,argv));p=subprocess.run(argv,cwd=f,env=env,capture_output=True,creationflags=subprocess.CREATE_NO_WINDOW);log=o/(name+'.log');log.write_bytes(p.stdout+p.stderr);(o/(name+'.json')).write_text(json.dumps(dict(argv=argv,exit=p.returncode,executable_sha256=sha(argv[0]),log_sha256=sha(log)),indent=2)+'\n',newline='\n')
 if expected is not None:assert p.returncode==expected,(name,p.returncode)
 return p
run('source-join',[sys.executable,r/'join.py'])
run('build',[cmake,'--build',b,'--target','test_native_retail_normal_envelope_intent','-j2'])
run('intent',[b/'test_native_retail_normal_envelope_intent.exe'])
p=run('missing-timing',[cmake,'--build',b,'--target','test_native_retail_normal_envelope_missing_timing','-j2'],None);assert p.returncode!=0
missing=re.findall(r'undefined symbol: ([^\r\n]+)',(p.stdout+p.stderr).decode(errors='replace'))
assert len(missing)==1 and missing[0]=='mka::sound::normal_envelope_required::realize_retail_normal_envelope_profile(mka::sound::RetailNormalEnvelopeIntent const&, unsigned int, mka::sound::RetailNormalEnvelopeFailure&)'
p=run('symbols',[compiler/'llvm-nm.exe','-C',b/'test_native_retail_normal_envelope_intent.exe']);assert not re.search(rb'(?<![A-Za-z0-9_])(?:ps2|gs|spu2|sif|Cpu|recomp)::|\b(?:g_ram|g_spr)\b|SDL_|HostVoice::start',p.stdout)
names=['cmake/native_game.cmake','runtime/include/mka/retail_normal_envelope_intent.hpp','runtime/include/mka/retail_normal_envelope_required.hpp','runtime/src/native/retail_normal_envelope_intent.cpp','tests/test_retail_normal_envelope_intent.cpp','tests/test_retail_normal_envelope_missing_timing.cpp']
manifest={n:sha(f/n) for n in names};assert all(b'\r' not in (f/n).read_bytes() for n in names);(o/'SOURCE_FILES.json').write_text(json.dumps(manifest,indent=2)+'\n',newline='\n')
result=dict(status='PASS_NORMAL_SETUP_INTENT_ONLY',base_commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=f,text=True).strip(),source_manifest_sha256=sha(o/'SOURCE_FILES.json'),original_join_sha256=sha(r/'SOURCE_JOIN.json'),compiler_sha256=sha(compiler/'clang++.exe'),cache_sha256=sha(b/'CMakeCache.txt'),required_missing_symbols=missing,scope='Source-decoded setup/start intent only. Timing/profile realization required,not implemented. No playback/rate/PCM/endpoint/gameplay equivalence.')
(o/'VERIFICATION.json').write_text(json.dumps(result,indent=2)+'\n',newline='\n');print(result['status'])
