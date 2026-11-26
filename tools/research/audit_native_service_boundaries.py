"""Source-pinned native service boundaries with owned file/pure-audio controls.

Never launches an oracle/game or opens COM/audio endpoints. IOP/SIF reference
compatibility is inventoried separately and is not accepted native architecture.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys

ROOT=Path(__file__).resolve().parents[2]
ROWS=[
 dict(service='IOP',native_status='REPLACED_BY_TYPED_SERVICE_BOUNDARIES_NOT_A_NATIVE_IOP_CPU',
      native_files=['runtime/src/native/retail_pad_startup.cpp','runtime/src/native/retail_main.cpp'],
      legacy_files=['runtime/src/native/iop_native.h','runtime/src/iop/cpu.cpp'],
      anchors=['SIF/module/port negotiation','validate_input_owner'],
      existing_tests=['tests/test_retail_pad_startup.cpp'],
      open=['Individual game call/import consumption and full native startup remain required.','Compiled IopCpuState/IopMemory modules retain console ABI and memory; they are reference compatibility, never native-port credit.']),
 dict(service='SIF',native_status='DIRECT_TYPED_NATIVE_OWNERS_AT_SPECIFIC_BOUNDARIES',
      native_files=['runtime/src/native/retail_pad_startup.cpp','runtime/src/native/retail_save_startup.cpp','runtime/src/native/retail_sound_bank_main.cpp'],
      legacy_files=['runtime/src/iop/sif.cpp'],anchors=['validate_input_owner','NativeSaveStore::open_or_create','create_playback_context_316320'],
      existing_tests=['tests/test_retail_save_startup.cpp'],
      open=['No general SIF/RPC emulation is an accepted native service.','All required producers, errors and consumers still need per-boundary source and behavioral evidence; no synthetic RPC success.']),
 dict(service='cards_and_saves',native_status='NATIVE_HOST_SAVE_STORE_AND_MAIN_INITIALIZER',
      native_files=['runtime/src/native/native_save_store.cpp','runtime/src/native/retail_save_startup.cpp','runtime/include/mka/native_save_store.hpp'],
      legacy_files=['runtime/src/iop/mcserv.cpp'],anchors=['FlushFileBuffers','FILE_RENAME_INFO','NativeSaveStore::open_or_create'],
      existing_tests=['tests/test_native_save_store.cpp','tests/test_retail_save_startup.cpp'],
      open=['Native save semantics and user-owned profiles remain mandatory; legacy PS2 card transport is not a native requirement.','This owned store test does not prove an actual gameplay save/reload or migrate user card bytes.']),
 dict(service='audio',native_status='NATIVE_WINDOWS_OUTPUT_AND_TYPED_PLAYBACK_OWNERS_WITH_OPEN_RETAIL_CHILDREN',
      native_files=['runtime/src/native/audio_device.cpp','runtime/include/mka/audio_device.hpp','runtime/src/native/retail_sound_runtime.cpp','runtime/src/native/retail_sound_bank_main.cpp'],
      legacy_files=['runtime/src/iop/libsd_native.cpp','runtime/src/iop/audio_service.cpp'],
      anchors=['IAudioClient::Initialize','create_playback_context_316320','RetailCurrentSoundPhase::failed'],
      existing_tests=['tests/test_native_audio_device.cpp','tests/test_retail_sound_bank_jobs.cpp'],
      open=['Default test validates configuration/PCM only; no endpoint, COM or audible output.','Retail parameter/context creation, effects, worker/DSP, timing and full connected audio equivalence are separate gates.'])]

def need(v,s):
    if not v:raise ValueError(s)
def pin(p):
    q=Path(p).resolve();return dict(path=str(q),sha256=hashlib.sha256(q.read_bytes()).hexdigest())
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compiler',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=a.output.resolve()
    need(out.is_relative_to(ROOT/'build/oracle-telemetry')and not out.exists(),'fresh_owned_output_required');need(shutil.disk_usage(out.parent).free>12<<30,'12GiB_reserve_required')
    inputs=[Path(__file__),a.compiler];rows=[]
    for row in ROWS:
        paths=[ROOT/n for key in ('native_files','legacy_files','existing_tests')for n in row[key]]
        # An absent test is reported, never invented or credited.
        files=[q for q in paths if q.is_file()];native='\n'.join((ROOT/n).read_text()for n in row['native_files'])
        for anchor in row['anchors']:need(anchor in native,'native_service_anchor_changed:'+row['service']+':'+anchor)
        inputs+=files;rows.append(dict(**row,source_pins=[pin(q)for q in files],missing_existing_tests=[n for n in row['existing_tests']if not(ROOT/n).exists()],classification_scope='Bounded source inventory; not whole-build reachability/acceptance or new native implementation credit.'))
    need('argc==1)return 0' in (ROOT/'tests/test_native_audio_device.cpp').read_text(),'pure_audio_default_seam_changed')
    checkspec=[('save-store','tests/test_native_save_store.cpp','runtime/src/native/native_save_store.cpp',[]),('audio-pure','tests/test_native_audio_device.cpp','runtime/src/native/audio_device.cpp',['-lole32','-luuid'])]
    inputs += [ROOT/n for _,test,source,_ in checkspec for n in (test,source)]+list((ROOT/'runtime/include/mka').glob('*.hpp'))
    before=[pin(q)for q in dict.fromkeys(inputs)];out.mkdir(parents=True);(out/'temp').mkdir();env=dict(os.environ,TEMP=str(out/'temp'),TMP=str(out/'temp'),PATH=str(a.compiler.resolve().parent)+os.pathsep+os.environ['PATH']);checks=[]
    for name,test,source,libs in checkspec:
        binary=out/(name+'.exe');cmd=[str(a.compiler),'-std=c++20','-O1','-static-libgcc','-static-libstdc++','-I'+str(ROOT/'runtime/include'),str(ROOT/test),str(ROOT/source),*libs,'-o',str(binary)]
        c=subprocess.run(cmd,capture_output=True,env=env,timeout=120);log=out/(name+'-compile.log');log.write_bytes(c.stdout+c.stderr);record=dict(name=name,compile_argv=cmd,compile_exit=c.returncode,compile_log=pin(log))
        if c.returncode==0:
            run=subprocess.run([str(binary)],capture_output=True,env=env,timeout=120);q=out/(name+'.log');q.write_bytes(run.stdout+run.stderr);record.update(run_argv=[str(binary)],run_exit=run.returncode,log=pin(q),stdout=run.stdout.decode(errors='replace'),stderr=run.stderr.decode(errors='replace'),binary=pin(binary))
        checks.append(record)
    changed=[q['path']for q in before if pin(q['path'])!=q];passed=not changed and all(x['compile_exit']==0 and x.get('run_exit')==0 for x in checks)
    result=dict(status='PASS_BOUNDED_NATIVE_SERVICE_INVENTORY_AND_OWNED_CONTROLS'if passed else'SERVICE_INVENTORY_CONTROLS_FAILED',source_pins=before,input_changes=changed,services=rows,checks=checks,invocation=[sys.executable,*sys.argv],native_credit_delta=0,oracle_executed=False,endpoint_or_COM_opened=False,gameplay_save_or_audio_parity_claim=False,
        limits=['Static source paths and two component tests only; no claim of complete native startup or full call-graph coverage.','Fresh files exist only under this owned output TEMP; original cards/states/settings/builds are untouched.','IOP/SIF reference code retains machine/transport state even when AOT; it is not reclassified as accepted native service.'])
    target=out/'RESULT.json';target.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8',newline='\n');print(json.dumps(dict(status=result['status'],receipt=pin(target),checks=[(r['name'],r.get('run_exit'))for r in checks])));return 0 if passed else 2
if __name__=='__main__':raise SystemExit(main())
