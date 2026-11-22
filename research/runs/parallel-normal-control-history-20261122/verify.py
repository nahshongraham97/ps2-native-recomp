# Frozen native component verification. No game or physical endpoints.
from pathlib import Path
import json,hashlib,subprocess,time,re,os
w=Path(__file__).resolve().parents[3];d=Path(__file__).parent;v=d/'validation-03';v.mkdir(exist_ok=False)
h=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
files={}
for base in ['runtime','tests','cmake','recomp']:
 for p in (w/base).rglob('*'):
  if p.is_file():files[p.relative_to(w).as_posix()]=h(p)
files['CMakeLists.txt']=h(w/'CMakeLists.txt')
(v/'SOURCE_FILES.json').write_text(json.dumps(files,indent=2)+'\n')
expected_path=w.parent/'ps2-native-recomp/research/runs/native-retail-renderer-input-dependencies-20261122/validation-02/VERIFICATION.json'
expected=json.loads(expected_path.read_text());missing=expected.get('remaining_undefined_symbols',expected.get('remaining_integration_symbols'))
assert len(missing)==34
(v/'EXPECTED_BASE34.json').write_text(json.dumps({'source':str(expected_path),'sha256':h(expected_path),'base':'44d3d02bb205fad839634ffcfedcd9db5b943b15','names':missing},indent=2)+'\n')
compiler=Path(r'C:/Users/nahsh/mk port/Armageddon-backup/dependencies/compiler/llvm-mingw-20261006-ucrt-x86_64/bin')
cmake=Path(r'C:/Users/nahsh/mk port/Armageddon-backup/dependencies/cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe')
assets=w.parents[2]/'private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon';ms2=assets/'sndsps2/mslasset.ms2'
(v/'ASSETS.json').write_text(json.dumps({str(p):h(p) for p in [ms2,assets/'sndsps2/mslasset.ms4']},indent=2)+'\n')
def run(name,a):
 a=list(map(str,a));t=time.monotonic();r=subprocess.run(a,cwd=w,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW,timeout=300)
 (v/(name+'.log')).write_bytes(r.stdout);q={'argv':a,'exit':r.returncode,'seconds':time.monotonic()-t,'executable_sha256':h(Path(a[0])),'source_manifest_sha256':h(v/'SOURCE_FILES.json'),'log_sha256':hashlib.sha256(r.stdout).hexdigest(),'game_run':False,'physical_endpoints_opened':False}
 (v/(name+'.json')).write_text(json.dumps(q,indent=2)+'\n');print(name,r.returncode,flush=True)
 if r.returncode:print(r.stdout[-2400:].decode(errors='replace'),flush=True)
 return q,r.stdout
sound=['test_native_held_playback','test_native_sound_curves','test_native_host_voice','test_native_program_copy','test_native_program_copy_failure','test_native_bank_playback','test_native_playback_owner','test_native_playback_reservation','test_native_playback_reservation_gaps','test_native_default_playback_runtime','test_native_program_output_activation','test_native_forced_output_retirement','test_native_forced_output_retirement_failure']
main=['test_native_retail_reset_sound','test_native_retail_initial_reset','test_native_retail_initial_task','test_native_retail_startup','test_native_retail_file_startup','test_native_retail_middleware_main']
extra=['test_native_runtime_program_work','test_native_runtime_preparation','test_native_sound_output_plan','test_native_retail_sound_context','test_native_program_host_clock','test_native_audio_device']
build=w/'build-sound-history'
r,_=run('build',[cmake,'--build',build,'--target',*extra,*sound,*main,'test_native_control_history','-j2']);assert r['exit']==0
r,symbols=run('context-symbols',[compiler/'llvm-nm.exe',build/'test_native_retail_sound_context.exe']);assert r['exit']==0
for s in ['__wrap__ZN3mka5sound29open_native_game_sound_outputERNS0_18AudioDeviceFailureE','__wrap__ZN3mka5sound17NativeAudioDevice4openERKNS0_17AudioDeviceConfigERNS0_18AudioDeviceFailureE','__wrap__Znwy','__wrap__ZN3mka5sound17NativeProgramCopyD1Ev']:assert s.encode() in symbols
empty=v/'empty-content';empty.mkdir()
plans=[(extra[0],[ms2]),(extra[1],[ms2]),(extra[2],[]),(extra[3],[assets]),(extra[4],[]),(extra[5],[])]
plans += [(n,[ms2]) for n in sound]
plans += [(main[0],[assets,v/'negative-content']),(main[1],[empty]),(main[2],[empty]),(main[3],[v/'startup-fixtures']),(main[4],[v]),(main[5],[empty])]
prior=d/'validation-02';reuse={'test_native_sound_curves':'sound_curves','test_native_host_voice':'host_voice','test_native_program_copy':'program_copy','test_native_program_copy_failure':'program_copy_failure','test_native_control_history':'control_history'}
clients=[]
for name,a in plans+[('test_native_control_history',[ms2,d/'reference-03/history-corpus.txt'])]:
 if name in reuse:
  old=prior/(reuse[name]+'.json');q=json.loads(old.read_text());assert q['exit']==0 and h(build/(name+'.exe'))==q['executable_sha256'];clients.append({'name':name,'reuse':str(old),'receipt_sha256':h(old),'binary_sha256':q['executable_sha256']});print(name,'REUSED_EXACT_BINARY',flush=True)
 else:
  q,_=run(name,[build/(name+'.exe'),*a]);assert q['exit']==0;clients.append({'name':name,'receipt':name+'.json','binary_sha256':q['executable_sha256']})
q,log=run('full-entry-negative',[cmake,'--build',build,'--target','test_native_startup_missing_bindings','-j2']);actual=[x.strip() for x in re.findall(r'undefined symbol: (.*)',log.decode())];assert q['exit']!=0 and sorted(actual)==sorted(missing) and len(actual)==34
assert all(h(w/p)==s for p,s in files.items())
assert all(h(Path(p))==s for p,s in json.loads((v/'ASSETS.json').read_text()).items())
(v/'VERIFICATION.json').write_text(json.dumps({'status':'PASS_RETAINED_NORMAL_CONTROL_HISTORY_COMPONENTS','base':'44d3d02bb205fad839634ffcfedcd9db5b943b15','source_files':len(files),'source_manifest_sha256':h(v/'SOURCE_FILES.json'),'compiler_sha256':h(compiler/'clang++.exe'),'cache_sha256':h(build/'CMakeCache.txt'),'clients':clients,'remaining_undefined_symbols':actual,'game_run':False,'physical_endpoints_opened':False,'limits':['9534/959C history and actual clone/HostVoice dynamic use only','Whole pause/latch/timing/queued coordination remains unimplemented','No generic dynamic-control cadence or exact normal envelope realization claimed']},indent=2)+'\n')
print('PASS26_CLIENT_IDENTITIES_21_FRESH_5_EXACT_PRIOR_UNCHANGED_BASE34',flush=True)
