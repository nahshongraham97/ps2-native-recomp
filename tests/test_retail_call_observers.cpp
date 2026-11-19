// Full-state non-interference fixture for transitional retail call observers.
// Instrumented compiled bodies are test doubles, not retail gameplay or devices.
#include "ps2/cpu.hpp"
#include "ps2/memory.hpp"
#include "ps2/os.hpp"
#include "mka_retail_hooks.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <utility>
#include <vector>

namespace {
std::map<std::uint32_t,ps2::hook::HookHandler> hooks;
std::vector<std::pair<std::uint32_t,std::uint32_t>> stores;
}
namespace ps2 {
std::uint8_t g_ram[kRamSize];
std::uint8_t g_spr[kScratchpadSize];
std::uint32_t slow_ld32(std::uint32_t) { std::abort(); }
void slow_st32(std::uint32_t,std::uint32_t) { std::abort(); }
bool g_watch_on = true;
std::uint32_t g_watch_base = 0, g_watch_end = kRamSize;
void watch_hit(std::uint32_t at,std::uint32_t value,const char*) { stores.emplace_back(at,value); }
namespace hook {
void register_hook(std::uint32_t pc,HookHandler handler,const char*) { hooks[pc]=handler; }
void unregister_hook(std::uint32_t pc) { hooks.erase(pc); }
}
}
namespace {
unsigned calls = 0;
std::uint32_t actual_entry = 0, actual_depth = 0;
ps2::Cpu incoming;
void put32(std::uint8_t* memory, std::size_t at, std::uint32_t value) {
    std::memcpy(memory + at, &value, sizeof(value));
}
void body(ps2::Cpu& c, std::uint8_t* memory, std::uint32_t entry, std::uint32_t depth) {
    // Deliberate register/FPU/return and memory mutations made only by the
    // original body. Observers must neither predict nor replace this behavior.
    c.r[2].d[0] ^= 0x12345678abcdef09ull;
    c.r[4].d[1] += entry;
    c.f[12] ^= 0x3f123456u;
    c.pc = 0xabcdef00u + depth;
    memory[0x180 + (entry & 63)] ^= 0x5a;
    put32(memory, 0x00660180u, 0x3e765432u);
    put32(memory, 0x0064c3acu, 4u);
    put32(memory, 0x0064e0e4u, 0x1234u);
}
void original(ps2::Cpu& c, std::uint32_t entry, std::uint32_t depth) {
    ++calls;
    actual_entry = entry;
    actual_depth = depth;
    incoming = c;
    body(c, ps2::g_ram, entry, depth);
}
void check(bool ok, const char* why) {
    if (!ok) { std::fprintf(stderr, "FAIL %s\n", why); std::exit(1); }
}
using Wrapper = void (*)(ps2::Cpu&, std::uint32_t, std::uint32_t);
void verify(Wrapper wrapper, std::uint32_t entry, std::uint32_t argument,
            std::uint32_t mode, std::uint32_t player) {
    ps2::Cpu c;
    std::memset(&c, 0xa5, sizeof(c));
    c.r[4].w[0] = argument;
    c.r[5].w[0] = 15;
    c.r[19].w[0] = player ^ 0x12345678u; // Must not replace a0's channel.
    c.f[12] = 0xbd96bb99u;
    put32(ps2::g_ram, 0x00660180u, 0x3f800000u);
    put32(ps2::g_ram, 0x0064dea4u, mode);
    put32(ps2::g_ram, 0x0064c3acu, 3u);
    const auto before = c;
    auto expected = c;
    std::vector<std::uint8_t> memory(ps2::g_ram, ps2::g_ram + ps2::kRamSize);
    body(expected, memory.data(), entry, 17);
    calls = 0;
    wrapper(c, entry, 17);
    check(calls == 1 && actual_entry == entry && actual_depth == 17,
          "original compiled body must run exactly once with unchanged entry/depth");
    check(std::memcmp(&incoming, &before, sizeof(c)) == 0,
          "observer must preserve every incoming register and CPU field");
    check(std::memcmp(&c, &expected, sizeof(c)) == 0,
          "observer must preserve every result field from original body");
    check(std::memcmp(ps2::g_ram, memory.data(), memory.size()) == 0,
          "observer must not add health, input, position, or other memory writes");
}
void verify_leaf(std::uint32_t pc) {
    ps2::Cpu c;
    std::memset(&c,0xa5,sizeof(c));
    c.pc=pc;
    c.r[4].w[0]=0x1200;
    c.r[5].w[0]=0x87654321;
    c.r[28].w[0]=0x660000;
    c.r[31].w[0]=0x12345678;
    auto expected=c;
    expected.pc=c.r[31].w[0];
    std::vector<std::uint8_t> memory(ps2::g_ram,ps2::g_ram+ps2::kRamSize);
    std::vector<std::pair<std::uint32_t,std::uint32_t>> wanted;
    // These explicit effects are pinned to original overlay instruction words
    // in parent research/runs/native-overlay-leaf-review-20261119/REVIEW.json.
    if(pc==0x84a100 || pc==0x7c4980 || pc==0x856700) {
        const unsigned value=pc==0x84a100?2:pc==0x7c4980?1:3;
        expected.r[3].sd[0]=value;
        wanted.emplace_back(c.r[28].w[0]-20304u,value);
    } else if(pc==0x7dc1f8) {
        expected.r[2].sd[0]=0x830000;
        expected.r[3].sd[0]=0x830000;
        wanted.emplace_back(0x82c774,c.r[4].w[0]);
        wanted.emplace_back(0x82c778,c.r[5].w[0]);
    } else if(pc==0x812750) {
        wanted.emplace_back(c.r[4].w[0],0);
        wanted.emplace_back(c.r[4].w[0]+8,c.r[5].w[0]);
        wanted.emplace_back(c.r[4].w[0]+4,0);
    } else check(pc==0x812760,"only original instruction-backed leaves accepted");
    for(const auto& write:wanted) put32(memory.data(),write.first,write.second);
    stores.clear();hooks.at(pc)(c);
    check(std::memcmp(&c,&expected,sizeof(c))==0,"leaf full register effects match original instructions");
    check(stores==wanted,"leaf writes preserve original delay-slot order");
    check(std::memcmp(ps2::g_ram,memory.data(),memory.size())==0,"leaf memory effects match original instructions");
}
}
extern "C" {
void __real__ZN3gen9unit_0018ERN3ps23CpuEjj(ps2::Cpu& c, std::uint32_t e, std::uint32_t d) { original(c,e,d); }
void __real__ZN3gen9unit_0020ERN3ps23CpuEjj(ps2::Cpu& c, std::uint32_t e, std::uint32_t d) { original(c,e,d); }
void __real__ZN3gen9unit_0025ERN3ps23CpuEjj(ps2::Cpu& c, std::uint32_t e, std::uint32_t d) { original(c,e,d); }
void __wrap__ZN3gen9unit_0018ERN3ps23CpuEjj(ps2::Cpu&,std::uint32_t,std::uint32_t);
void __wrap__ZN3gen9unit_0020ERN3ps23CpuEjj(ps2::Cpu&,std::uint32_t,std::uint32_t);
void __wrap__ZN3gen9unit_0025ERN3ps23CpuEjj(ps2::Cpu&,std::uint32_t,std::uint32_t);
void mka_process_audio(const std::int16_t*,std::size_t);
}
int main() {
    std::memset(ps2::g_ram, 0x69, sizeof(ps2::g_ram));
    check(ps2::os::set_env("MKA_TRACE_MODE_INPUT", "1"), "enable read-only scanner trace");
    for (const auto mode : {0u,1u,2u,4u,0xffffffffu}) {
        verify(__wrap__ZN3gen9unit_0025ERN3ps23CpuEjj,204,0,mode,0);
        for (std::uint32_t player=0; player!=2; ++player) {
            verify(__wrap__ZN3gen9unit_0018ERN3ps23CpuEjj,309,player,mode,player);
            verify(__wrap__ZN3gen9unit_0018ERN3ps23CpuEjj,328,player,mode,player);
        }
    }
    for (std::uint32_t id=0; id!=33; ++id)
        verify(__wrap__ZN3gen9unit_0020ERN3ps23CpuEjj,126,id,0,0);
    verify(__wrap__ZN3gen9unit_0020ERN3ps23CpuEjj,126,0xffffffffu,0,0);
    verify(__wrap__ZN3gen9unit_0018ERN3ps23CpuEjj,10,1,0,1);
    verify(__wrap__ZN3gen9unit_0020ERN3ps23CpuEjj,10,1,0,1);
    verify(__wrap__ZN3gen9unit_0025ERN3ps23CpuEjj,10,1,0,1);
    const std::int16_t samples[]={-32768,32767,0,17};
    std::int16_t copy[4]; std::memcpy(copy,samples,sizeof(copy));
    for (int i=0;i!=256;++i) mka_process_audio(samples,2);
    check(std::memcmp(copy,samples,sizeof(copy))==0,"audio observation never modifies samples");
    hooks[0x197bc0]=[](ps2::Cpu&) { std::abort(); };
    mka::retail::register_retail_observers_and_leafs();
    check(hooks.size()==6 && !hooks.count(0x197bc0) && !hooks.count(0x856780),
          "authored health and invented BSS replacements must not be registered");
    for(const auto pc:{0x84a100u,0x7dc1f8u,0x812750u,0x812760u,0x7c4980u,0x856700u}) verify_leaf(pc);
    std::puts("PASS full CPU/memory observer non-interference; all33 retail movie IDs execute original once; six instruction-backed leaf effects; no game/native acceptance claim");
}
