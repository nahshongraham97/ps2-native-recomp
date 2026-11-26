"""Connect three proved typed EE plans to the complete disposable AOT build.

Reuses its exact strict IOP/static EE/full65026VU link inventory, changing only
private dispatch and adapter objects. Packages are read-only hardlinks. No
normal build, original source, compiler toolchain or persistent card changes.
"""
from pathlib import Path
import argparse
import json
import os
import shutil
import subprocess
import sys

sys.path.insert(0,str(Path(__file__).resolve().parent))
import specialize_r192_stub_family as auth
from capture_r192_family_boundary import pin


def once(text,old,new):
    auth.require(text.count(old)==1,'resource_adapter_source_seam_changed:'+old[:70]);return text.replace(old,new)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['base-build','observations','event-proof','elf','compiler','output']:
        p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--expected-base-sha',required=True)
    p.add_argument('--expected-event-proof-sha',required=True)
    a=p.parse_args();out=a.output.resolve();base=a.base_build.resolve();normal=auth.W/'build/r192-compiled-konquest'
    auth.require(out.is_relative_to((auth.W/'build').resolve()) and not out.exists(),'fresh_owned_output_required')
    receipt=base/'BUILD.json';auth.require(pin(receipt)['sha256']==a.expected_base_sha,'complete_base_build_receipt_changed')
    record=json.loads(receipt.read_text());auth.require(record['status']=='COMPLETE_SOURCE_VU_AND_STATIC_IOP_CONNECTION_BUILT' and not record['input_changes'] and record['VU_unique_keys']==65026 and record['VU_DLL_count']==99,'complete_reference_base_required')
    auth.require(record['candidate_sha256']=='25d6ae51cdbb8e88b95118ba5a4bdfbf466b50f0ef7ed7e6d3bd3c417419e761' and pin(Path(record['candidate']))['sha256']==record['candidate_sha256'],'measured_complete_base_binary_changed')
    for path,digest in record['input_pins'].items():auth.require(pin(Path(path))['sha256']==digest,'complete_base_link_input_changed:'+path)
    proof=a.event_proof/'RESULT.json';auth.require(pin(proof)['sha256']==a.expected_event_proof_sha,'actual_event_proof_changed')
    pr=json.loads(proof.read_text());auth.require(pr['status']=='PASS_ACTUAL_SERVICE_CONDITIONAL_EVENT_SHADOWS' and pr.get('immutable_original_inputs_checked') is True and not pr['input_changes'],'current_subject_actual_event_shadow_required')
    for row in pr['source_pins']:auth.require(pin(Path(row['path']))==row,'actual_event_proof_source_changed')
    observed=a.observations/'RESULT.json';auth.require(pin(observed)['sha256']=='bc94ab802cc7e2108829d8f626d105868332fd54dc838a3829db10e9d90360da','actual_plan_capture_changed')
    obs=json.loads(observed.read_text());auth.require(obs['summary']['usable_completed']==3,'three_authentic_logical_shapes_required')
    for row in obs['observations']:auth.require(pin(Path(row['path']))==row,'actual_plan_capture_bytes_changed')
    auth.require(pin(a.elf)['sha256']=='a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f','original_ELF_changed')
    import analyze_r192_builder_domain as words
    original=words.Original(a.elf.read_bytes())
    core=auth.W/'tools/research/r192_aot_modules';header=core/'reference_resource_event_adapter.hpp'
    old_dispatch=auth.W/'build/oracle-telemetry/emitted-reference-preferred-probe-01/source/dispatch.cpp'
    base_pins={Path(path).resolve():digest for path,digest in record['input_pins'].items()}
    old_obj=old_dispatch.with_suffix('.obj');auth.require(old_obj.resolve() in base_pins,'measured_lifecycle_dispatch_absent')
    old_freeze=old_dispatch.parent.parent/'FREEZE_BUILD_LINK.json';freeze=json.loads(old_freeze.read_text())
    auth.require(any(row['source']==pin(old_dispatch) and row['object']==pin(old_obj) for row in freeze['private_objects']),'measured_dispatch_source_object_join_missing')
    # Only descriptor/shape helpers are used to manufacture fixed authentication
    # constants. This is not an emitted-code execution or decoding table.
    data='static const mka::emitted::resource_reference::Contract resource_contract=[](){using namespace mka::emitted::resource_reference;Contract c{};c.caller={'+','.join(str(original.word(0x2e612c+4*i))+'u' for i in range(21))+'};'
    case_pins=[]
    import struct
    for index,key in enumerate(['1','2','3']):
        folder=a.observations/'observations';meta=folder/('case-'+key+'.json');m=json.loads(meta.read_text());ram=folder/('case-'+key+'-segment-1-before-RAM.bin');raw=ram.read_bytes();n=[48,41,33][index]
        auth.require(struct.unpack_from('<II',raw,m['base']+4*(n-2))==(0x03e00008,0),'original_JR_delay_extent_changed')
        data+=f'{{auto&s=c.shapes[{index}];s.active_words={n};s.data_words={m["source_data_start_words"]};s.total_words={m["source_total_words"]};s.logical.attribute_count={len(m["attributes"])};s.logical.effect_count={len(m["effects_raw"])};s.logical.filter_count={len(m["filters_raw"])};'
        for i,row in enumerate(m['attributes']):data+=f's.logical.attributes[{i}]={{'+','.join(map(str,row))+'};'
        for i,e in enumerate(m['effects_raw']):
            identifier=e[0]&65535;src=e[4] if identifier in (2,3,25,17) else 0
            data+=f's.logical.effects[{i}].id={identifier};s.logical.effects[{i}].destination_tag={e[2]};s.logical.effects[{i}].source_tag={src};'
        for i,f in enumerate(m['filters_raw']):data+=f's.logical.filters[{i}]={{'+','.join(map(str,[f[0]&65535,f[1],0]))+'};'
        data+='s.code={'+','.join(str(w)+'u' for w in struct.unpack_from('<'+str(n)+'I',raw,m['base']))+'};}'
        case_pins += [meta,ram]
    data+='return c;}();\n'
    source=old_dispatch.read_text();source=once(source,'#include "'+(core/'reference_family_adapter.hpp').as_posix()+'"','#include "'+header.as_posix()+'"')
    source=once(source,'    if (mka::emitted::reference::preferred_family(c)) return;\n','')
    source=once(source,'            mu->function(c, entry, depth);','            if (mka::emitted::reference::preferred_family(c)) return;\n            mu->function(c, entry, depth);')
    source=once(source,'    u->function(c, id, depth);','    if (mka::emitted::reference::preferred_family(c)) return;\n    u->function(c, id, depth);')
    auth.require(shutil.disk_usage(out.parent).free>(12<<30)+pin(Path(record['candidate']))['size']+250*1024*1024,'12GiB_build_reserve_required')
    sources=[Path(__file__),header,core/'resource_pipeline_events.hpp',core/'reference_resource_layout.hpp',core/'semantic_resource_pipeline.hpp',core/'semantic_vector_effects.hpp',core/'resource_pipeline_continuation.hpp',old_dispatch,old_freeze,receipt,proof,observed,a.elf,a.compiler,*case_pins,*(normal/'src/runtime/include/ps2').glob('*.hpp'),normal/'src/runtime/src/kernel.hpp']
    link=list(record['link_arguments'])
    for item in link:
        if not item.startswith('-') and Path(item).suffix in ('.obj','.a'):sources.append(Path(item))
    pins=[pin(path) for path in dict.fromkeys(sources)]
    out.mkdir(parents=True);src=out/'source';src.mkdir();target=out/'out';target.mkdir()
    contract=src/'resource_contract.hpp';contract.write_text(data,encoding='utf8',newline='\n')
    dispatch=src/'dispatch.cpp';dispatch.write_text(source,encoding='utf8',newline='\n')
    adapter=src/'reference_adapter.cpp';adapter.write_text('#include "'+header.as_posix()+'"\n#include "resource_contract.hpp"\nnamespace mka::emitted::reference {\nstatic resource_reference::Adapter adapter(resource_contract);\nbool preferred_family(ps2::Cpu&c){return adapter.try_dispatch(c);}\nvoid reset_family_after_state_replacement(){adapter.reset();}\nvoid retire_family_thread(int id){adapter.retire(id);}\nbool family_pending(){return adapter.pending();}\n}\n',encoding='utf8',newline='\n')
    commands=[]
    def run(argv,name):
        cp=subprocess.run(argv,capture_output=True);log=out/(name+'.log');log.write_bytes(cp.stdout+cp.stderr);commands.append(dict(argv=argv,exit=cp.returncode,log=pin(log)));return cp
    for cpp in [dispatch,adapter]:
        argv=[str(a.compiler),'-std=c++20','-mavx2','-ffp-contract=off','-O2','-DNDEBUG','-DPS2_GPU','-I'+str(normal/'src/runtime/include'),'-I'+str(normal/'src/runtime/src'),'-I'+str(normal/'src/third_party/wgpu-native/include'),'-c',str(cpp),'-o',str(cpp.with_suffix('.obj'))]
        auth.require(run(argv,'compile-'+cpp.stem).returncode==0,'bounded_adapter_compile_failed:'+cpp.name)
    replaced=[]
    for i,value in enumerate(link):
        if value.startswith('-Wl,-Map,'):link[i]='-Wl,-Map,'+str(out/'resource-adapter-link.map')
        elif Path(value).resolve() in (old_obj.resolve(),old_obj.with_name('reference_adapter.obj').resolve()):link[i]=str(src/Path(value).name);replaced.append(Path(value).name)
    auth.require(sorted(replaced)==['dispatch.obj','reference_adapter.obj'] and link[-2]=='-o','exact_two_object_replacement_required')
    link[-1]=str(target/'ps2run.exe');response=out/'link.rsp';response.write_text('\n'.join('"'+x.replace('\\','/').replace('"','\\"')+'"' for x in link)+'\n',encoding='utf8',newline='\n')
    auth.require(run([str(a.compiler),'@'+str(response)],'link').returncode==0,'bounded_adapter_link_failed')
    package=[]
    for path in (base/'out').glob('*.dll'):os.link(path,target/path.name);package.append(path)
    for domain in ['ee','vu']:
        folder=target/'aot-modules'/domain;folder.mkdir(parents=True)
        for path in (base/'out/aot-modules'/domain).iterdir():
            auth.require(path.is_file(),'package_directory_shape_changed');before=pin(path);os.link(path,folder/path.name);auth.require(pin(folder/path.name)['sha256']==before['sha256'],'readonly_package_hardlink_changed');package.append(path)
    package_pins=[pin(path) for path in package];changes=[r['path'] for r in pins if pin(Path(r['path']))!=r];auth.require(not changes,'adapter_build_input_changed')
    result=dict(status='BOUNDED_THREE_PLAN_RESOURCE_ADAPTER_BUILT',candidate=str(target/'ps2run.exe'),candidate_sha256=pin(target/'ps2run.exe')['sha256'],base_receipt=pin(receipt),base_candidate=pin(Path(record['candidate'])),event_proof=pin(proof),native_subject_changes=pr['native_subject_changes'],source_pins=pins,input_changes=changes,commands=commands,link_arguments=link,package_pins=package_pins,private_sources=[pin(path) for path in [contract,dispatch,adapter]],private_objects=[pin(path.with_suffix('.obj')) for path in [dispatch,adapter]],
                scope='Three authenticated logical plans, CURRENT Cpu projection, actual original preempt service; build only. No game execution/native ownership/credit/universal acceptance.')
    dest=out/'BUILD.json';dest.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8',newline='\n');print(json.dumps(dict(status=result['status'],candidate_sha256=result['candidate_sha256'],receipt_sha256=pin(dest)['sha256'])))


if __name__=='__main__':main()
