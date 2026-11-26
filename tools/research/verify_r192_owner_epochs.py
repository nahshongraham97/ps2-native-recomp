"""Actual snapshot membership/refusal plus constructed lifecycle gate controls.

Constructed hook sequences test the registry, not an observed producer call.
Connected fresh-boot source-label telemetry is a separate mandatory proof.
"""
from pathlib import Path
import argparse
import json
import shutil
import subprocess
import sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
import analyze_r192_builder_domain as auth

HOST=r'''
#include "@HEADER@"
#include <fstream>
#include <iostream>
namespace ps2 {alignas(64) std::uint8_t g_ram[kRamSize+64],g_spr[kScratchpadSize];}
using namespace ps2;using namespace mka::emitted::resource_owner_epochs;
static void w(unsigned a,unsigned v){std::memcpy(g_ram+a,&v,4);}
static unsigned r(unsigned a){unsigned v;std::memcpy(&v,g_ram+a,4);return v;}
static void need(bool x,const char*s){if(!x)throw std::runtime_error(s);}
struct Snapshot{const char*cpu;const char*ram;};const Snapshot snapshots[]={@SNAPSHOTS@};
static void read(const char*name,void*to,unsigned size){std::ifstream f(name,std::ios::binary);f.read(static_cast<char*>(to),size);need(bool(f)&&f.peek()==EOF,"snapshot byte extent");}
struct Fixture {unsigned cache,back,pool,table,desc,temp,out,list,frame,id;};
static Fixture fixture(unsigned n){return {0x1a00000+n*0x10000,0x100000+n*0x20000,0x101000+n*0x20000,0x101010+n*0x20000,0x101014+n*0x20000,0x1b00000+n*0x10000,0x1b01000+n*0x10000,0x1b02000+n*0x10000,0x1e00000+n*0x1000,0x123456+n};}
static void prepare(const Fixture&f){w(0x59fd90,0x12d870);w(0x59fd94,0x12d860);w(0x59fd98,0x12d830);w(0x59fd9c,0x210920);
 w(f.cache,0x59fd90);w(f.cache+4,f.id);w(f.cache+60,f.back);w(f.cache+64+524,1);w(f.cache+64+528,f.table);w(f.cache+64+636,f.pool);w(f.cache+64+536,999);
 w(f.table,f.desc);w(f.desc+72,f.cache+64);w(f.desc+76,f.pool);w(f.desc+916,2);w(f.temp+192,2);w(f.temp+80,0);w(f.temp+916,2);
 for(unsigned i=0;i<2;++i){w(f.temp+208+48*i,1);w(f.temp+216+48*i,769);}w(f.list,769);w(f.list+4,0);w(f.list+8,0);w(f.list+12,0xffffffff);
 for(unsigned i=0;i<4;++i){w(f.out+4*i,0x100+i);w(f.pool+4*i,0x100+i);}w(f.frame+1500,f.cache);w(f.frame+1424,1);w(f.frame+448,4);w(0x6540f0-21844,f.temp);
}
static void sequence(Registry&registry,const Fixture&f,bool omit_copy=false,unsigned allocation_bytes=0x10000,bool safe=false,unsigned mutate=0){Cpu c{};auto fire=[&](unsigned site){if(safe)registry.observe_hook(c,site,1);else registry.hook(c,site,1);};
 c.r[28].d[0]=0x6540f0;c.r[29].d[0]=f.frame+1504;fire(0x2fa9d0);
 c.r[29].d[0]=f.frame;c.r[4].d[0]=f.list;c.r[5].d[0]=f.temp;c.r[6].d[0]=f.out;c.r[7].d[0]=2000;fire(0x2ed3e0);
 c.r[29].d[0]=f.frame-128;c.r[18].d[0]=f.temp;c.r[2].d[0]=4;fire(0x2ed460);
 c.r[29].d[0]=f.frame;c.r[8].d[0]=0x2fee20;c.r[10].d[0]=0;fire(0x211c50);
 c.r[29].d[0]=f.frame-1296;c.r[21].d[0]=f.cache;c.r[19].d[0]=f.back;c.r[17].d[0]=allocation_bytes;fire(0x212200);
 c.r[29].d[0]=f.frame;fire(0x2faf20);if(mutate==3)w(f.pool+8,0xabcdef);fire(0x2faf4c);if(mutate==1)w(f.pool+8,0xabcdef);if(mutate==2)w(f.pool,0xabcdef);
 fire(0x2fafb0);if(!omit_copy)fire(0x2fb078);fire(0x2fb0d0);fire(0x2fb158);fire(0x2fb38c);fire(0x2fb3c4);
}
struct Reentry {Registry*registry{};Fixture f{};bool checked{};};
static void callback(void*opaque,unsigned cache){auto&x=*static_cast<Reentry*>(opaque);if(!x.registry||cache!=x.f.cache)return;auto v=x.registry->validate(cache,x.f.desc,x.f.pool,16);need(!v.admitted&&(v.reason=="OWNER_RETIRING"||v.reason=="FAILED_OR_IN_PROGRESS_RESTORE_POISON"),"cleanup callback observed live epoch");x.checked=true;}
int main(){try{
 unsigned observed=0;for(const auto&s:snapshots){Cpu c{};read(s.cpu,&c,sizeof c);read(s.ram,g_ram,kRamSize);const unsigned descriptor=c.r[19].w[0],owner=r(descriptor+72),cache=owner-64,code=r(descriptor+76);Registry epochs;
  const auto v=epochs.rehydrate(cache,descriptor,code,4);need(v.current_member&&!v.admitted&&v.generation&&v.reason=="UNPROVED_BACKING_ALLOCATION_EXTENT","actual snapshot must remain extent-unproved");
  need(!epochs.get_program(cache,descriptor)&&!epochs.has_owner(cache),"snapshot invented original producer Plan or fresh owner");std::cout<<"ACTUAL snapshot current_member=1 allocation_extent_unproved=1 cache="<<std::hex<<cache<<" count="<<r(owner+524)<<std::dec<<"\n";++observed;
 }
 std::memset(g_ram,0,kRamSize);const auto a=fixture(0),b=fixture(1);prepare(a);Reentry state{};state.f=a;Registry registry({&state,callback});state.registry=&registry;sequence(registry,a);auto v=registry.validate(a.cache,a.desc,a.pool,16);need(v.admitted,"constructed complete transaction refused");
 auto plan=registry.get_program(a.cache,a.desc);need(plan&&plan->input&&plan->input->original_attributes.size()==1&&plan->input->original_attributes[0].tag==769,"source sentinel Plan not retained independently of owner999");const auto generation=v.generation;
 need(registry.validate_region(a.cache,a.back,16,generation).admitted&&registry.validate_region(a.cache,a.back+0x10000-16,16,generation).admitted,"authenticated backing boundary or adjacent padding refused");
 need(!registry.validate_region(a.cache,a.back-1,4,generation).admitted&&!registry.validate_region(a.cache,a.back+0x10000-15,16,generation).admitted&&!registry.validate_region(a.cache,0xfffffff0,32,generation).admitted&&!registry.validate_region(a.cache,a.pool,16,generation+1).admitted&&!registry.validate_region(a.cache,a.pool,0,generation).admitted,"backing extent/generation/zero/overflow plant accepted");
 w(a.table,a.desc+4);need(!registry.validate(a.cache,a.desc,a.pool,16).admitted,"changed pointer table admitted");w(a.table,a.desc);
 w(a.desc+76,a.pool+4);need(!registry.validate(a.cache,a.desc,a.pool,16).admitted,"changed descriptor code pointer admitted");w(a.desc+76,a.pool);
 Cpu c{};c.r[4].d[0]=a.cache;registry.hook(c,0x210780,1);need(state.checked,"cleanup callback was not tested");need(!registry.validate(a.cache,a.desc,a.pool,16).admitted&&registry.validate(a.cache,a.desc,a.pool,16).generation!=generation,"old generation survived cleanup");
 need(plan->input->original_attributes[0].tag==769,"retained source Plan dangling after revoke");
 prepare(a);sequence(registry,a,true,0x10000,true);need(registry.has_owner(a.cache)&&!registry.validate(a.cache,a.desc,a.pool,16).admitted,"aborted descriptor copy admitted or disappeared into snapshot fallback");
 prepare(a);sequence(registry,a,false,4,true);need(registry.has_owner(a.cache)&&!registry.validate(a.cache,a.desc,a.pool,16).admitted,"undersized backing witness admitted");
 prepare(a);sequence(registry,a,false,0x10000,false,1);need(registry.validate(a.cache,a.desc,a.pool,16).admitted&&registry.get_program(a.cache,a.desc)->final_pool_preparation_required,"mutable final literal data incorrectly treated as immutable code");
 prepare(a);sequence(registry,a,false,0x10000,false,2);need(!registry.validate(a.cache,a.desc,a.pool,16).admitted,"changed immutable code accepted");
 prepare(a);sequence(registry,a,false,0x10000,true,3);need(!registry.validate(a.cache,a.desc,a.pool,16).admitted,"bad full copy origin accepted");
 prepare(a);sequence(registry,a);prepare(b);sequence(registry,b);c.r[4].d[0]=a.id;registry.hook(c,0x130ab0,1);need(registry.validate(a.cache,a.desc,a.pool,16).admitted&&registry.validate(b.cache,b.desc,b.pool,16).admitted&&registry.counters().last_requested_identity==a.id,"future identity setter was treated as existing owner death");
 const auto bad=fixture(2);prepare(bad);for(unsigned i=0;i<=20;++i){w(bad.list+12*i,769);w(bad.list+12*i+4,0);w(bad.list+12*i+8,0);}w(bad.list+12*21,0xffffffff);
 sequence(registry,bad,false,0x10000,true);need(registry.has_owner(bad.cache)&&!registry.get_program(bad.cache,bad.desc)&&registry.validate(a.cache,a.desc,a.pool,16).admitted&&registry.validate(b.cache,b.desc,b.pool,16).admitted&&registry.counters().refused&&registry.counters().quarantined&&!registry.last_refusal().empty(),"unsupported capture escaped quarantine or poisoned unrelated leases");
 registry.begin_restore();need(registry.poisoned()&&!registry.validate(b.cache,b.desc,b.pool,16).admitted,"restore failed to poison");registry.end_restore(false);need(registry.poisoned(),"failed restore cleared poison");registry.end_restore(true);need(!registry.poisoned()&&!registry.rehydrate(b.cache,b.desc,b.pool,16).admitted,"restore invented prior allocation extent");
 std::cout<<"PASS owner_epochs actual_snapshot_refusals="<<observed<<" constructed_complete_transaction=1 source_sentinel_not_owner_count=1 cleanup_reentry=1 descriptor_table_control=1 code_pointer_control=1 old_generation_control=1 aborted_copy_control=1 short_backing_control=1 identity_setter_telemetry_only=1 mutable_literal_control=1 immutable_code_control=1 full_copy_origin_control=1 affected_only_quarantine_control=1 backing_region_positive=2 backing_region_plants=5 restore_poison_controls=3 runtime_connected_fresh_trace=OPEN\n";return 0;
}catch(const std::exception&e){std::cerr<<"REFUSED "<<e.what()<<"\n";return 2;}}
'''


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('observations','runtime-build','compiler','output'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();out=a.output.resolve();auth.need(out.is_relative_to(auth.W/'build/oracle-telemetry') and not out.exists(),'fresh_owned_output_required')
    auth.need(shutil.disk_usage(out.parent).free>(12<<30)+(16<<20),'12GiB_reserve_required')
    receipt=a.observations/'RESULT.json';auth.need(auth.pin(receipt)['sha256']=='bc94ab802cc7e2108829d8f626d105868332fd54dc838a3829db10e9d90360da','actual_observations_changed')
    observed=json.loads(receipt.read_text());records={Path(p['path']).resolve():p for p in observed['observations']};inputs=[Path(__file__),Path(auth.__file__),receipt,a.compiler];rows=[]
    for i in (1,2,3):
        cpu=(a.observations/f'observations/case-{i}-segment-1-before-Cpu.bin').resolve();ram=cpu.with_name(cpu.name.replace('Cpu.bin','RAM.bin'))
        for q in (cpu,ram):auth.need(auth.pin(q)==records[q],'actual_snapshot_pin_changed');inputs.append(q)
        rows.append('{'+json.dumps(cpu.as_posix())+','+json.dumps(ram.as_posix())+'}')
    core=auth.W/'tools/research/r192_aot_modules';header=core/'reference_resource_owner_epochs.hpp'
    inputs += [header,core/'effective_resource_construction.hpp',core/'reference_resource_layout.hpp',core/'reference_resource_word_equality.hpp',core/'semantic_resource_pipeline.hpp',core/'semantic_vector_effects.hpp',*(a.runtime_build/'src/runtime/include/ps2').glob('*.hpp')]
    pins=[auth.pin(q) for q in dict.fromkeys(inputs)];out.mkdir(parents=True);cpp=out/'owner_epochs.cpp';cpp.write_text(HOST.replace('@HEADER@',header.as_posix()).replace('@SNAPSHOTS@',','.join(rows)),encoding='utf8',newline='\n');exe=out/'owner_epochs.exe'
    command=[str(a.compiler),'-std=c++20','-O1','-mavx2','-ffp-contract=off','-static-libgcc','-static-libstdc++','-I'+str((a.runtime_build/'src/runtime/include').resolve()),str(cpp),'-o',str(exe)]
    cp=subprocess.run(command,capture_output=True,timeout=120);(out/'compile.log').write_bytes(cp.stdout+cp.stderr);auth.need(cp.returncode==0,'owner_epoch_test_compile_failed')
    run=subprocess.run([str(exe)],capture_output=True,timeout=120);(out/'run.log').write_bytes(run.stdout+run.stderr);changes=[p['path'] for p in pins if auth.pin(Path(p['path']))!=p]
    result=dict(status='PASS_SNAPSHOT_REFUSALS_AND_CONSTRUCTED_EPOCH_CONTROLS' if run.returncode==0 and not changes else 'FAIL',source_pins=pins,input_changes=changes,compile_argv=command,run_exit=run.returncode,stdout=run.stdout.decode(errors='replace'),stderr=run.stderr.decode(errors='replace'),artifacts=[auth.pin(q) for q in (cpp,exe)],scope='Three authentic snapshots: current membership only, backing extent unproved and refused. Constructed lifecycle gate controls are not an actual producer invocation. Actual hooked fresh boot remains required.',command=[sys.executable,str(Path(__file__).resolve()),*sys.argv[1:]])
    target=out/'RESULT.json';target.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8',newline='\n');print(json.dumps(dict(status=result['status'],receipt=auth.pin(target))));return 0 if result['status'].startswith('PASS') else 2


if __name__=='__main__':raise SystemExit(main())
