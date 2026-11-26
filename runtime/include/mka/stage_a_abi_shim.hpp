#pragma once
// Bounded Phase6 Stage A test boundary. No global Cpu swap, native owner or
// ordinary-call convention is inferred for any other retail function.
#include "ps2/cpu.hpp"
#include <cstring>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace mka::stage_a::abi {
struct ResourceHeapReference { std::uint32_t address; }; // unused by this leaf
enum class HeapCloseCall { arena_001a2068, resource_001a1bb4 };
inline constexpr std::uint32_t heap_close_entry = 0x001a3c40;
struct CallWords { std::uint32_t pc, jal, delay; unsigned delay_base; };
inline constexpr CallWords source(HeapCloseCall call) {
    switch(call) {
    case HeapCloseCall::arena_001a2068: return {0x001a2068,0x0c068f10,0x8e24000c,17};
    case HeapCloseCall::resource_001a1bb4: return {0x001a1bb4,0x0c068f10,0x8c44000c,2};
    }
    throw std::runtime_error("ABI_SHIM_REFUSED:unproved_call_site");
}
inline void require(bool value,const char*why) {if(!value)throw std::runtime_error(why);}
inline ps2::Cpu copy_cpu(const ps2::Cpu& from) {
    static_assert(std::is_trivially_copyable_v<ps2::Cpu>);
    ps2::Cpu result;std::memcpy(&result,&from,sizeof result);return result;
}
inline bool same_cpu(const ps2::Cpu&a,const ps2::Cpu&b) {return std::memcmp(&a,&b,sizeof a)==0;}
inline std::uint64_t sign_extend32(std::uint32_t value) {
    return value&0x80000000u ? (0xffffffff00000000ull|value) : value;
}
inline void entry_preconditions(const ps2::Cpu&cpu) {
    require(cpu.pc==heap_close_entry,"ABI_SHIM_REFUSED:wrong_source_entry");
    require(!(cpu.r[31].w[0]&3u),"ABI_SHIM_REFUSED:misaligned_return_target");
    require(cpu.budget>=2,"ABI_SHIM_REFUSED:insufficient_explicit_host_allowance");
}
inline ps2::Cpu expected_leaf(const ps2::Cpu&before) {
    auto result=copy_cpu(before);
    // JR captures low32 RA BEFORE its actual ADDIU V0,zero,1 delay effect.
    const auto destination=before.r[31].w[0];
    result.r[2].d[0]=sign_extend32(1); // original signed32 result; upper64 retained
    result.pc=destination;result.budget-=2;return result;
}
template<class Native>
void native_entry(ps2::Cpu&cpu,Native&&native) {
    entry_preconditions(cpu);const auto before=copy_cpu(cpu);
    const auto result=std::forward<Native>(native)(ResourceHeapReference{before.r[4].w[0]});
    static_assert(std::is_same_v<std::remove_cv_t<decltype(result)>,std::int32_t>);
    require(same_cpu(cpu,before),"ABI_SHIM_REFUSED:native_callback_changed_source_Cpu");
    require(result==1,"ABI_SHIM_REFUSED:constant_return_body_mismatch");
    const auto projected=expected_leaf(before);std::memcpy(&cpu,&projected,sizeof cpu);
}
template<class Aot>
void aot_entry(ps2::Cpu&cpu,Aot&&aot) {
    entry_preconditions(cpu);const auto expected=expected_leaf(cpu);
    std::forward<Aot>(aot)(cpu);
    // Validate, do NOT repair/restore unexpected AOT mutations.
    require(same_cpu(cpu,expected),"ABI_SHIM_REFUSED:AOT_full_Cpu_contract_mismatch");
}
template<class Read32>
void original_call_prefix(ps2::Cpu&cpu,HeapCloseCall site,Read32&&read32) {
    const auto words=source(site);
    require(cpu.pc==words.pc,"ABI_SHIM_REFUSED:wrong_call_PC");
    require(cpu.budget>=4,"ABI_SHIM_REFUSED:insufficient_explicit_host_allowance");
    cpu.budget-=2;
    cpu.r[31].d[0]=sign_extend32(words.pc+8); // JAL preserves bits127:64
    const auto after_link=copy_cpu(cpu);
    // Typed StageA memory belongs to the caller. No memory implementation or
    // assumed native pointer/heap owner exists in this transition header.
    const std::uint32_t address=cpu.r[words.delay_base].w[0]+12u;
    const auto value=std::forward<Read32>(read32)(address,std::as_const(cpu));
    static_assert(std::is_same_v<std::remove_cv_t<decltype(value)>,std::uint32_t>);
    require(same_cpu(cpu,after_link),"ABI_SHIM_REFUSED:delay_reader_changed_source_Cpu");
    cpu.r[4].d[0]=sign_extend32(value); // original LW; preserve bits127:64
    cpu.pc=heap_close_entry;
}
template<class Read32,class Native>
void native_call(ps2::Cpu&cpu,HeapCloseCall site,Read32&&read32,Native&&native) {
    original_call_prefix(cpu,site,std::forward<Read32>(read32));
    native_entry(cpu,std::forward<Native>(native));
}
template<class Read32,class Aot>
void aot_call(ps2::Cpu&cpu,HeapCloseCall site,Read32&&read32,Aot&&aot) {
    original_call_prefix(cpu,site,std::forward<Read32>(read32));
    aot_entry(cpu,std::forward<Aot>(aot));
}
} // namespace mka::stage_a::abi
