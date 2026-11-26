"""Observe actual original call/return, then compare a native shadow offline.

Recompiles one privately copied AOT unit, never a protected/candidate source.
The actual callee still runs unchanged. No native replacement is installed.
"""
from pathlib import Path
import argparse
import json
import re
import shutil
import struct
import subprocess
import sys

sys.path.insert(0,str(Path(__file__).resolve().parent))
import specialize_r192_stub_family as family
import capture_r192_family_boundary as capture_tool


def once(text,before,after):
    family.require(text.count(before)==1,'observation_seam_changed:'+before[:50])
    return text.replace(before,after)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ['vector-proof','candidate-build','source-fork','state','card-copy','compiler','output']:
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--replay-observations',type=Path,help='Pinned observer01 receipt directory; offline comparisons only, never launches.')
    a=parser.parse_args();out=a.output.resolve();n=a.candidate_build.resolve()
    family.require(out.is_relative_to((family.W/'build/oracle-telemetry').resolve()) and not out.exists(),'fresh_owned_output_required')
    family.require(family.pin(n/'out/ps2run.exe')['sha256']==capture_tool.N_SHA,'N_reference_binary_changed')
    for path in [a.state,a.card_copy]:family.require(path.resolve().is_relative_to((family.W/'build/oracle-telemetry').resolve()),'owned_input_copy_required')
    proof=a.vector_proof/'RESULT.json';record=json.loads(proof.read_text())
    family.require(family.pin(proof)['sha256']=='a57fd9d7fe9c7d363bc3f256bca5b9af6b953e0847490da2244a1a08f7603397','frozen_vector_proof_changed')
    family.require(record['status']=='PASS_TYPED_VECTOR_SIDE_OUTPUTS','vector_proof_required')
    fixture_pin=next(r for r in record['runs'][0]['fixture_pins'] if Path(r['path']).suffix=='.cpp')
    fixture=Path(fixture_pin['path']);family.require(family.pin(fixture)['sha256']==fixture_pin['sha256'],'vector_fixture_changed')
    fixture_text=fixture.read_bytes().decode().replace('\r\n','\n')
    words=[int(x.strip().removesuffix('u'),16) for x in re.search(r'constexpr unsigned program\[\]=\{([^}]+)\};',fixture_text)[1].split(',')]
    family.require(len(words)==61,'complete_program_identity_required')
    boundary_ram=next(Path(r['path']) for r in record['source_pins'] if Path(r['path']).name=='RAM.bin')
    boundary_cpu=boundary_ram.with_name('Cpu.bin');sample=boundary_ram.read_bytes();sample_object=struct.unpack_from('<I',boundary_cpu.read_bytes(),19*16)[0]
    opaque_records=sample[sample_object+208:sample_object+400];filter_record=sample[sample_object+84:sample_object+108]
    original=n/'generated_ee/unit_0079.cpp';text=original.read_bytes().decode().replace('\r\n','\n')
    start=text.index('L_002E60E0:');stop=text.index('L_002E6188:',start)
    part=text[start:stop]
    part=once(part,'call(c, t, depth + 1u);','const unsigned observation=mka_observe::before(c,t);call(c, t, depth + 1u);mka_observe::after(c,observation);')
    out.mkdir(parents=True);data=out/'observations';data.mkdir();run=out/'run';run.mkdir();replayed=None;old_record=None
    if a.replay_observations:
        replayed=a.replay_observations/'RESULT.json'
        family.require(family.pin(replayed)['sha256']=='0219eacefa312ccf3c4734c623d7e8c1032b8339b275fa540fd0e64d9bd95cb9','actual_observation_receipt_changed')
        old_record=json.loads(replayed.read_text());data=(a.replay_observations/'observations').resolve()
        for row in [*old_record['observation_pins'],old_record['observer_binary']]:family.require(family.pin(Path(row['path']))['sha256']==row['sha256'],'actual_observation_artifact_changed')
        family.require(set(str(x.resolve()) for x in data.iterdir() if x.is_file())==set(r['path'] for r in old_record['observation_pins']),'observation_inventory_changed')
    observer=OBSERVER.replace('@WORDS@',','.join(hex(w)+'u' for w in words[:52])).replace('@DATA@',data.as_posix())
    authored=family.W/'tools/research/r192_aot_modules'
    text=once(text,'#include "units.hpp"','#include "units.hpp"\n'+observer)
    text=text[:text.index('L_002E60E0:')]+part+text[text.index('L_002E6188:',text.index('L_002E60E0:')):]
    unit=out/'observed_unit_0079.cpp';unit.write_text(text,encoding='utf8',newline='\n');obj=out/'observed_unit_0079.obj'
    if replayed:family.require(unit.read_bytes()==(a.replay_observations/unit.name).read_bytes(),'actual_observer_source_reconstruction_changed')
    host=n/'out/CMakeFiles/ps2run.dir/host'
    objects=[host/name for name in ['main.cpp.obj','mka_unit_wrappers.cpp.obj','r192_aot_modules/loader.cpp.obj','profile_win.cpp.obj','window.cpp.obj','keymap.cpp.obj','stretch.cpp.obj']]
    libraries=[n/'out/libps2game.a',n/'out/libps2runtime.a',n/'src/third_party/sdl2/lib/libSDL2.dll.a',n/'src/third_party/wgpu-native/lib/libwgpu_native.a']
    inputs=[Path(__file__),Path(family.__file__),Path(capture_tool.__file__),proof,fixture,boundary_ram,boundary_cpu,original,n/'src/runtime/src/kernel.hpp',n/'out/ps2run.exe',a.state,a.card_copy,a.compiler,*objects,*libraries,
            *authored.glob('semantic_*.hpp'),*sorted((n/'src/runtime/include/ps2').glob('*.hpp'))]
    if replayed:inputs += [replayed,*[Path(r['path']) for r in old_record['observation_pins']],Path(old_record['observer_binary']['path'])]
    pins=[family.pin(x) for x in dict.fromkeys(inputs)]
    compile_args=[str(a.compiler),'-std=c++20','-mavx2','-ffp-contract=off','-O2','-DNDEBUG',
                  '-I'+str(n/'src/runtime/include'),'-I'+str(n/'src/runtime/src'),'-I'+str(n/'generated_ee'),'-c',str(unit),'-o',str(obj)]
    exe=run/'ps2run.exe'
    link_args=[str(a.compiler),'-mavx2','-ffp-contract=off','-O3','-DNDEBUG','-mwindows','-static-libgcc','-static-libstdc++',
               *['-Wl,--wrap=_ZN3gen9unit_'+u+'ERN3ps23CpuEjj' for u in ['0018','0020','0025']],str(obj),*[str(x) for x in objects],'-o',str(exe),*[str(x) for x in libraries],'-lbcrypt','-lwinmm',
               *['-l'+x for x in ['ws2_32','userenv','ntdll','synchronization','pthread','m','kernel32','user32','gdi32','winspool','shell32','ole32','oleaut32','uuid','comdlg32','advapi32']]]
    commands=[]
    def command(name,args):
        cp=subprocess.run(args,capture_output=True);(out/(name+'.log')).write_bytes(cp.stdout+cp.stderr)
        commands.append(dict(name=name,argv=args,exit=cp.returncode));return cp
    if not replayed:
        family.require(command('compile-original-unit-with-observations',compile_args).returncode==0,'observer_unit_compile_failed')
        family.require(command('link-owned-observer',link_args).returncode==0,'observer_link_failed')
        for name in ['SDL2.dll','dxcompiler.dll','dxil.dll']:shutil.copyfile(n/'out'/name,run/name)
        package=run/'aot-modules/ee';package.mkdir(parents=True)
        for source in (n/'out/aot-modules/ee').iterdir():
            if source.is_file():shutil.copyfile(source,package/source.name)
        driver=family.W/'tools/research/replay_r192_aot_sessions.py'
        args=[sys.executable,str(driver),'--candidate',str(exe),'--expected-binary-sha',family.pin(exe)['sha256'],'--source-fork',str(a.source_fork.resolve()),
              '--state',str(a.state.resolve()),'--card-copy',str(a.card_copy.resolve()),'--additional-frames','90','--output',str(out/'probe')]
        actual=command('actual-owned-state90-original-observer',args)
    else:
        actual=subprocess.CompletedProcess([str(replayed)],0)
        commands.append(dict(name='authenticated-prior-actual-observations-no-launch',argv=[str(replayed)],exit=0))
    cases=sorted(data.glob('case-*-before-Cpu.bin'))
    comparison=[]
    if actual.returncode==0:
        for path in cases:
            case=path.name.removesuffix('-before-Cpu.bin')
            for suffix in ['before-RAM','after-Cpu','after-RAM']:family.require((data/(case+'-'+suffix+'.bin')).exists(),'incomplete_observed_case')
            original_main=fixture_text.index('\nint main(int argc,char**argv)')
            body=fixture_text[:original_main]+fixture_text[original_main:].replace('\nint main(int argc,char**argv)','\nstatic int prior_vector_matrix(int argc,char**argv)',1)
            body,n_includes=re.subn(r'#include "[^"\n]*semantic_no_yield_adapter\.hpp"','#include "'+(authored/'semantic_no_yield_adapter.hpp').as_posix()+'"',body)
            family.require(n_includes==1,'offline_vector_include_seam_changed')
            body+=COMPARE.replace('@CASE@',(data/case).as_posix()).replace('@RECORDS@',','.join(str(x) for x in opaque_records)).replace('@FILTER@',','.join(str(x) for x in filter_record[:20]))
            source=out/(case+'-compare.cpp');source.write_text(body,encoding='utf8',newline='\n');binary=out/(case+'-compare.exe')
            args=[str(a.compiler),'-std=c++20','-mavx2','-ffp-contract=off','-O1','-static-libgcc','-static-libstdc++','-I'+str(n/'src/runtime/include'),str(source),'-o',str(binary)]
            family.require(command(case+'-compile-shadow',args).returncode==0,'offline_observed_comparison_compile_failed')
            cp=command(case+'-compare-shadow',[str(binary)])
            plants=[]
            if cp.returncode==0:
                for flag in ['return','budget','VU','code','constant','order','transient']:
                    planted=command(case+'-planted-'+flag,[str(binary),'--planted-'+flag])
                    plants.append(dict(name=flag,exit=planted.returncode,expected_exit=1,stderr=planted.stderr.decode(errors='replace')))
            comparison.append(dict(case=case,exit=cp.returncode,stdout=cp.stdout.decode(errors='replace'),stderr=cp.stderr.decode(errors='replace'),
                                   plants=plants,fixture_pins=[family.pin(source),family.pin(binary)]))
    changes=[r['path'] for r in pins if family.pin(Path(r['path']))['sha256']!=r['sha256']]
    admitted=[r for r in comparison if 'PASS_ACTUAL_ORIGINAL_RETURN_NATIVE_SHADOW' in r['stdout']]
    refused=[r for r in comparison if r['exit']==2 and 'REFUSED_OBSERVED_INSUFFICIENT_BUDGET' in r['stderr']]
    passed=actual.returncode==0 and len(admitted)==2 and len(refused)==1 and len(comparison)==3 and not changes and all(p['exit']==1 and 'OBSERVATION_DIFFERENCE' in p['stderr'] for r in admitted for p in r['plants'])
    result=dict(status='PASS_BOUNDED_ACTUAL_ORIGINAL_OBSERVATION_NATIVE_SHADOW' if passed else 'REFUSED',commands=commands,cases=comparison,
                admitted_count=len(admitted),insufficient_budget_count=len(refused),source_pins=pins,input_changes=changes,
                observation_pins=[family.pin(x) for x in sorted(data.iterdir()) if x.is_file()],observer_binary=old_record['observer_binary'] if replayed else family.pin(exe),
                actual_reference_receipt=family.pin(replayed) if replayed else None,
                observer_fixture_pins=[family.pin(unit)]+([] if replayed else [family.pin(obj)]),
                scope=['Actual original call/return observed at2E6178/2E6180; all execution remains original compiled AOT.',
                       'Native shadow is offline on captured data; exact full RAM, complete VU, literal FCR, budget, defined CPU and return compared.',
                       'Ordered stores are checked against independently compiled original on the same captured entry; not a live write-order trace.',
                       'Low-budget observations are retained as named refusals; no continuation/native replacement/ownership/credit claim.'])
    target=out/'RESULT.json';target.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8',newline='\n')
    print(json.dumps(dict(status=result['status'],result=str(target),sha256=family.pin(target)['sha256'])))
    return 0 if passed else 1


OBSERVER=r'''
#include "kernel.hpp"
#include <cstdio>
#include <fstream>
#include <string>
#include <cstring>
namespace mka_observe {
static unsigned selected=0,known=0,unknown=0,high=0,low=0;
static const unsigned code[]={@WORDS@};
static void save(unsigned id,const char*label,const void*data,std::size_t bytes){
 const auto path=std::string("@DATA@/case-")+std::to_string(id)+"-"+label+".bin";
 std::ofstream file(path,std::ios::binary);file.write(static_cast<const char*>(data),bytes);if(!file)throw std::runtime_error("semantic observation output failed");
}
static unsigned word(unsigned address){unsigned result;std::memcpy(&result,ps2::g_ram+address,4);return result;}
static unsigned before(const ps2::Cpu&c,unsigned target){
 const auto object=c.r[19].w[0];
 if(object>ps2::kRamSize-928||target>ps2::kRamSize-244||(target&15)||word(object+76)!=target||word(object+916)!=52||std::memcmp(ps2::g_ram+target,code,sizeof code)){++unknown;return 0;}
 ++known;const auto n=c.r[10].d[0];const auto worst=16ull+34ull*n;
 const bool enough=n<=0x7fffffffu&&worst<=0x7fffffffu&&c.budget>=0&&static_cast<unsigned>(c.budget)>=worst;
 if((enough&&high>=2)||(!enough&&low>=1))return 0;
 if(enough)++high;else ++low;const auto id=++selected;
 save(id,"before-Cpu",&c,sizeof c);save(id,"before-RAM",ps2::g_ram,ps2::kRamSize);
 const int thread=ps2::kernel::current_thread();save(id,"before-thread",&thread,sizeof thread);
 std::fprintf(stderr,"[semantic-observation] before case=%u target=%08X object=%08X SP=%08X count=%llu budget=%d admission=%s\n",id,target,object,c.r[29].w[0],n,c.budget,enough?"no-yield":"REFUSED_insufficient_budget");return id;
}
static void after(const ps2::Cpu&c,unsigned id){if(!id)return;
 save(id,"after-Cpu",&c,sizeof c);save(id,"after-RAM",ps2::g_ram,ps2::kRamSize);
 const int thread=ps2::kernel::current_thread();save(id,"after-thread",&thread,sizeof thread);
 std::fprintf(stderr,"[semantic-observation] after case=%u pc=%08X budget=%d\n",id,c.pc,c.budget);
}
struct Summary {~Summary(){std::fprintf(stderr,"[semantic-observation] summary known=%u unknown=%u selected=%u high=%u low=%u\n",known,unknown,selected,high,low);}};
static Summary summary;
}
'''


COMPARE=r'''
static void actual_read(const char*part,void*destination,std::size_t bytes){std::ifstream f(std::string("@CASE@-")+part+".bin",std::ios::binary);f.read(static_cast<char*>(destination),bytes);if(!f||f.peek()!=EOF)throw std::runtime_error("observation input extent");}
int main(int argc,char**argv){try{
 mode=argc>1?argv[1]:"";
 Cpu before{},after{};actual_read("before-Cpu",&before,sizeof before);actual_read("after-Cpu",&after,sizeof after);
 actual_read("before-RAM",g_ram,kRamSize);const unsigned object=before.r[19].w[0],base=before.r[12].w[0];
 const auto n=before.r[10].d[0];
 try{typed::no_yield::admit(n,before.budget);}catch(const std::exception&e){if(std::string(e.what()).find("insufficient worst-case budget")!=std::string::npos){std::fprintf(stderr,"REFUSED_OBSERVED_INSUFFICIENT_BUDGET count=%llu budget=%d actual_return=%08X\n",n,before.budget,after.pc);return 2;}throw;}
 if(mode=="--planted-return")after.r[2].w[0]^=1;
 if(mode=="--planted-budget")--after.budget;
 if(mode=="--planted-VU")after.vu0.status^=0x40;
 if(mode=="--planted-code")g_ram[base]^=1;
 if(mode=="--planted-constant")g_ram[base+208]^=1;
 if(object>kRamSize-928||base>kRamSize-244||before.r[31].d[0]!=0x2e6180||before.r[11].d[0]!=base+208||ld32(object+76)!=base||ld32(object+916)!=52||ld32(object+192)!=4||ld32(object+80)!=1)throw std::runtime_error("actual caller/object contract");
 constexpr unsigned caller_words[]={0x8c8c004c,0x11800015,0x46006506,0x8e650028,0x8e640030,0x440aa000,0x8e68002c,0x8e620004,0x8e690034,0x00481818,0x70491018,0x00a33021,0x00823821,0x48aa0800,0x8e620394,0x8e640008,0x8e650010,0x00021080,0x8e6a0000,0x0180f809,0x01825821};
 if(std::memcmp(caller_words,g_ram+0x2e612c,sizeof caller_words))throw std::runtime_error("actual original caller words");
 for(unsigned i=0;i<48;++i)if(ld32(base+i*4)!=reachable_original_words[i])throw std::runtime_error("actual reachable original word");
 constexpr unsigned char opaque_records[]={@RECORDS@},filter_record[]={@FILTER@};
 for(unsigned i=0;i<192;++i){const auto record=i/48,offset=i%48;const bool numeric=(record==1&&offset>=16&&offset<28)||(record==2&&offset>=16&&offset<20)||(record==3&&offset>=24&&offset<40);if(!numeric&&g_ram[object+208+i]!=opaque_records[i])throw std::runtime_error("actual opaque descriptor field unproved");}
 if(std::memcmp(filter_record,g_ram+object+84,sizeof filter_record))throw std::runtime_error("actual filter binding unproved");
 int thread_before{},thread_after{};actual_read("before-thread",&thread_before,4);actual_read("after-thread",&thread_after,4);if(thread_before<=0||thread_before!=thread_after)throw std::runtime_error("actual thread lifetime");
 for(unsigned i=0;i<4;++i){unsigned r=object+208+i*48;auto&e=captured[i];e=effect(ld16(r),ld32(r+8));if(e.id==5)for(unsigned lane=0;lane<3;++lane)e.constant[lane]=ld32(r+16+lane*4);if(e.id==6)e.constant[0]=ld32(r+16);if(e.id==17){e.source_tag=ld32(r+16);e.remap.start=ld32(r+24);e.remap.extent=ld32(r+28);e.remap.upper=ld32(r+32);e.remap.lower=ld32(r+36);}}
 captured[1].constant[3]=captured[2].constant[0];filter_limit=ld32(object+104);auto p=pipeline("regenerated",before.f[12],before.r[11].w[0]);
 if(before.vu0.vf[1].w[0]!=p.step)throw std::runtime_error("actual original scalar/vector step relation");
 unsigned producer_flags=0;const auto&r=p.effects[3].remap;
 const unsigned constants[]={p.effects[1].constant[0],p.effects[1].constant[1],p.effects[1].constant[2],p.effects[2].constant[0],fp::cvt_s_w(r.upper),fp::ee::div(producer_flags,fp::ee::add(producer_flags,0xbd4ccccd,fp::cvt_s_w(unsigned(r.upper)-unsigned(r.lower))),r.extent),r.start,r.extent,p.filter_limit};
 if(std::memcmp(constants,g_ram+base+208,sizeof constants))throw std::runtime_error("actual descriptor/constant producer coherence");
 const unsigned owner=ld32(object+72);if(owner>kRamSize-544||ld32(owner+536)!=5)throw std::runtime_error("actual resource owner");const auto attributes=ld32(owner+540);if(attributes>kRamSize-60)throw std::runtime_error("actual resource table extent");
 constexpr unsigned attrs[]={256,0,0,259,0,16,257,0,32,258,0,12,769,1,0};if(std::memcmp(attrs,g_ram+attributes,sizeof attrs))throw std::runtime_error("actual resource layout binding");
 auto overlaps=[](unsigned begin,unsigned long long size,unsigned protected_begin,unsigned protected_size){return begin<static_cast<unsigned long long>(protected_begin)+protected_size&&protected_begin<static_cast<unsigned long long>(begin)+size;};
 for(unsigned reg:{6u,7u})if(n){const auto stride=before.r[reg==6?8:9].d[0],begin=before.r[reg].d[0];const unsigned width=reg==6?48u:4u;if(stride>kRamSize||n-1>(kRamSize-width)/(stride?stride:1))throw std::runtime_error("actual output arithmetic bounds");const auto size=(n-1)*stride+width;if(begin>kRamSize||size>kRamSize-begin)throw std::runtime_error("actual output extent");for(auto zone:std::array<std::pair<unsigned,unsigned>,4>{{{object,928},{base,244},{attributes,60},{before.r[29].w[0],496}}})if(overlaps(begin,size,zone.first,zone.second))throw std::runtime_error("actual output/code/descriptor/frame alias unproved");}
 std::vector<unsigned char> initial(g_ram,g_ram+kRamSize),actual(kRamSize),reference_ram(kRamSize);actual_read("after-RAM",actual.data(),kRamSize);
 for(auto zone:std::array<std::pair<unsigned,unsigned>,3>{{{object,928},{base,244},{attributes,60}}})if(std::memcmp(initial.data()+zone.first,actual.data()+zone.first,zone.second))throw std::runtime_error("actual descriptor/code lifetime changed");
 Cpu reference=before;writes.clear();preempt_calls=0;yield_requested=false;reference_regenerated(reference);auto trace=writes;std::memcpy(reference_ram.data(),g_ram,kRamSize);
 if(preempt_calls||std::memcmp(actual.data(),g_ram,kRamSize)||reference.pc!=after.pc||reference.budget!=after.budget||std::memcmp(&reference.vu0,&after.vu0,sizeof after.vu0)||reference.fcr31!=after.fcr31)throw std::runtime_error("independent compiled original/actual call join");
 std::memcpy(g_ram,initial.data(),kRamSize);writes.clear();auto result=typed::no_yield::run(n?streams(before):typed::Streams{},p,before.budget,before.fcr31,{nullptr,&observe});
 if(mode=="--planted-order")std::swap(writes[0],writes[1]);
 if(mode=="--planted-transient")writes.push_back(writes[0]);
 Cpu projected=before;set32(projected.r[2],result.pipeline.accepted);projected.pc=before.r[31].w[0];projected.budget=result.remaining_budget;projected.fcr31=result.scalar_control_status;const auto&v=result.pipeline.vector;
 if(!result.pipeline.vector_effects_admitted)throw std::runtime_error("vector family not admitted");std::memcpy(&projected.vu0.vf[4],v.constant_product.data(),16);if(v.element_observed){std::memcpy(&projected.vu0.vf[2],v.last_resource256.data(),16);std::memcpy(&projected.vu0.vf[3],v.last_resource257.data(),16);}projected.vu0.mac=v.last_mac;projected.vu0.status=v.project_status(before.vu0.status);
 bool good=!std::memcmp(actual.data(),g_ram,kRamSize)&&trace==writes&&projected.pc==after.pc&&projected.budget==after.budget&&projected.fcr31==after.fcr31&&!std::memcmp(&projected.vu0,&after.vu0,sizeof after.vu0);
 for(unsigned reg=0;reg<32;++reg)if(reg!=1&&reg!=4&&reg!=5&&reg!=6&&reg!=7&&reg!=10)good&=!std::memcmp(&projected.r[reg],&after.r[reg],16);
 for(unsigned reg=11;reg<32;++reg)good&=projected.f[reg]==after.f[reg];good&=!std::memcmp(&projected.hi,&after.hi,16)&&!std::memcmp(&projected.lo,&after.lo,16)&&projected.sa==after.sa;
 if(!good)throw std::runtime_error("native shadow/actual original difference");
 std::printf("PASS_ACTUAL_ORIGINAL_RETURN_NATIVE_SHADOW count=%llu accepted=%u ordered_original_replay_writes=%zu full_RAM=1 full_VU=1 FCR_literal=1 budget=%d actual_thread=%d observed_actual_JALR_boundary=1 observed_whole_caller_entry=0 native_runtime_replacement=0\n",n,result.pipeline.accepted,writes.size(),after.budget,thread_before);return 0;
 }catch(const std::exception&e){std::fprintf(stderr,"OBSERVATION_DIFFERENCE %s\n",e.what());return 1;}}
'''


if __name__=='__main__':raise SystemExit(main())
