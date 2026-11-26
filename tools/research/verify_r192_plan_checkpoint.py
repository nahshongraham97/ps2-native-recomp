"""DPAPI/state pairing tests over an actual original-builder program.

Uses constructed lifecycle events and a source-faithful v8 RAM prefix. It does
not claim full machine_state/device parsing or a real game save/load roundtrip.
No full RAM/state artifact is written; only small paired metadata is persisted.
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
#include "@CHECKPOINT_CPP@"
namespace ck=mka::emitted::resource_plan_checkpoint;
namespace ep=mka::emitted::resource_owner_epochs;
static ep::Registry epochs;
static unsigned ee_begin{},ee_success{},ee_failure{},iop_begin{},iop_success{},iop_failure{};
static bool fail_ee_begin{},fail_iop_begin{};
extern "C" ep::Registry&r192_family_registry(){return epochs;}
extern "C" void r192_family_restore_begin(){if(fail_ee_begin)throw std::runtime_error("planted EE begin failure");++ee_begin;epochs.begin_restore();}
extern "C" void r192_family_restore_end(bool okay){if(okay)++ee_success;else++ee_failure;epochs.end_restore(okay);}
extern "C" const char*r192_family_checkpoint_source_id(){return "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";}
extern "C" const char*r192_family_checkpoint_owned_root(){return "@OUT@";}
namespace ps2::iop::kernel {void r192_fresh_before_machine_restore(){if(fail_iop_begin)throw std::runtime_error("planted IOP begin failure");++iop_begin;}void r192_fresh_after_machine_restore(bool okay){if(okay)++iop_success;else++iop_failure;}}
static void must(bool value,const char*s){if(!value)throw std::runtime_error(s);}
static void put(unsigned at,unsigned value){std::memcpy(g_ram+at,&value,4);}
static constexpr unsigned cache=0x1a00000,backing=0x1a10000,pool=backing+0x1000,temporary=0x1b00000,output=0x1c00000,list=0x1c30000,frame=0x1e00000;
static unsigned descriptor,words,poolwords;
static void source_record(){
 const RawFilter filter{2,769,0,0x3f800000};const Test t{"checkpoint_actual_original",1,{filter},1,FilterBreak::none};const auto program=build(t,t.raw,1);
 words=program.words;poolwords=program.pool;const unsigned table=pool+4*((words+3)&~3u);descriptor=table+4;
 std::memcpy(g_ram+temporary,program.descriptor.data(),928);std::memcpy(g_ram+descriptor,program.descriptor.data(),928);std::memcpy(g_ram+output,program.output.data(),4*words);std::memcpy(g_ram+pool,program.output.data(),4*words);
 put(cache,0x59fd90);put(cache+4,0x7654321);put(cache+60,backing);put(cache+64+524,1);put(cache+64+528,table);put(cache+64+636,pool);put(cache+64+536,999);put(table,descriptor);put(descriptor+72,cache+64);put(descriptor+76,pool);
 put(frame+1424,1);put(frame+448,(words+3)&~3u);put(frame+1500,cache);put(0x6540f0-21844,temporary);
 Cpu c{};c.r[28].d[0]=0x6540f0;c.r[29].d[0]=frame+1504;epochs.hook(c,0x2fa9d0,1);c.r[29].d[0]=frame;c.r[4].d[0]=list;c.r[5].d[0]=temporary;c.r[6].d[0]=output;c.r[7].d[0]=2000;epochs.hook(c,0x2ed3e0,1);
 c.r[29].d[0]=frame-128;c.r[18].d[0]=temporary;c.r[2].d[0]=words;epochs.hook(c,0x2ed460,1);c.r[29].d[0]=frame;c.r[8].d[0]=0x2fee20;c.r[10].d[0]=0;epochs.hook(c,0x211c50,1);
 c.r[29].d[0]=frame-1296;c.r[21].d[0]=cache;c.r[19].d[0]=backing;c.r[17].d[0]=0x10000;epochs.hook(c,0x212200,1);c.r[29].d[0]=frame;
 for(unsigned pc:{0x2faf20u,0x2faf4cu,0x2fafb0u,0x2fb078u,0x2fb0d0u,0x2fb158u,0x2fb38cu,0x2fb3c4u})epochs.hook(c,pc,1);
 must(epochs.validate(cache,descriptor,pool,4*words).admitted,"actual program constructed source transaction refused");
}
static ck::Bytes state_prefix(){constexpr unsigned offset=20+sizeof(Cpu);ck::Bytes state(offset+kRamSize+8);std::memcpy(state.data(),"PS2STATE",8);state[8]=8;std::memcpy(state.data()+12,"ee",2);std::memcpy(state.data()+offset,g_ram,kRamSize);std::memcpy(state.data()+offset+kRamSize,"end",3);return state;}
static void metadata(const std::string&name,std::span<const std::uint8_t>bytes){std::ofstream f(std::filesystem::u8path(name+".native-plans"),std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));must(bool(f),"metadata fixture write");}
int main(){try{
 source_record();const auto old=epochs.get_program(cache,descriptor)->generation;auto state=state_prefix();const auto legacy_payload=state;const std::string good="@OUT@/paired.state";ck::before_save(good);ck::prepare_save(good,state);
 must(!std::filesystem::exists(good)&&std::equal(legacy_payload.begin(),legacy_payload.end(),ck::payload_for_restore(state).begin()),"save preflight wrote a state or changed original v8 payload");
 const auto sealed=ck::pending_save->sealed,plain=ck::protect(sealed,false);const auto prepared=ck::decode(plain,state);metadata(good,sealed);
 {ck::PathLoad pair(good,state);ck::begin_buffer_restore(state);epochs.reset();must(epochs.poisoned(),"neutral kernel reset cleared restore poison");ck::end_buffer_restore(true);}
 const auto restored=epochs.get_program(cache,descriptor);must(restored&&restored->generation!=old&&restored->restored_paired_witness&&restored->input->original_attributes.size()==2,"paired Plan did not import with new epoch/source attributes");
 unsigned plants=0;
 auto reject=[&](const std::string&name,const ck::Bytes&bytes,std::span<const std::uint8_t>candidate){const auto before=ee_begin;metadata(name,bytes);bool refused=false;try{ck::PathLoad bad(name,candidate);}catch(const std::exception&){refused=true;}must(refused&&ee_begin==before&&epochs.get_program(cache,descriptor),"bad pair entered progressive restore or changed current Plan");++plants;};
 auto corrupt=sealed;corrupt[corrupt.size()/2]^=1;reject("@OUT@/bad-seal.state",corrupt,state);
 auto wrongstate=state;wrongstate[20+sizeof(Cpu)+0x100]^=1;reject("@OUT@/wrong-state.state",sealed,wrongstate);
 auto malformed=prepared;malformed.owners[0].bytes=4;reject("@OUT@/short-extent.state",ck::protect(ck::encode(malformed),true),state);
 malformed=prepared;malformed.owners[0].programs[0].descriptor+=4;reject("@OUT@/wrong-member.state",ck::protect(ck::encode(malformed),true),state);
 auto changed=state;changed[20+sizeof(Cpu)+pool]^=1;auto mismatched=prepared;mismatched.state_hash=ck::hash(changed);reject("@OUT@/changed-code.state",ck::protect(ck::encode(mismatched),true),changed);
 auto badschema=plain;const unsigned schema=4+unsigned(std::strlen("MKA_SOURCE_PLANS"));badschema[schema]^=1;auto resign=[&](ck::Bytes&raw){auto sum=ck::hash(std::span<const std::uint8_t>(raw).first(raw.size()-32));std::copy(sum.begin(),sum.end(),raw.end()-32);};resign(badschema);reject("@OUT@/bad-schema.state",ck::protect(badschema,true),state);
 auto badabi=plain;badabi[schema+4]^=1;resign(badabi);reject("@OUT@/bad-abi.state",ck::protect(badabi,true),state);
 auto fingerprint=plain;const auto abi_at=std::search(fingerprint.begin(),fingerprint.end(),std::begin(ck::abi),std::end(ck::abi)-1);must(abi_at!=fingerprint.end(),"fixed ABI fingerprint seam missing");*abi_at^=1;resign(fingerprint);reject("@OUT@/bad-fingerprint.state",ck::protect(fingerprint,true),state);
 auto trailing=plain;trailing.insert(trailing.end()-32,0x42);resign(trailing);reject("@OUT@/trailing.state",ck::protect(trailing,true),state);
 auto truncated=sealed;truncated.resize(truncated.size()/2);reject("@OUT@/truncated.state",truncated,state);
 auto wrong_envelope=state;wrong_envelope[wrong_envelope.size()-16]^=1;reject("@OUT@/wrong-envelope.state",sealed,wrong_envelope);
 const auto before_missing=ee_begin;bool missing_refused=false;try{ck::PathLoad missing("@OUT@/required-missing.state",state);}catch(const std::exception&){missing_refused=true;}must(missing_refused&&ee_begin==before_missing,"missing new required pair reached parser");++plants;
 bool byte_refused=false;try{ck::begin_buffer_restore(state);}catch(const std::exception&){byte_refused=true;}must(byte_refused&&ee_begin==before_missing,"byte-only required pair reached parser");++plants;
 {ck::PathLoad pair(good,state);ck::begin_buffer_restore(state);ck::end_buffer_restore(false);must(epochs.poisoned()&&!epochs.get_program(cache,descriptor),"failed progressive parse imported Plans or cleared poison");epochs.reset();must(epochs.poisoned(),"neutral reset cleared failed-restore poison");}
 {ck::PathLoad pair(good,state);ck::begin_buffer_restore(state);ck::end_buffer_restore(true);must(epochs.get_program(cache,descriptor)!=nullptr,"retry full success failed");}
 {ck::PathLoad legacy("@OUT@/legacy.state",legacy_payload);ck::begin_buffer_restore(legacy_payload);ck::end_buffer_restore(true);must(!epochs.get_program(cache,descriptor)&&!epochs.has_owner(cache),"legacy state invented Plan history");}
 const std::string protected_name="@OUT@/preexisting.state";{std::ofstream f(protected_name);f<<"preserve";}bool protected_refused=false;try{ck::before_save(protected_name);}catch(const std::exception&){protected_refused=true;}must(protected_refused&&std::filesystem::file_size(protected_name)==8,"preexisting save was not protected");
 must(ee_begin==iop_begin&&ee_success==iop_success&&ee_failure==iop_failure&&ee_failure==1,"outer EE/IOP restore outcome coordination");
 const auto old_ee=ee_begin,old_iop=iop_begin,old_ee_fail=ee_failure,old_iop_fail=iop_failure;fail_ee_begin=true;bool refused=false;try{ck::begin_buffer_restore(legacy_payload);}catch(const std::exception&){refused=true;}fail_ee_begin=false;
 must(refused&&ee_begin==old_ee&&iop_begin==old_iop&&ee_failure==old_ee_fail&&iop_failure==old_iop_fail,"failed EE begin caused unpaired end");
 fail_iop_begin=true;refused=false;try{ck::begin_buffer_restore(legacy_payload);}catch(const std::exception&){refused=true;}fail_iop_begin=false;
 must(refused&&ee_begin==old_ee+1&&iop_begin==old_iop&&ee_failure==old_ee_fail+1&&iop_failure==old_iop_fail&&epochs.poisoned(),"failed IOP begin caused unpaired IOP end or missed EE poison");
 epochs.end_restore(true);auto bad_import=prepared.owners;bad_import[0].programs[0].code+=4;refused=false;try{epochs.import_paired_checkpoint(bad_import);}catch(const std::exception&){refused=true;}
 must(refused&&epochs.poisoned()&&!epochs.has_owner(cache),"invalid direct paired import did not poison atomically");
 std::cout<<"PASS paired_source_plans actual_original_program_words="<<words<<" original_attributes=2 owner_attribute_count=999 DPAPI_roundtrip=1 new_epoch=1 preflight_plants="<<plants<<" real_save_preparation_no_file_write=1 unchanged_original_payload=1 envelope_bytes=32 required_missing_pair_refusal=1 byte_only_required_pair_refusal=1 failed_progressive_poison=1 restore_begin_failure_controls=2 atomic_bad_import_poison=1 legacy_nohistory=1 preexisting_save_refusal=1 full_machine_parser=MOCK_ONLY active_context_serialization=UNSUPPORTED\n";return 0;
}catch(const std::exception&e){std::cerr<<"REFUSED "<<e.what()<<"\n";return 2;}}
'''


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('construction-proof','runtime-build','compiler','output'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();out=a.output.resolve();auth.need(out.is_relative_to(auth.W/'build/oracle-telemetry') and not out.exists(),'fresh_owned_output_required');auth.need(shutil.disk_usage(out.parent).free>(12<<30)+(16<<20),'12GiB_reserve_required')
    receipt=a.construction_proof/'RESULT.json';auth.need(auth.pin(receipt)['sha256']=='b4f2c4a664c29e28b9916072e834d59cd6261bc4322ee6c0c2c1f6184627bf36','actual_original_constructor_proof_changed')
    record=json.loads(receipt.read_text());source=a.construction_proof/'effective_construction.cpp';auth.need(any(auth.pin(source)==r for r in record['artifacts']),'original_constructed_reference_changed');original=source.read_text();auth.need(original.count('int main(')==1,'original_main_changed');prefix=original.split('int main(',1)[0]
    core=auth.W/'tools/research/r192_aot_modules';implementation=core/'reference_resource_plan_checkpoint.cpp';inputs=[Path(__file__),Path(auth.__file__),receipt,source,a.compiler,implementation,*core.glob('reference_resource_*checkpoint.hpp'),core/'reference_resource_owner_epochs.hpp',core/'effective_resource_construction.hpp',core/'reference_resource_layout.hpp',core/'reference_resource_word_equality.hpp',core/'semantic_resource_pipeline.hpp',core/'semantic_vector_effects.hpp',*[Path(r['path']) for r in record['source_pins'] if Path(r['path']).suffix in ('.bin','.cpp','.hpp')],*(a.runtime_build/'src/runtime/include/ps2').glob('*.hpp')]
    pins=[auth.pin(q) for q in dict.fromkeys(inputs)];out.mkdir(parents=True);cpp=out/'plan_checkpoint.cpp';cpp.write_text(prefix+HOST.replace('@CHECKPOINT_CPP@',implementation.as_posix()).replace('@OUT@',out.as_posix()),encoding='utf8',newline='\n');exe=out/'plan_checkpoint.exe'
    command=[str(a.compiler),'-std=c++20','-O1','-mavx2','-ffp-contract=off','-static-libgcc','-static-libstdc++','-I'+str((a.runtime_build/'src/runtime/include').resolve()),str(cpp),'-o',str(exe),'-lcrypt32','-lbcrypt']
    cp=subprocess.run(command,capture_output=True,timeout=120);(out/'compile.log').write_bytes(cp.stdout+cp.stderr);auth.need(cp.returncode==0,'checkpoint_test_compile_failed')
    run=subprocess.run([str(exe)],capture_output=True,timeout=120);(out/'run.log').write_bytes(run.stdout+run.stderr);changes=[r['path'] for r in pins if auth.pin(Path(r['path']))!=r]
    result=dict(status='PASS_PAIRED_PLAN_COMPONENTS' if run.returncode==0 and not changes else 'FAIL',source_pins=pins,input_changes=changes,compile_argv=command,run_exit=run.returncode,stdout=run.stdout.decode(errors='replace'),stderr=run.stderr.decode(errors='replace'),artifacts=[auth.pin(q) for q in out.iterdir() if q.is_file()],scope='Actual original-builder words plus CONSTRUCTED hook/state RAM prefix and mock full-parser outcomes. DPAPI user-scope seal does not prove origin against arbitrary same-user code. No game save/load or active continuation persistence claim.',invocation=[sys.executable,str(Path(__file__).resolve()),*sys.argv[1:]])
    target=out/'RESULT.json';target.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8',newline='\n');print(json.dumps(dict(status=result['status'],receipt=auth.pin(target))));return 0 if result['status'].startswith('PASS') else 2


if __name__=='__main__':raise SystemExit(main())
