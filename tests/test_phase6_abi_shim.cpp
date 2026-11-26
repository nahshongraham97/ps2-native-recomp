#include "mka/stage_a_abi_shim.hpp"
#include "stage_a_leaf.hpp"
#include "original_abi_reference.hpp"
#include <array>
#include <cstdio>
#include <string>
namespace abi=mka::stage_a::abi;
namespace {
std::string plant;unsigned comparisons{},refusals{},reads{};
void check(bool value,const char*reason){if(!value){std::fprintf(stderr,"FAIL_ABI_SHIM case=%u reason=%s\n",comparisons,reason);throw std::runtime_error(reason);}}
ps2::Cpu fixture(std::uint64_t seed){
    ps2::Cpu c;auto*p=reinterpret_cast<unsigned char*>(&c);
    for(std::size_t i=0;i<sizeof c;++i){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;p[i]=static_cast<unsigned char>(seed);}
    std::memset(&c.r[0],0,sizeof c.r[0]);c.budget=1000;return c;
}
struct Observation {std::uint32_t address{},link{};unsigned calls{};};
void corruption(ps2::Cpu&c){
    if(comparisons)return;
    if(plant=="--planted-width")c.r[4].d[0]&=0xffffffffu;
    if(plant=="--planted-stale-ssa")c.r[2].d[0]=c.r[3].d[0];
    if(plant=="--planted-high-lane")c.r[2].d[1]^=1;
    if(plant=="--planted-return")c.pc^=4;
    if(plant=="--planted-frame")c.r[29].d[0]^=16;
}
template<class F>void refusal(const char*reason,F&&f){
    try{f();}catch(const std::runtime_error&e){check(std::string(e.what())==reason,"structural refusal reason");++refusals;return;}
    check(false,"structural refusal missing");
}
void one_call(abi::HeapCloseCall site,std::uint32_t loaded,std::uint64_t seed){
    auto before=fixture(seed);const auto words=abi::source(site);before.pc=words.pc;
    before.r[words.delay_base].w[0]=0x1020u;const auto expected_address=0x102cu;
    auto original=abi::copy_cpu(before),native=abi::copy_cpu(before),aot=abi::copy_cpu(before);
    Observation source_read,native_read,aot_read;
    auto reader=[&](Observation&ob){return [&,obp=&ob](std::uint32_t address,const ps2::Cpu&c)->std::uint32_t{
        obp->address=address;obp->link=c.r[31].w[0];++obp->calls;++reads;return loaded;};};
    auto source_reader=reader(source_read);original_abi_reference::call(original,words.pc,source_reader);
    auto native_reader=[&](std::uint32_t address,const ps2::Cpu&c)->std::uint32_t{
        native_read.address=address;native_read.link=c.r[31].w[0];++native_read.calls;++reads;
        return !comparisons&&plant=="--planted-invented-input" ? loaded^4u : loaded;
    };
    auto native_body=[&](abi::ResourceHeapReference heap)->std::int32_t{
        check(heap.address==loaded,"typed input projection");
        return stage_a::close_resource_heap({heap.address});
    };
    if(!comparisons&&plant=="--planted-delay")original_abi_reference::no_delay_call(native,words.pc,native_reader);
    else if(!comparisons&&plant=="--planted-reordered-delay")original_abi_reference::reordered_call(native,words.pc,native_reader);
    else abi::native_call(native,site,native_reader,native_body);
    auto aot_reader=reader(aot_read);
    abi::aot_call(aot,site,aot_reader,[](ps2::Cpu&c){original_abi_reference::leaf(c);});
    corruption(native);
    check(source_read.calls==1&&source_read.address==expected_address&&source_read.link==words.pc+8,"original JAL delay source");
    check(native_read.calls==source_read.calls&&native_read.address==source_read.address&&native_read.link==source_read.link,"delay ordering/address/count");
    check(aot_read.calls==source_read.calls&&aot_read.address==source_read.address&&aot_read.link==source_read.link,"AOT delay ordering/address/count");
    check(abi::same_cpu(original,native),"native full Cpu byte projection");
    check(abi::same_cpu(original,aot),"AOT full Cpu byte projection");++comparisons;
}
void one_entry(std::uint32_t target,std::uint64_t seed){
    auto before=fixture(seed);before.pc=abi::heap_close_entry;before.r[31].w[0]=target;
    auto original=abi::copy_cpu(before),native=abi::copy_cpu(before),aot=abi::copy_cpu(before);
    original_abi_reference::leaf(original);
    abi::native_entry(native,[](abi::ResourceHeapReference h){return stage_a::close_resource_heap({h.address});});
    abi::aot_entry(aot,[](ps2::Cpu&c){original_abi_reference::leaf(c);});
    check(abi::same_cpu(original,native)&&abi::same_cpu(original,aot),"entry full Cpu byte preservation");++comparisons;
}
}
int main(int argc,char**argv){try{
    if(argc>2)return 2;if(argc==2)plant=argv[1];
    const std::array<std::uint32_t,8> loaded{0x80000000u,0u,1u,0x7fffffffu,0xffffffffu,0xdeadbeefu,0x80000001u,0x12345678u};
    for(auto site:{abi::HeapCloseCall::arena_001a2068,abi::HeapCloseCall::resource_001a1bb4})
        for(auto value:loaded)for(unsigned i=0;i<8;++i)one_call(site,value,0xdeadbeef00001000ull+i);
    for(auto target:{0u,4u,0x00100000u,0xfffffffcu})for(unsigned i=0;i<8;++i)one_entry(target,0x8910000000010000ull+i);
    auto c=fixture(0x12345678);c.pc=abi::heap_close_entry;c.r[31].w[0]=4;
    refusal("ABI_SHIM_REFUSED:AOT_full_Cpu_contract_mismatch",[&]{auto d=abi::copy_cpu(c);abi::aot_entry(d,[](ps2::Cpu&v){original_abi_reference::leaf(v);v.r[31].d[1]^=1;});});
    refusal("ABI_SHIM_REFUSED:native_callback_changed_source_Cpu",[&]{auto d=abi::copy_cpu(c);abi::native_entry(d,[&](abi::ResourceHeapReference)->std::int32_t{d.fcr31^=1;return 1;});});
    refusal("ABI_SHIM_REFUSED:constant_return_body_mismatch",[&]{auto d=abi::copy_cpu(c);abi::native_entry(d,[](abi::ResourceHeapReference)->std::int32_t{return 0;});});
    refusal("ABI_SHIM_REFUSED:misaligned_return_target",[&]{auto d=abi::copy_cpu(c);d.r[31].w[0]=3;abi::native_entry(d,[](abi::ResourceHeapReference)->std::int32_t{return 1;});});
    refusal("ABI_SHIM_REFUSED:wrong_source_entry",[&]{auto d=abi::copy_cpu(c);d.pc^=4;abi::native_entry(d,[](abi::ResourceHeapReference)->std::int32_t{return 1;});});
    refusal("ABI_SHIM_REFUSED:insufficient_explicit_host_allowance",[&]{auto d=abi::copy_cpu(c);d.budget=1;abi::native_entry(d,[](abi::ResourceHeapReference)->std::int32_t{return 1;});});
    refusal("ABI_SHIM_REFUSED:unproved_call_site",[]{(void)abi::source(static_cast<abi::HeapCloseCall>(42));});
    std::printf("PASS_ABI_SHIM constructed=%u exact_call_bundles=128 entry_boundaries=32 read_events=%u structural_refusals=%u sizeof_Cpu=%zu native_credit=0\n",comparisons,reads,refusals,sizeof(ps2::Cpu));return 0;
}catch(const std::exception&e){std::fprintf(stderr,"REFUSED_ABI_SHIM:%s\n",e.what());return 1;}}
