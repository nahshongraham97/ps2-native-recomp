#include "ps2/vu_ops.hpp"
#include "ps2/system.hpp"
#include "vu_micro.hpp"
#include "gs/queue.hpp"
#include "gs/gif.hpp"
#include "hw.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

// Standalone dispatch contract fixture. No game, platform device or interpreter
// is linked; callbacks below are test-native functions, not VU algorithm proof.
namespace ps2 {
alignas(64) std::uint8_t g_vu0_code[4096], g_vu0_mem[4096], g_vu1_code[16384], g_vu1_mem[16384];
Cpu g_cpu;
Options g_options;
namespace gs::queue {
bool vu_active() { return false; }
Role& role() { static Role value=Role::Emulation; return value; }
Ring& vu() { std::abort(); }
void Ring::post(Type,std::uint32_t,std::uint32_t,const void*,std::uint32_t) { std::abort(); }
}
namespace gif {
unsigned fixture_transfers=0;
void transfer(int path,const std::uint8_t* bytes,std::uint32_t count) {
    if (path!=1 || !bytes || count!=1) std::abort();
    ++fixture_transfers;
}
}
}
namespace {
using namespace ps2;
void check(bool ok,const char* why) { if(!ok) { std::fprintf(stderr,"FAIL %s\n",why);std::exit(1); } }
void word(std::size_t at,std::uint32_t bits) { std::memcpy(g_vu1_code+at,&bits,4); }
void code() {
    std::memset(g_vu1_code,0,sizeof g_vu1_code);
    word(0,0x8000033cu);word(4,0x400002ffu);
    word(8,0x8000033cu);word(12,0x400002ffu);
    vu::code_changed(1);
}
std::uint64_t key(std::uint32_t entry) {
    return vu::program_key(g_vu1_code,vu::reachable(g_vu1_code,16383u,entry),entry);
}
unsigned calls_a=0,calls_b=0;
void a(Vu0Regs& r,std::uint8_t* memory) { ++calls_a;r.vi[1]=123;memory[0]=45; }
void b(Vu0Regs& r,std::uint8_t* memory) { ++calls_b;r.vi[1]=234;memory[0]=67; }
void continuation(Vu0Regs& r,std::uint8_t*) { r.vi[2]=89;vu::interpret(1,8); }
void install(vu::ProgramFn function,std::uint32_t entry=0) {
    const vu::Program p{key(entry),function};vu::register_programs(&p,1);
}
}
int main(int argc,char** argv) {
#if defined(_WIN32)
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
    using namespace ps2;
    const std::string mode=argc>1?argv[1]:"positive";
    code();vu::reset();
    if(mode=="missing") { vu::run(1,0);return 91; }
    if(mode=="vu0") { vu::run(0,0);return 92; }
    if(mode=="unaligned") { install(a);vu::run(1,2);return 93; }
    if(mode=="conflict") { install(a);install(b);return 94; }
    if(mode=="null-registry") { vu::register_programs(nullptr,1);return 95; }
    if(mode=="packet-overflow") {
        const std::uint64_t tag=0x8000u|0x7fffu;std::memcpy(g_vu1_mem,&tag,8);
        vu::kick(1,0);return 96;
    }
    if(mode=="packet-no-end") { std::memset(g_vu1_mem,0,sizeof g_vu1_mem);vu::kick(1,0);return 97; }
    if(mode=="continuation") {
        install(continuation);install(b,8);vu::run(1,0);
        check(vu::regs(1).vi[2]==89 && calls_b==1,"linked continuation");
    } else if(mode=="positive") {
        check(vu::strict_aot_enabled(),"mandatory AOT independent of environment");
        install(a);install(a);vu::run(1,0);vu::run(1,0);
        check(calls_a==2 && vu::regs(1).vi[1]==123 && g_vu1_mem[0]==45,"known linked function and cache");
        word(0,0x8000033du);vu::code_changed(1);install(b);vu::run(1,0);
        check(calls_b==1 && g_vu1_mem[0]==67,"changed code selects exact linked identity");
        word(0,0x8000033cu);vu::code_changed(1);vu::run(1,0);
        check(calls_a==3 && g_vu1_mem[0]==45,"restored code selects cached identity");
        vu::set_tops(11,22);vu::reset();
        check(vu::top()==0 && vu::itop()==0 && vu::regs(1).vf[0].w[3]==0x3f800000u,"reset contract");
        std::memset(g_vu1_mem,0,sizeof g_vu1_mem);
        const std::uint64_t tag=0x8000u;std::memcpy(g_vu1_mem,&tag,8);vu::kick(1,0);
        check(gif::fixture_transfers==1,"complete packet submitted once");
        check(vu::interpreted_calls()==0,"no interpreter exists");
    } else return 98;
    std::puts("AOT-only dispatch fixture passed");
}
