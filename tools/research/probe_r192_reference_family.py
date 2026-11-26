"""Build/probe a disposable preferred native EE family beside immutable N.

Four owned objects replace original dispatch/kernel/system and supply the
source-verified semantic adapter. Existing original AOT/runtime archives and
the sealed package are read only. No interpreter/compiler fallback is added.
"""
from pathlib import Path
import argparse
import json
import re
import shutil
import subprocess
import sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
import specialize_r192_stub_family as family
import capture_r192_family_boundary as capture


def once(text,before,after):
    family.require(text.count(before)==1,'owned_reference_seam_changed:'+before[:60])
    return text.replace(before,after)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ['adapter-proof','candidate-build','source-fork','state','card-copy','compiler','output']:parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--minimum-free-gib',type=int,default=20)
    a=parser.parse_args();out=a.output.resolve();n=a.candidate_build.resolve()
    family.require(out.is_relative_to((family.W/'build/oracle-telemetry').resolve()) and not out.exists(),'fresh_owned_output_required')
    family.require(a.minimum_free_gib>=20,'shared_disk_reserve_must_be_at_least20GiB')
    for path in [a.state,a.card_copy]:family.require(path.resolve().is_relative_to((family.W/'build/oracle-telemetry').resolve()),'owned_input_copy_required')
    family.require(family.pin(n/'out/ps2run.exe')['sha256']==capture.N_SHA,'N_reference_binary_changed')
    source_abi={'runtime/src/dispatch.cpp':'677a873628c45ddc7d5efae2b5ab187cc6fad38bd1b0e84eb080cbee87227ea2',
                'runtime/src/kernel.cpp':'a14def36e52cf9153fe398d2b832fbd642d56003320ef1f321acad454f745728',
                'runtime/src/system.cpp':'7a75bfed5758f57d4979f919ffa9526a7af337fd7a86871fb9bdcc1c6ea93384',
                'runtime/include/ps2/cpu.hpp':'2eb8c482859337add6480f0a1f0ad79ab908dda6d878d575f84f554ecf81c0fe'}
    lineage=n/'SOURCE_BUILD_DIGESTS.json';lineage_record=json.loads(lineage.read_text())
    for name,sha in source_abi.items():family.require(lineage_record['v8_source']['files'][name]==sha and family.pin(n/'src'/name)['sha256']==sha,'actual_R192_source_ABI_changed:'+name)
    for name in ['ps2run.exe','libps2game.a','libps2runtime.a','aot-modules/ee/manifest.txt']:family.require(family.pin(n/'out'/name)['sha256']==lineage_record['artifacts'][name],'actual_N_artifact_lineage_changed:'+name)
    proof=a.adapter_proof/'RESULT.json';family.require(family.pin(proof)['sha256']=='a0e6f68d2b48e43fd07a62d7be6011ca2a7bff9ebc4d6105d2250e9da79bc820','adapter_proof_changed')
    record=json.loads(proof.read_text());family.require(record['status']=='PASS_BOUNDED_REFERENCE_ADAPTER','positive_adapter_proof_required')
    for row in [*record['source_pins'],record['contract_pin']]:family.require(family.pin(Path(row['path']))['sha256']==row['sha256'],'tested_adapter_dependency_changed:'+row['path'])
    package_sources=[p for p in (n/'out/aot-modules/ee').iterdir() if p.is_file()]
    predicted=sum(p.stat().st_size for p in package_sources)+2*(n/'out/ps2run.exe').stat().st_size+(128<<20)
    reserve=a.minimum_free_gib*(1<<30)
    family.require(shutil.disk_usage(out.parent).free>=reserve+predicted,'shared_disk_reserve_unavailable')
    out.mkdir(parents=True);source_dir=out/'source';source_dir.mkdir();run=out/'run';run.mkdir()
    folder=family.W/'tools/research/r192_aot_modules';header=folder/'reference_family_adapter.hpp'
    include='#include "'+header.as_posix()+'"\n'
    originals={name:n/'src/runtime/src'/name for name in ['dispatch.cpp','kernel.cpp','system.cpp']}
    texts={name:path.read_bytes().decode().replace('\r\n','\n') for name,path in originals.items()}
    family.require('constexpr std::uint32_t kStateVersion = 8' in texts['system.cpp'],'actual_R192_v8_source_required')
    texts['dispatch.cpp']=once(texts['dispatch.cpp'],'    if (const auto h = ps2::hook::find_hook(pc)) { h(c); return; }',
                              '    if (const auto h = ps2::hook::find_hook(pc)) { h(c); return; }\n    if (mka::emitted::reference::preferred_family(c)) return;')
    texts['kernel.cpp']=once(texts['kernel.cpp'],'void reset_thread(Thread &t, std::uint32_t argument) {','void reset_thread(Thread &t, std::uint32_t argument) {\n    mka::emitted::reference::retire_family_thread(t.id);')
    texts['kernel.cpp']=once(texts['kernel.cpp'],'void make_dormant(Thread &t) {','void make_dormant(Thread &t) {\n    mka::emitted::reference::retire_family_thread(t.id);')
    family.require(texts['kernel.cpp'].count('        g_threads.erase(t->id);')==2,'thread_delete_seams_changed')
    texts['kernel.cpp']=texts['kernel.cpp'].replace('        g_threads.erase(t->id);','        mka::emitted::reference::retire_family_thread(t->id);\n        g_threads.erase(t->id);')
    texts['kernel.cpp']=once(texts['kernel.cpp'],'void reset() {\n    g_threads.clear();','void reset() {\n    mka::emitted::reference::reset_family_after_state_replacement();\n    g_threads.clear();')
    texts['system.cpp']=once(texts['system.cpp'],'void machine_state(state::Stream &s) {',
                            'void machine_state(state::Stream &s) {\n    if (s.loading()) mka::emitted::reference::reset_family_after_state_replacement();\n    else if (mka::emitted::reference::family_pending()) throw std::runtime_error("semantic reference: live typed continuation cannot be saved in original state format");')
    copied=[]
    for name,text in texts.items():
        path=source_dir/name;path.write_text(include+text,encoding='utf8',newline='\n');copied.append(path)
    contract=source_dir/'reference_contract.hpp';contract.write_bytes(Path(record['contract_pin']['path']).read_bytes())
    instance=source_dir/'reference_adapter.cpp';instance.write_text(include+'#include "reference_contract.hpp"\n'+INSTANCE,encoding='utf8',newline='\n');copied.append(instance)
    host=n/'out/CMakeFiles/ps2run.dir/host'
    host_objects=[host/name for name in ['main.cpp.obj','mka_unit_wrappers.cpp.obj','r192_aot_modules/loader.cpp.obj','profile_win.cpp.obj','window.cpp.obj','keymap.cpp.obj','stretch.cpp.obj']]
    libraries=[n/'out/libps2game.a',n/'out/libps2runtime.a',n/'src/third_party/sdl2/lib/libSDL2.dll.a',n/'src/third_party/wgpu-native/lib/libwgpu_native.a']
    driver=family.W/'tools/research/replay_r192_aot_sessions.py'
    inputs=[Path(__file__),Path(family.__file__),Path(capture.__file__),proof,lineage,driver,header,folder/'reference_family_projection.hpp',*folder.glob('semantic_*.hpp'),
            *originals.values(),n/'out/ps2run.exe',a.state,a.card_copy,a.compiler,*host_objects,*libraries,*package_sources,
            *sorted((n/'src/runtime/include/ps2').glob('*.hpp')),n/'src/runtime/src/kernel.hpp',n/'src/runtime/src/quick_keys.inc']
    pins=[family.pin(p) for p in dict.fromkeys(inputs)];commands=[]
    def command(name,args):
        cp=subprocess.run(args,capture_output=True);(out/(name+'.log')).write_bytes(cp.stdout+cp.stderr)
        commands.append(dict(name=name,argv=args,exit=cp.returncode));return cp
    objects=[]
    for path in copied:
        obj=path.with_suffix('.obj');objects.append(obj)
        args=[str(a.compiler),'-std=gnu++20','-mavx2','-ffp-contract=off','-O3','-DNDEBUG','-DPS2_GPU',
              '-I'+str(n/'src/runtime/include'),'-I'+str(n/'src/runtime/src'),'-I'+str(n/'src/third_party/wgpu-native/include'),'-c',str(path),'-o',str(obj)]
        family.require(command('compile-'+path.stem,args).returncode==0,'owned_object_compile_failed:'+path.name)
    exe=run/'ps2run.exe'
    args=[str(a.compiler),'-mavx2','-ffp-contract=off','-O3','-DNDEBUG','-mwindows','-static-libgcc','-static-libstdc++',
          *['-Wl,--wrap=_ZN3gen9unit_'+u+'ERN3ps23CpuEjj' for u in ['0018','0020','0025']],*[str(x) for x in objects],*[str(x) for x in host_objects],'-o',str(exe),*[str(x) for x in libraries],'-lbcrypt','-lwinmm',
          *['-l'+x for x in ['ws2_32','userenv','ntdll','synchronization','pthread','m','kernel32','user32','gdi32','winspool','shell32','ole32','oleaut32','uuid','comdlg32','advapi32']]]
    family.require(command('link-owned-reference-candidate',args).returncode==0,'owned_reference_link_failed')
    family.require(shutil.disk_usage(out).free>=reserve+sum(p.stat().st_size for p in package_sources),'shared_disk_reserve_before_package_unavailable')
    for name in ['SDL2.dll','dxcompiler.dll','dxil.dll']:shutil.copyfile(n/'out'/name,run/name)
    package=run/'aot-modules/ee';package.mkdir(parents=True)
    for path in package_sources:shutil.copyfile(path,package/path.name)
    program=next(Path(row['path']) for row in record['source_pins'] if Path(row['path']).name=='SLUS_214.10')
    verify=command('verify-sealed-package-no-game',[str(exe),str(program),'--verify-aot-package'])
    family.require(verify.returncode==0,'actual_sealed_package_verification_failed')
    family.require(shutil.disk_usage(out).free>=reserve,'shared_disk_reserve_before_probe_unavailable')
    args=[sys.executable,str(driver),'--candidate',str(exe),'--expected-binary-sha',family.pin(exe)['sha256'],'--source-fork',str(a.source_fork.resolve()),
          '--state',str(a.state.resolve()),'--card-copy',str(a.card_copy.resolve()),'--additional-frames','90','--output',str(out/'probe')]
    actual=command('actual-owned-state90-preferred-family',args)
    log=(out/'probe/stderr.log').read_text(errors='replace') if (out/'probe/stderr.log').exists() else ''
    summaries=re.findall(r'\[semantic-reference\] summary entries=(\d+) resumes=(\d+) completed=(\d+) suspended=(\d+) original_preempt_calls=(\d+) other_original_families=(\d+) pending=(\d+)',log)
    counts=dict(zip(['entries','resumes','completed','suspended','original_preempt_calls','other_original_families','pending'],map(int,summaries[-1]))) if len(summaries)==1 else None
    changes=[r['path'] for r in pins if family.pin(Path(r['path']))['sha256']!=r['sha256']]
    passed=actual.returncode==0 and counts and all(counts[k]>0 for k in ['entries','resumes','completed','original_preempt_calls']) and not changes and '[semantic-reference] REFUSED' not in log
    result=dict(status='PASS_BOUNDED_ACTUAL_PREFERRED_REFERENCE_FAMILY' if passed else 'REFUSED',commands=commands,counters=counts,source_pins=pins,input_changes=changes,
                generated_pins=[family.pin(x) for x in [*copied,contract,*objects]],candidate=family.pin(exe),package_manifest=family.pin(package/'manifest.txt'),
                full_probe_receipt=family.pin(out/'probe/RESULT.json') if (out/'probe/RESULT.json').exists() else None,
                scope=['Actual typed family replacement in a disposable v8 reference candidate, existing compiled original scheduler invoked unchanged.',
                       'Source-proved thread reset/dormant/delete and machine-state replacement hooks retire contexts; active original-state saving refuses because native context is not serialized.',
                       'Initial owned80225 state,90 new frames, no input recording: regression coverage, not universal/gameplay/image parity.',
                       'Other families retain existing compiled original AOT route; orphan/sharedVU/ABI/lifetime changes refuse, not an interpreter/JIT fallback.',
                       'Original N, card, source trees, launcher, ownership/credit and accepted native-scheduler status are unchanged.'])
    target=out/'RESULT.json';target.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8',newline='\n');print(json.dumps(dict(status=result['status'],result=str(target),sha256=family.pin(target)['sha256'])))
    return 0 if passed else 1


INSTANCE=r'''
namespace mka::emitted::reference {
static Adapter& instance(){static Adapter adapter(contract);return adapter;}
bool preferred_family(ps2::Cpu&c){return instance().try_dispatch(c);}
void reset_family_after_state_replacement(){instance().reset_after_machine_state_replacement();}
void retire_family_thread(int id){instance().retire_thread(id);}
bool family_pending(){return instance().pending()!=0;}
}
'''

if __name__=='__main__':raise SystemExit(main())
