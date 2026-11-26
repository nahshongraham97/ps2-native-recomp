"""Capture bounded real post-selection EE producer plans in a disposable host.

Original compiled callees/preempt execute unchanged. This is input/boundary
observation, not native replacement or a generic runtime code decoder. The
observer refuses unproved plans explicitly and preserves original execution.
"""
from pathlib import Path
import argparse
import json
import os
import shutil
import subprocess
import sys

sys.path.insert(0,str(Path(__file__).resolve().parent))
import specialize_r192_stub_family as family
import capture_r192_family_boundary as capture


def once(text,old,new):
    family.require(text.count(old)==1,'original_observer_seam_changed:'+old[:60]);return text.replace(old,new)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ['projection-proof','candidate-build','source-fork','state','card-copy','compiler','output']:
        p.add_argument('--'+n,type=Path,required=True)
    p.add_argument('--prefer-low-budget',action='store_true',help='Observe natural first entries below their source worst-cost bound; never changes runtime budget.')
    a=p.parse_args();out=a.output.resolve();n=a.candidate_build.resolve()
    family.require(out.is_relative_to((family.W/'build/oracle-telemetry').resolve()) and not out.exists(),'fresh_owned_output_required')
    for path in [a.state,a.card_copy]:family.require(path.resolve().is_relative_to((family.W/'build/oracle-telemetry').resolve()),'owned_input_copy_required')
    family.require(family.pin(n/'out/ps2run.exe')['sha256']==capture.N_SHA,'protected_N_identity_changed')
    proof=a.projection_proof/'RESULT.json';family.require(family.pin(proof)['sha256']=='39cb6e2801d9a17f69d0485a33cf962b8c1b015d00548be8df6314713d4e5219','full_reference_projection_proof_changed')
    prior=json.loads(proof.read_text());family.require(prior['status']=='PASS_BOUNDED_FULL_REFERENCE_PROJECTION' and not prior['input_changes'],'full_reference_projection_proof_not_passed')
    for pin in prior['source_pins']:family.require(family.pin(Path(pin['path']))==pin,'projection_source_changed')
    lineage=n/'SOURCE_BUILD_DIGESTS.json';record=json.loads(lineage.read_text())
    for name in ['ps2run.exe','libps2game.a','libps2runtime.a','aot-modules/ee/manifest.txt']:
        family.require(family.pin(n/'out'/name)['sha256']==record['artifacts'][name],'protected_N_artifact_changed:'+name)
    dispatch=n/'src/runtime/src/dispatch.cpp'
    family.require(family.pin(dispatch)['sha256']=='677a873628c45ddc7d5efae2b5ab187cc6fad38bd1b0e84eb080cbee87227ea2','original_dispatch_changed')
    family.require(shutil.disk_usage(out.parent).free>(12+3)*(1<<30),'capture_reserve_unavailable')
    out.mkdir(parents=True);src=out/'source';src.mkdir();observations=out/'observations';observations.mkdir();run=out/'run';run.mkdir()
    plan=family.W/'tools/research/r192_aot_modules/reference_resource_layout.hpp'
    declarations=src/'resource_observer.hpp';declarations.write_text('#pragma once\n#include "ps2/generated.hpp"\nnamespace resource_observer {unsigned before(ps2::Cpu&,const char*,const char*);void after(ps2::Cpu&,unsigned);}\n',encoding='utf8',newline='\n')
    text=dispatch.read_text();text='#include "resource_observer.hpp"\n'+text
    text=once(text,'            mu->function(c, entry, depth);','            const auto observation=resource_observer::before(c,"sealed",m->module->name);\n            mu->function(c, entry, depth);\n            resource_observer::after(c,observation);')
    text=once(text,'    u->function(c, id, depth);','    const auto observation=resource_observer::before(c,"static","main");\n    u->function(c, id, depth);\n    resource_observer::after(c,observation);')
    copied=src/'dispatch.cpp';copied.write_text(text,encoding='utf8',newline='\n')
    observer=src/'resource_observer.cpp';observer.write_text(OBSERVER.replace('@HEADER@',plan.as_posix()).replace('@OUTPUT@',observations.as_posix()).replace('@LOW_BUDGET@','true' if a.prefer_low_budget else 'false'),encoding='utf8',newline='\n')
    host=n/'out/CMakeFiles/ps2run.dir/host'
    objects=[host/x for x in ['main.cpp.obj','mka_unit_wrappers.cpp.obj','r192_aot_modules/loader.cpp.obj','profile_win.cpp.obj','window.cpp.obj','keymap.cpp.obj','stretch.cpp.obj']]
    libs=[n/'out/libps2game.a',n/'out/libps2runtime.a',n/'src/third_party/sdl2/lib/libSDL2.dll.a',n/'src/third_party/wgpu-native/lib/libwgpu_native.a']
    package=list((n/'out/aot-modules/ee').iterdir());family.require(all(x.is_file() for x in package),'sealed_package_shape_changed')
    driver=family.W/'tools/research/replay_r192_aot_sessions.py'
    program=next(Path(x['path']) for x in prior['source_pins'] if Path(x['path']).name=='SLUS_214.10')
    inputs=[Path(__file__),Path(family.__file__),Path(capture.__file__),proof,lineage,dispatch,plan,plan.with_name('semantic_resource_pipeline.hpp'),plan.with_name('semantic_vector_effects.hpp'),n/'out/ps2run.exe',a.state,a.card_copy,a.compiler,driver,program,*objects,*libs,*package,
            *(n/'src/runtime/include/ps2').glob('*.hpp'),n/'src/runtime/src/kernel.hpp']
    pins=[family.pin(x) for x in dict.fromkeys(inputs)]
    configured=dict(case_slots=list(range(1,7)),logical_plan_cap=6,guard_cap_per_case=2,segment_cap_per_case=3,
                    minimum_free_bytes=12*(1<<30),observation_only=True,prefer_low_budget=a.prefer_low_budget,state=family.pin(a.state),companion=family.pin(a.card_copy),source_binary=family.pin(n/'out/ps2run.exe'))
    (out/'CONFIGURED.json').write_text(json.dumps(configured,indent=2)+'\n',encoding='utf8',newline='\n')
    commands=[]
    def command(label,argv):
        cp=subprocess.run(argv,capture_output=True);log=out/(label+'.log');log.write_bytes(cp.stdout+cp.stderr)
        commands.append(dict(name=label,argv=argv,exit=cp.returncode,log=family.pin(log)));return cp
    private=[]
    for path in [copied,observer]:
        obj=path.with_suffix('.obj');private.append(obj)
        args=[str(a.compiler),'-std=gnu++20','-mavx2','-ffp-contract=off','-O2','-DNDEBUG','-DPS2_GPU','-I'+str(n/'src/runtime/include'),'-I'+str(n/'src/runtime/src'),'-I'+str(n/'src/third_party/wgpu-native/include'),'-c',str(path),'-o',str(obj)]
        family.require(command('compile-'+path.stem,args).returncode==0,'observer_compile_failed:'+path.name)
    exe=run/'ps2run.exe'
    args=[str(a.compiler),'-mavx2','-ffp-contract=off','-O3','-DNDEBUG','-mwindows','-static-libgcc','-static-libstdc++','-Wl,--wrap=_ZN3ps27preemptERNS_3CpuE',
          *['-Wl,--wrap=_ZN3gen9unit_'+u+'ERN3ps23CpuEjj' for u in ['0018','0020','0025']],*[str(x) for x in private],*[str(x) for x in objects],'-o',str(exe),*[str(x) for x in libs],'-lbcrypt','-lwinmm',
          *['-l'+x for x in ['ws2_32','userenv','ntdll','synchronization','pthread','m','kernel32','user32','gdi32','winspool','shell32','ole32','oleaut32','uuid','comdlg32','advapi32']]]
    family.require(command('link-observation-only-host',args).returncode==0,'observer_link_failed')
    for name in ['SDL2.dll','dxcompiler.dll','dxil.dll']:os.link(n/'out'/name,run/name)
    target=run/'aot-modules/ee';target.mkdir(parents=True)
    for path in package:os.link(path,target/path.name)
    family.require(command('verify-hardlinked-readonly-package',[str(exe),str(program),'--verify-aot-package']).returncode==0,'observer_package_verification_failed')
    # Package verification exits without emulation and writes its empty summary.
    empty=observations/'summary.json'
    if empty.exists():empty.rename(observations/'package-verification-summary.json')
    family.require(shutil.disk_usage(out).free>(12+2)*(1<<30),'capture_reserve_before_launch_unavailable')
    args=[sys.executable,str(driver),'--candidate',str(exe),'--expected-binary-sha',family.pin(exe)['sha256'],'--source-fork',str(a.source_fork.resolve()),'--state',str(a.state.resolve()),'--card-copy',str(a.card_copy.resolve()),'--additional-frames','90','--output',str(out/'probe')]
    actual=command('actual-original90-postselection-observer',args)
    summary=json.loads((observations/'summary.json').read_text()) if (observations/'summary.json').exists() else None
    changes=[x['path'] for x in pins if family.pin(Path(x['path']))!=x]
    passed=actual.returncode==0 and summary and summary['selected']>0 and summary['usable_completed']>0 and not changes
    result=dict(schema='r192_actual_resource_plan_observations_v1',status='PASS_BOUNDED_ORIGINAL_PLAN_OBSERVATIONS' if passed else 'REFUSED',invocation=[sys.executable,*sys.argv],
                source_pins=pins,input_changes=changes,configured=configured,commands=commands,summary=summary,
                artifacts=[family.pin(x) for x in [declarations,copied,observer,*private,exe]],observations=[family.pin(x) for x in observations.iterdir() if x.is_file()],
                actual_probe=family.pin(out/'probe/RESULT.json') if (out/'probe/RESULT.json').exists() else None,
                limits=['Original compiled EE code and actual original preempt executed unchanged; no generic native replacement installed.',
                        'Six LOGICAL plans maximum; unadmitted invocations are not distinct families. Orphans/caps/partial layouts remain named and are not success.',
                        'Post-selection Cpu.base is directly observed; external callbacks are before/after boundary effects, not a fabricated store width/order trace.',
                        'Offline original-builder word identity and native shadows must pass separately before any native semantic admission.',
                        'Sealed package/dependencies are authenticated read-only hardlinks; cards/states are copied by the existing owned replay driver. Protected N/originals/settings unchanged.'])
    dest=out/'RESULT.json';dest.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8',newline='\n');print(json.dumps(dict(status=result['status'],sha256=family.pin(dest)['sha256'],result=str(dest))))
    return 0 if passed else 1


OBSERVER=r'''
#include "resource_observer.hpp"
#include "kernel.hpp"
#include "ps2/system.hpp"
#include "@HEADER@"
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <memory>
#include <compare>
#include <iomanip>
namespace resource_observer {
using namespace ps2;using namespace mka::emitted::reference_resources;
constexpr const char*folder="@OUTPUT@";
struct Key{std::uint64_t stack;unsigned object,base;int thread;auto operator<=>(const Key&)const=default;};
struct Case{unsigned id,segment=0,guards=0,prior=0;Key key;Descriptor descriptor;Layout layout;std::string signature;std::uint64_t code_hash;bool active=false,refused=false;};
static std::map<Key,std::unique_ptr<Case>>pending;static std::set<std::string>signatures;static std::map<std::string,unsigned>refusals;
static std::map<unsigned,std::string>case_status;
static unsigned low_budget_skipped=0;
static unsigned selected=0,completed=0,usable_completed=0,suspended=0,invocations=0,active=0,orphans=0;
struct CaptureFailure:std::runtime_error{using std::runtime_error::runtime_error;};
static unsigned word(unsigned at){if(at>kRamSize-4)throw std::runtime_error("descriptor read outside RAM");unsigned v;std::memcpy(&v,g_ram+at,4);return v;}
static unsigned half(unsigned at){if(at>kRamSize-2)throw std::runtime_error("descriptor read outside RAM");unsigned short v;std::memcpy(&v,g_ram+at,2);return v;}
static std::uint64_t fingerprint(unsigned at,unsigned bytes){if(at>kRamSize-bytes)throw std::runtime_error("code extent outside RAM");std::uint64_t h=1469598103934665603ull;for(unsigned i=0;i<bytes;++i)h=(h^g_ram[at+i])*1099511628211ull;return h;}
static std::string prefix(const Case&x){return std::string("case-")+std::to_string(x.id)+"-segment-"+std::to_string(x.segment);}
static void save(const std::string&name,const void*data,std::size_t bytes){
 std::error_code error;const auto free=std::filesystem::space(folder,error);if(error)throw CaptureFailure("resource observation disk query failed");
 if(free.available<(12ull<<30)+bytes)throw CaptureFailure("resource observation refused:12GiB disk reserve");
 std::ofstream f(std::string(folder)+"/"+name,std::ios::binary);f.write(static_cast<const char*>(data),bytes);if(!f)throw CaptureFailure("resource observation write failed");
}
static void snapshot(Case&x,const std::string&label,const Cpu&c){save(prefix(x)+"-"+label+"-Cpu.bin",&c,sizeof c);save(prefix(x)+"-"+label+"-RAM.bin",g_ram,kRamSize);}
static Descriptor descriptor(unsigned object,std::string&signature){
 Descriptor d{};const auto owner=word(object+72);if(owner<16||owner>kRamSize-544)throw std::runtime_error("owner extent unproved");
 d.attribute_count=word(owner+536);const auto attributes=word(owner+540);d.effect_count=word(object+192);d.filter_count=word(object+80);
 if(!d.attribute_count||d.attribute_count>20||d.effect_count>13||d.filter_count>1||attributes>kRamSize-d.attribute_count*12)throw std::runtime_error("logical counts or attribute bounds unproved");
 std::ostringstream s;s<<d.attribute_count<<":";
 for(unsigned i=0;i<d.attribute_count;++i){auto&a=d.attributes[i];a={word(attributes+i*12),word(attributes+i*12+4),word(attributes+i*12+8)};s<<a.tag<<","<<a.stream<<","<<a.offset<<";";}
 s<<"E"<<d.effect_count<<":";
 for(unsigned i=0;i<d.effect_count;++i){auto&e=d.effects[i];const unsigned at=object+208+i*48;e.id=half(at);e.destination_tag=word(at+8);
  if(e.id==2||e.id==3||e.id==25||e.id==17)e.source_tag=word(at+16);
  for(unsigned k=0;k<3;++k)e.constant[k]=word(at+16+k*4);
  e.remap={std::int32_t(word(at+32)),std::int32_t(word(at+36)),word(at+24),word(at+28)};
  s<<e.id<<","<<e.destination_tag<<","<<e.source_tag<<";";
 }
 s<<"F"<<d.filter_count<<":";
 for(unsigned i=0;i<d.filter_count;++i){const auto at=object+84+i*24;if(word(at+16))throw std::runtime_error("filter control16 unproved");d.filters[i]={half(at),word(at+4),word(at+20)};s<<d.filters[i].id<<","<<d.filters[i].source_tag<<";";}
 signature=s.str();return d;
}
static void metadata(const Case&x,const char*route,const char*module,const Cpu&c){
 const auto owner=word(x.key.object+72),attributes=word(owner+540);std::ostringstream j;
 j<<"{\"id\":"<<x.id<<",\"thread\":"<<x.key.thread<<",\"stack\":"<<x.key.stack<<",\"object\":"<<x.key.object<<",\"base\":"<<x.key.base<<",\"owner\":"<<owner<<",\"attributes_address\":"<<attributes<<",\"route\":"<<std::quoted(route)<<",\"selected_Cpu_base\":"<<c.base<<",\"module\":"<<std::quoted(module?module:"")<<",\"source_data_start_words\":"<<x.layout.data_start_words<<",\"source_total_words\":"<<x.layout.total_words<<",\"attributes\":[";
 for(unsigned i=0;i<x.descriptor.attribute_count;++i){if(i)j<<",";const auto&a=x.descriptor.attributes[i];j<<"["<<a.tag<<","<<a.stream<<","<<a.offset<<"]";}
 j<<"],\"effects_raw\":[";for(unsigned i=0;i<x.descriptor.effect_count;++i){if(i)j<<",";j<<"[";for(unsigned k=0;k<12;++k){if(k)j<<",";j<<word(x.key.object+208+i*48+k*4);}j<<"]";}
 j<<"],\"filters_raw\":[";for(unsigned i=0;i<x.descriptor.filter_count;++i){if(i)j<<",";j<<"[";for(unsigned k=0;k<6;++k){if(k)j<<",";j<<word(x.key.object+84+i*24+k*4);}j<<"]";}
 j<<"]}\n";const auto text=j.str();save("case-"+std::to_string(x.id)+".json",text.data(),text.size());
}
unsigned before(Cpu&c,const char*route,const char*module){
 if(c.r[31].d[0]!=0x2e6180u)return 0;++invocations;const auto object=c.r[19].w[0];
 if(&c!=&g_cpu||object<16||object>kRamSize-928||(object&15)||c.r[19].d[0]!=object||c.r[29].d[0]<16||c.r[29].d[0]>kRamSize-496||(c.r[29].w[0]&15)||kernel::current_thread()<=0){++refusals["caller_object_thread_frame_unproved"];return 0;}
 const auto base=word(object+76),pc=c.pc&~kUnwind;Key key{c.r[29].d[0],object,base,kernel::current_thread()};auto found=pending.find(key);
 try{
  if(found==pending.end()){
   if(pc!=base){++orphans;++refusals["orphan_resume_not_rehydrated"];return 0;}
   if(selected>=6){++refusals["six_logical_plan_cap"];return 0;}
   std::string signature;auto d=descriptor(object,signature);auto p=derive_layout(d);p.require_intact();
   if(@LOW_BUDGET@&&c.budget>=0&&std::uint64_t(c.budget)>=cost(timing(d),c.r[10].w[0],c.r[10].w[0])){++low_budget_skipped;return 0;}
   if(signatures.contains(signature)){++refusals["logical_plan_already_sampled"];return 0;}
   if((base&15)||base>kRamSize-4*(p.total_words+4)||word(object+916)!=p.data_start_words||c.r[12].d[0]!=base||c.r[11].d[0]!=base+4*p.data_start_words||c.r[10].d[0]>0x7fffffffu||c.r[8].d[0]>0x7fffffffu||c.r[9].d[0]>0x7fffffffu||c.f[12]!=c.vu0.vf[1].w[0])throw std::runtime_error("original target_data_count_step_stride contract unproved");
   auto x=std::make_unique<Case>();x->id=++selected;case_status[x->id]="SELECTED";x->key=key;x->descriptor=d;x->layout=p;x->signature=signature;x->code_hash=fingerprint(base,p.data_start_words*4);metadata(*x,route,module,c);signatures.insert(signature);found=pending.emplace(key,std::move(x)).first;
  }
  auto&x=*found->second;if(x.refused)return 0;
  if(x.active||++x.segment>3)throw std::runtime_error("active_or_segment_cap");
  if(pc!=base&&pc!=base+x.layout.prelude_words*4)throw std::runtime_error("unknown_resume_PC");
  std::string signature;descriptor(object,signature);if(signature!=x.signature||word(object+916)!=x.layout.data_start_words||fingerprint(base,x.layout.data_start_words*4)!=x.code_hash)throw std::runtime_error("code_or_logical_descriptor_lifetime_changed");
  x.prior=active;active=x.id;x.active=true;snapshot(x,"before",c);
  std::fprintf(stderr,"[resource-observe] before case=%u segment=%u PC=%08X base=%08X route=%s thread=%d SP=%08X\n",x.id,x.segment,c.pc,c.base,route,key.thread,c.r[29].w[0]);return x.id;
 }catch(const CaptureFailure&){if(found!=pending.end()){auto&x=*found->second;x.refused=true;x.active=false;active=x.prior;case_status[x.id]="REFUSED_CAPTURE_IO";}throw;
 }catch(const std::exception&e){++refusals[e.what()];if(found!=pending.end()){auto&x=*found->second;x.refused=true;if(x.active){x.active=false;active=x.prior;}case_status[x.id]=std::string("UNADMITTED:")+e.what();}std::fprintf(stderr,"[resource-observe] UNADMITTED PC=%08X object=%08X reason=%s\n",pc,object,e.what());return 0;}
}
static Case*lookup(unsigned id){for(auto&[key,x]:pending)if(x->id==id)return x.get();return nullptr;}
void after(Cpu&c,unsigned id){if(!id)return;auto*x=lookup(id);if(!x)return;snapshot(*x,"after",c);x->active=false;active=x->prior;
 if(c.pc==0x2e6180){++completed;if(!x->refused){++usable_completed;case_status[id]="COMPLETE_USABLE_OBSERVATION";}const auto key=x->key;pending.erase(key);}else if((c.pc&~kUnwind)==x->key.base+x->layout.prelude_words*4&&(c.pc&kUnwind)){++suspended;if(!x->refused)case_status[id]="SUSPENDED_PENDING";}else{x->refused=true;case_status[id]="UNADMITTED:original_boundary_PC_unproved";++refusals["original_boundary_PC_unproved"];}
 std::fprintf(stderr,"[resource-observe] after case=%u PC=%08X budget=%d\n",id,c.pc,c.budget);
}
struct GuardToken{unsigned id,index;};
static GuardToken guard_before(Cpu&c){auto*x=lookup(active);if(!x||x->refused||!x->active||c.r[29].d[0]!=x->key.stack||kernel::current_thread()!=x->key.thread||c.r[31].d[0]!=0x2e6180)return{};
 if(++x->guards>2){x->refused=true;case_status[x->id]="UNADMITTED:two_guard_snapshot_cap";++refusals["two_guard_snapshot_cap"];return{};}
 snapshot(*x,"guard-"+std::to_string(x->guards)+"-before",c);return{x->id,x->guards};
}
static void guard_after(Cpu&c,GuardToken token,bool result){if(!token.id)return;auto*x=lookup(token.id);if(!x)return;snapshot(*x,"guard-"+std::to_string(token.index)+"-after",c);const unsigned value=result;save(prefix(*x)+"-guard-"+std::to_string(token.index)+"-result.bin",&value,4);}
struct Summary{~Summary(){try{std::ostringstream j;j<<"{\"selected\":"<<selected<<",\"completed\":"<<completed<<",\"usable_completed\":"<<usable_completed<<",\"suspended\":"<<suspended<<",\"low_budget_selection_skipped\":"<<low_budget_skipped<<",\"caller_invocations\":"<<invocations<<",\"orphans\":"<<orphans<<",\"pending\":"<<pending.size()<<",\"refusals\":{";bool first=true;for(auto&[reason,count]:refusals){if(!first)j<<",";first=false;j<<std::quoted(reason)<<":"<<count;}j<<"},\"case_status\":{";first=true;for(auto&[id,status]:case_status){if(!first)j<<",";first=false;j<<std::quoted(std::to_string(id))<<":"<<std::quoted(status);}j<<"}}\n";auto text=j.str();save("summary.json",text.data(),text.size());}catch(const std::exception&e){std::fprintf(stderr,"[resource-observe] summary REFUSED %s\n",e.what());}}};static Summary summary;
}
extern "C" bool __real__ZN3ps27preemptERNS_3CpuE(ps2::Cpu&);
extern "C" bool __wrap__ZN3ps27preemptERNS_3CpuE(ps2::Cpu&c){const auto token=resource_observer::guard_before(c);const bool result=__real__ZN3ps27preemptERNS_3CpuE(c);resource_observer::guard_after(c,token,result);return result;}
'''


if __name__=='__main__':raise SystemExit(main())
