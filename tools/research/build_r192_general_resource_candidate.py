"""Build disposable descriptor-derived dispatch and real source lifecycle hooks.

Original normal/R192 binaries and cards are never outputs. Every source is
snapshotted before compilation; the generated-unit overrides match the exact
existing game archive. No live compiler or instruction interpreter is added.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess

from verify_r192_resource_word_equality import digest

ROOT=Path(__file__).resolve().parents[2]


def need(ok,reason):
    if not ok:raise ValueError(reason)


def pin(q):return dict(path=str(Path(q).resolve()),sha256=digest(q))


ADAPTER=r'''
#include "reference_general_resource_adapter.hpp"
#include "general_contract.hpp"
#include "ps2/os.hpp"
#include <filesystem>
namespace {
using Adapter=mka::emitted::general_resource_reference::Adapter;
using Registry=mka::emitted::resource_owner_epochs::Registry;
Adapter*live_adapter=nullptr;
void retire(void*,unsigned cache){if(live_adapter)live_adapter->retire_cache(cache);}
Registry registry({nullptr,retire});
Adapter adapter(resource_contract,&registry);
struct Activation{Activation(){live_adapter=&adapter;}}activation;
std::map<unsigned,std::uint64_t>events;
std::map<std::string,std::uint64_t>refusals;
struct Report{~Report(){const auto&c=registry.counters();
 std::fprintf(stderr,"[resource-owner-epochs] observed=%llu refused=%llu quarantined=%llu allocations=%llu commits=%llu identity_setters=%llu native_credit=0\n",c.observed,c.refused,c.quarantined,c.allocations,c.commits,c.identity_setters);
 for(const auto&[site,n]:events)std::fprintf(stderr,"[resource-owner-epochs] source_site=%08X calls=%llu\n",site,n);
 for(const auto&[reason,n]:refusals)std::fprintf(stderr,"[resource-owner-epochs] declined=%llu reason=%s\n",n,reason.c_str());
}}report;
}
extern "C" void r192_resource_lifecycle_hook(ps2::Cpu&c,std::uint32_t site){
 ++events[site];
 // Revoke before original cleanup's first instruction/callback/free, even
 // when this owner came from a legacy state without allocation provenance.
 if(site==0x210780)adapter.retire_cache(c.r[4].w[0]);
 if(!registry.observe_hook(c,site,ps2::kernel::current_thread()))++refusals[registry.last_refusal()];
}
extern "C" Registry&r192_family_registry(){return registry;}
extern "C" void r192_family_restore_begin(){registry.begin_restore();adapter.reset();}
extern "C" void r192_family_restore_end(bool success){registry.end_restore(success);}
extern "C" const char*r192_family_checkpoint_source_id(){return "@SOURCE_ID@";}
extern "C" const char*r192_family_checkpoint_owned_root(){
 // Match quick::base_dir: runtime configured root, never a build-time test path.
 static std::string value;namespace fs=std::filesystem;fs::path root;
 if(!ps2::g_options.quick_dir.empty())root=ps2::os::path(ps2::g_options.quick_dir);
 else if(!ps2::g_options.memcard_path.empty())root=ps2::os::path(ps2::g_options.memcard_path).parent_path();
 else if(!ps2::g_options.program_path.empty())root=ps2::os::path(ps2::g_options.program_path).parent_path();
 if(root.empty())root=".";std::error_code error;auto full=fs::absolute(root,error);
 value=ps2::os::utf8(error?root:full.lexically_normal());return value.c_str();
}
namespace mka::emitted::reference {
void family_before_execution(){if(registry.poisoned()){
 std::fprintf(stderr,"[resource-owner-epochs] REFUSED execution_after_incomplete_restore\n");std::fflush(stderr);
 throw std::runtime_error("resource owner: execution after incomplete restore");}}
bool preferred_family(ps2::Cpu&c){return adapter.try_dispatch(c);}
bool preferred_missing_family(ps2::Cpu&c){return adapter.try_dispatch(c,true);}
void reset_family_after_state_replacement(){registry.reset();adapter.reset();}
void retire_family_thread(int id){adapter.retire(id);}
bool family_pending(){return adapter.pending();}
}
'''


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['base-build','controls','lifecycle-hooks','compiler','output']:p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--expected-controls-sha',required=True);p.add_argument('--expected-hooks-sha',required=True)
    p.add_argument('--system-proof',type=Path);p.add_argument('--expected-system-sha')
    p.add_argument('--iop-proof',type=Path);p.add_argument('--expected-iop-sha')
    p.add_argument('--compile-only',action='store_true',help='Check the changed private EE units only; do not link, execute or install.')
    p.add_argument('--parameter-proof',type=Path);p.add_argument('--expected-parameter-sha')
    a=p.parse_args();base=a.base_build.resolve();out=a.output.resolve();normal=ROOT/'build/r192-compiled-konquest'
    need(out.is_relative_to(ROOT/'build') and not out.exists(),'fresh_owned_output_required')
    need(digest(a.compiler)=='ee85bef2c733e24a26c22be3df94bd631cf9528a048fce91241c182a936737fe','frozen_compiler_required')
    receipt=base/'BUILD.json';need(digest(receipt)=='57b220ba8816b596b44f04ee467b7d90a546bd58d95e1e7dc315ee06b6917694','frozen_base_receipt_changed');old=json.loads(receipt.read_text())
    need(digest(old['candidate'])==old['candidate_sha256']=='6bab8b9cfa9f0ae28bc027841825f1ce0ebd89fd75e046fb7e189f88de7715b5','frozen_base_PE_changed')
    proof=a.controls/'RESULT.json';need(digest(proof)==a.expected_controls_sha,'general_controls_changed');checked=json.loads(proof.read_text());need(checked['status']=='PASS_GENERAL_SOURCE_EQUATION_ACTUAL_CONTROLS' and not checked['input_changes'],'general_controls_required')
    parameter_pins=[]
    if not a.compile_only:
        need(a.parameter_proof is not None and a.expected_parameter_sha,'parameter_space_gate_required_before_linking_general_bindings')
        need(digest(a.parameter_proof)==a.expected_parameter_sha,'parameter_space_proof_changed');parameters=json.loads(a.parameter_proof.read_text())
        need(parameters['status']=='PASS_BOUNDED_15_PATTERN_PARAMETER_SPACE' and not parameters['input_changes'] and len(parameters['pattern_coverage'])==15,'all_15_parameter_patterns_must_pass')
        for row in parameters['source_pins']:need(digest(row['path'])==row['sha256'],'parameter_space_source_changed:'+row['path']);parameter_pins.append(row)
        parameter_pins.append(pin(a.parameter_proof))
    hook=a.lifecycle_hooks/'RESULT.json';need(digest(hook)==a.expected_hooks_sha,'lifecycle_proof_changed');hooks=json.loads(hook.read_text());need(hooks['status']=='SOURCE_LABEL_LIFECYCLE_HOOK_OBJECTS_PREPARED' and not hooks['input_changes'] and len(hooks['units'])==4,'four_verified_hook_units_required')
    frozen=old['source_pins']+old['private_sources']+old['private_objects']+old['package_pins']
    for row in frozen:need(digest(row['path'])==row['sha256'],'base_input_changed:'+row['path'])
    for row in checked['source_pins']+hooks['source_pins']:need(digest(row['path'])==row['sha256'],'affected_proof_input_changed:'+row['path'])
    need(shutil.disk_usage(out.parent).free>(12<<30)+Path(old['candidate']).stat().st_size+(64<<20),'12GiB_build_reserve_required')
    out.mkdir(parents=True);source=out/'source';source.mkdir();target=out/'out';target.mkdir()
    core=ROOT/'tools/research/r192_aot_modules';snapshots=[]
    names=['reference_general_resource_adapter.hpp','reference_resource_owner_epochs.hpp','effective_resource_construction.hpp']
    for name in names:
        q=core/name;dest=source/name;dest.write_bytes(q.read_bytes());snapshots.append(dict(original=pin(q),snapshot=pin(dest)))
    # Subsequent canonical edits cannot change the actual compiled subjects;
    # these private immutable snapshots are the measured build inputs.
    contract0=base/'source/resource_contract.hpp';m=re.search(r'c\.caller=\{[^}]+\};',contract0.read_text());need(m is not None,'original_caller_contract_missing')
    contract=source/'general_contract.hpp';contract.write_text('static const mka::emitted::general_resource_reference::Contract resource_contract=[](){mka::emitted::general_resource_reference::Contract c{};'+m[0]+'return c;}();\n',newline='\n')
    dispatch=source/'dispatch.cpp';s=(base/'source/dispatch.cpp').read_text();old_header=(core/'reference_resource_event_adapter.hpp').as_posix();need(s.count(old_header)==1,'dispatch_header_seam_changed')
    s=s.replace(old_header,(source/'reference_general_resource_adapter.hpp').as_posix());needle='        record_miss(pc);';need(s.count(needle)==1,'pre_miss_seam_changed');s=s.replace(needle,'        if (mka::emitted::reference::preferred_missing_family(c)) return;\n'+needle)
    s=s.replace('namespace ps2 {','namespace mka::emitted::reference {bool preferred_missing_family(ps2::Cpu&);void family_before_execution();}\nnamespace ps2 {',1)
    seam='void dispatch(Cpu &c, std::uint32_t, std::uint32_t depth) {';need(s.count(seam)==1,'EE_execution_poison_seam_changed');s=s.replace(seam,seam+'\n    mka::emitted::reference::family_before_execution();');dispatch.write_text(s,newline='\n')
    source_id=hashlib.sha256(''.join(x['snapshot']['sha256'] for x in snapshots).encode()).hexdigest();cpp=source/'reference_adapter.cpp';cpp.write_text(ADAPTER.replace('@SOURCE_ID@',source_id),newline='\n')
    dependencies=[Path(__file__),receipt,proof,hook,a.compiler,contract0,*[Path(r['path']) for r in frozen],*[Path(r['path']) for r in checked['source_pins'] if Path(r['path']).resolve() not in {(core/n).resolve() for n in names}],*[Path(r['path']) for r in hooks['source_pins']],*[source/n for n in names],*(core.glob('*.hpp'))]
    # Canonical authored versions are recorded separately in snapshots; only
    # private copies are compiled. Existing stable core headers remain pinned.
    dependencies=[q for q in dependencies if q.resolve() not in {(core/n).resolve() for n in names}]
    pins=[pin(q) for q in dict.fromkeys(dependencies)]+parameter_pins;commands=[]
    def run(argv,name):
        cp=subprocess.run(argv,capture_output=True,timeout=240);q=out/(name+'.log');q.write_bytes(cp.stdout+cp.stderr);commands.append(dict(argv=argv,exit=cp.returncode,log=pin(q)));need(cp.returncode==0,'general_'+name+'_failed')
    flags=[str(a.compiler),'-std=c++20','-O2','-mavx2','-ffp-contract=off','-DNDEBUG','-DPS2_GPU','-I'+str(source),'-I'+str(core),'-I'+str(normal/'src/runtime/include'),'-I'+str(normal/'src/runtime/src'),'-I'+str(normal/'src/third_party/wgpu-native/include')]
    for q in [dispatch,cpp]:run([*flags,'-c',str(q),'-o',str(q.with_suffix('.obj'))],'compile-'+q.stem)
    if a.compile_only:
        need(a.system_proof is None and a.iop_proof is None,'compile_only_is_EE_source_only')
        changed=[r['path'] for r in pins if digest(r['path'])!=r['sha256']];need(not changed,'compile_only_input_changed')
        result=dict(status='GENERAL_RESOURCE_EE_UNITS_COMPILE_ONLY',source_identity=source_id,source_snapshots=snapshots,source_pins=pins,input_changes=changed,commands=commands,private_sources=[pin(q) for q in source.iterdir() if q.suffix in ('.hpp','.cpp')],private_objects=[pin(q.with_suffix('.obj')) for q in [dispatch,cpp]],scope='Affected EE adapter/dispatcher compile only. Includes global failed-restore execution gate; no linked/runtime/integrated poison proof or game acceptance follows.')
        q=out/'BUILD_ONLY.json';q.write_text(json.dumps(result,indent=2)+'\n',newline='\n');print(json.dumps(dict(status=result['status'],sha256=digest(q))));return
    link=list(old['link_arguments']);replaced=[]
    for i,value in enumerate(link):
        if Path(value).resolve() in {(base/'source/dispatch.obj').resolve(),(base/'source/reference_adapter.obj').resolve()}:link[i]=str(source/Path(value).name);replaced.append(Path(value).name)
        elif value.startswith('-Wl,-Map,'):link[i]='-Wl,-Map,'+str(out/'general-link.map')
    need(sorted(replaced)==['dispatch.obj','reference_adapter.obj'] and link[-2]=='-o','exact_reference_object_replacement_required')
    override=[]
    for row in hooks['units']:
        q=Path(row['object']['path']);need(digest(q)==row['object']['sha256'],'hook_override_changed');override.append(str(q));pins.append(pin(q))
    archives=[i for i,value in enumerate(link) if Path(value).name=='libps2game.a'];need(len(archives)==1 and digest(link[archives[0]])==hooks['archive']['sha256'],'matching_game_archive_required');link[archives[0]:archives[0]]=override
    # Additional integrated IOP/system object manifests have explicit old/new
    # objects; no archive or source lineage may be guessed by basename.
    for manifest,expected,label in [(a.system_proof,a.expected_system_sha,'system'),(a.iop_proof,a.expected_iop_sha,'IOP')]:
        if manifest is None:need(expected is None,'orphan_expected_proof_SHA');continue
        need(expected and digest(manifest)==expected,label+'_proof_changed');j=json.loads(manifest.read_text())
        if label=='IOP' and 'replacement_objects' not in j:
            need(j['status']=='FRESH_IOP_PRODUCTION_CONNECTOR_SCHEDULER_PASS' and not j['input_changes'],'fresh_IOP_component_required')
            lineage=ROOT/'build/r192-iop-loader-alias-candidate-01/BUILD.json';prior=json.loads(lineage.read_text())
            kernel=ROOT/'build/r192-iop-loader-alias-candidate-01/src/r192-v8-kernel.cpp'
            original=ROOT/'build/r192-iop-loader-alias-candidate-01/replacement-0.obj'
            matches=[r for r in prior['compile_receipts'] if str(kernel) in r.get('argv',r.get('command',[])) and str(original) in r.get('argv',r.get('command',[]))]
            need(len(matches)==1 and str(original) in old['link_arguments'],'fresh_IOP_original_kernel_lineage_missing')
            oldpin=next((r for r in frozen if Path(r['path']).resolve()==original.resolve()),None)
            need(oldpin and digest(original)==oldpin['sha256'],'fresh_IOP_original_kernel_changed')
            j={**j,'replacement_objects':[dict(original=oldpin,replacement=j['object'])]};pins.append(pin(lineage))
        for row in j['replacement_objects']:
            before=Path(row['original']['path']).resolve();after=Path(row['replacement']['path']);need(digest(before)==row['original']['sha256'] and digest(after)==row['replacement']['sha256'],label+'_object_identity_changed')
            indices=[i for i,v in enumerate(link) if not v.startswith('-') and Path(v).resolve()==before];need(len(indices)==1,label+'_exact_replace_seam_missing');link[indices[0]]=str(after);pins.extend([pin(before),pin(after)])
        for row in j.get('extra_objects',[]):need(digest(row['path'])==row['sha256'],label+'_extra_object_changed');link[0:0]=[row['path']];pins.append(pin(row['path']))
        for row in j.get('source_pins',[]):need(digest(row['path'])==row['sha256'],label+'_source_changed');pins.append(row)
        link.extend(j.get('extra_link_arguments',[]));pins.append(pin(manifest))
    outputs=[i for i,value in enumerate(link) if value=='-o'];need(len(outputs)==1,'unique_link_output_required');link[outputs[0]+1]=str(target/'ps2run.exe');response=out/'link.rsp';response.write_text('\n'.join('"'+x.replace('\\','/').replace('"','\\"')+'"' for x in link)+'\n',newline='\n');run([str(a.compiler),'@'+str(response)],'link')
    package=[]
    for q in (base/'out').glob('*.dll'):os.link(q,target/q.name);package.append(pin(target/q.name))
    for domain in ['ee','vu']:
        folder=target/'aot-modules'/domain;folder.mkdir(parents=True)
        for q in (base/'out/aot-modules'/domain).iterdir():need(q.is_file(),'package_shape_changed');os.link(q,folder/q.name);need(digest(folder/q.name)==digest(q),'package_bytes_changed');package.append(pin(folder/q.name))
    changed=[r['path'] for r in pins if digest(r['path'])!=r['sha256']];need(not changed,'general_build_input_changed')
    result=dict(status='GENERAL_RESOURCE_AND_SOURCE_LIFECYCLE_CANDIDATE_BUILT',candidate=str(target/'ps2run.exe'),candidate_sha256=digest(target/'ps2run.exe'),source_identity=source_id,base_receipt=pin(receipt),source_snapshots=snapshots,source_pins=pins,input_changes=changed,commands=commands,link_arguments=link,package_pins=package,private_sources=[pin(q) for q in source.iterdir() if q.suffix in ('.hpp','.cpp')],private_objects=[pin(q.with_suffix('.obj')) for q in [dispatch,cpp]],scope='Disposable build only; source-bound fresh Plans and explicitly limited snapshot reference behavior. Unknowns retain compiled AOT, no live compiler/interpreter introduced, no normal/protected install or native credit.')
    q=out/'BUILD.json';q.write_text(json.dumps(result,indent=2)+'\n',newline='\n');print(json.dumps(dict(status=result['status'],candidate_sha256=result['candidate_sha256'],build_sha256=digest(q))))


if __name__=='__main__':main()
