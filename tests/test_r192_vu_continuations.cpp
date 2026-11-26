// Test-only: link the pinned v8 VU reference, never a game target.
#include "ps2/vu_ops.hpp"
#include "ps2/system.hpp"
#include "ps2/vu_live.hpp"
#include "ps2/observer_hook.hpp"
#include "gs/queue.hpp"
#include "vu_jit_web.hpp"
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

namespace ps2 {
alignas(64) std::uint8_t g_vu0_code[4096], g_vu0_mem[4096], g_vu1_code[16384], g_vu1_mem[16384];
Cpu g_cpu;
Options g_options;
std::uint64_t frame_count() { return 0; }
bool halted() { return false; }
void halt(const std::string& reason) { throw std::runtime_error(reason); }
namespace profile {
bool profiler_enabled() { throw std::runtime_error("unexpected compiled dispatch"); }
void record_vu_compiled(std::uint64_t) { throw std::runtime_error("unexpected compiled dispatch"); }
void record_vu_interpret(std::uint64_t) { throw std::runtime_error("unexpected compiled dispatch"); }
}
namespace vif { std::uint32_t itop(int) { return 0; } }
namespace gif { void transfer(int, const std::uint8_t*, std::uint32_t) {} }
namespace gs::queue {
Ring& vu() { static Ring ring; return ring; }
Role& role() { static Role r=Role::Emulation; return r; }
bool vu_active() { return false; }
void Ring::post(Type, std::uint32_t, std::uint32_t, const void*, std::uint32_t) {}
}
namespace vu {
namespace jit {
bool g_dry=false;
std::vector<std::uint32_t>* g_kicks=nullptr;
std::int64_t g_escape=0;
ProgramFn compile(const std::uint8_t*,std::uint32_t) { throw std::runtime_error("unexpected JIT"); }
int mode() { throw std::runtime_error("unexpected JIT"); }
}
namespace live {
std::atomic<bool> g_ready{false};
std::vector<Program> take() { throw std::runtime_error("unexpected Live"); }
}
void run_fast(int, std::uint32_t, std::uint64_t);
Vu0Regs& regs(int);
void code_changed(int);
}
}

static void pair(unsigned pc, unsigned lo=0x8000033c, unsigned hi=0x000002ff) {
    std::memcpy(ps2::g_vu1_code+pc,&lo,4);
    std::memcpy(ps2::g_vu1_code+pc+4,&hi,4);
}

int main(int argc,char** argv) {
    const std::string plant=argc==2?argv[1]:"";
    if(argc>2 || (!plant.empty() && plant!="--plant-one-pair" && plant!="--plant-dropped-delay" && plant!="--plant-branch-fallthrough")) return 2;
    unsigned failures=0,checks=0;
    auto check=[&](const char* name,unsigned entry,unsigned expected,unsigned expected_vi2) {
        ps2::vu::code_changed(1);
        ps2::vu::run_fast(1,entry,0);
        auto observed=ps2::vu::regs(1);
        if(plant=="--plant-one-pair" && std::strcmp(name,"plain")==0) observed.tpc=8;
        if(plant=="--plant-dropped-delay" && std::strcmp(name,"plain")==0) observed.vi[2]=0;
        if(plant=="--plant-branch-fallthrough" && std::strcmp(name,"branch_E")==0) observed.tpc=16;
        const bool ok=observed.tpc==expected && observed.vi[2]==expected_vi2;
        std::printf("%s %s TPC=%u expected=%u VI2=%u expected=%u\n",ok?"PASS":"FAIL",name,observed.tpc,expected,observed.vi[2],expected_vi2);
        ++checks;failures+=!ok;
    };
    auto reset=[] {
        std::memset(ps2::g_vu1_code,0,sizeof ps2::g_vu1_code);
        std::memset(ps2::g_vu1_mem,0,sizeof ps2::g_vu1_mem);
        auto& r=ps2::vu::regs(1);r={};r.vf[0].w[3]=0x3f800000;
    };
    reset();pair(0,0x8000033c,0x400002ff);pair(8,0x10021001);
    pair(16,0x10021009,0x400002ff);pair(24);
    check("plain",0,16,1);check("resume",16,32,10);
    reset();pair(0,0x40000003,0xc00002ff);pair(8);check("I_bit_lower_is_immediate",0,16,0);
    reset();pair(16368,0x8000033c,0x400002ff);pair(16376);check("wrap",16368,0,0);
    reset();pair(0,0x40000003,0x400002ff);pair(8,0x10021001);check("branch_E",0,32,1);
    reset();pair(0,0x40000003);pair(8,0x8000033c,0x400002ff);pair(32,0x10021001);
    check("E_in_branch_delay",0,40,1);
    reset();ps2::vu::regs(1).vi[1]=4;pair(0,0x48000800,0x400002ff);pair(8);
    check("JR_E",0,32,0);
    reset();pair(0,0x8000033c,0x400002ff);pair(8,0x40000003);
    check("branch_in_E_delay",0,16,0);
    std::printf("checks=%u failures=%u\n",checks,failures);
    return failures?1:0;
}
