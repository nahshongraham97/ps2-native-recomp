#include "vu_micro.hpp"
#include "gs/gif.hpp"
#include "gs/queue.hpp"
#include "hw.hpp"
#include "ps2/fpu.hpp"
#include "ps2/state.hpp"
#include "ps2/system.hpp"
#include "ps2/vu.hpp"
#include "ps2/vu_ops.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ps2::vu {
std::uint32_t g_trace_vu1_pc = 0;
namespace {
Vu0Regs g_vu1;
std::uint64_t g_generation[2] = {1,1};
std::uint32_t g_top = 0, g_itop = 0;
[[noreturn]] void missing(int unit, std::uint32_t pc, std::uint64_t key, const char* why) {
    std::fprintf(stderr,"[vu%d-aot-stop] pc=%08X key=%016llX: %s; no interpreter or runtime compilation exists\n",
                 unit,pc,static_cast<unsigned long long>(key),why);
    std::fflush(stderr); std::abort();
}
struct Candidate {
    std::vector<std::uint32_t> runs;
    std::vector<std::uint8_t> code;
    std::uint64_t key = 0;
    ProgramFn function = nullptr;
};
struct EntryCache {
    std::uint64_t generation = 0, key = 0;
    ProgramFn function = nullptr;
    std::vector<Candidate> candidates;
};
std::unordered_map<std::uint64_t,ProgramFn> g_programs;
std::unordered_map<std::uint32_t,EntryCache> g_entries;
bool g_chaining = false;
std::int64_t g_chained = -1;
bool candidate_matches(const Candidate& c) {
    const auto* bytes = c.code.data();
    for (std::size_t i=0;i<c.runs.size();i+=2) {
        if (std::memcmp(g_vu1_code+c.runs[i],bytes,c.runs[i+1])) return false;
        bytes+=c.runs[i+1];
    }
    return true;
}
Candidate make_candidate(std::vector<std::uint32_t> pcs) {
    std::sort(pcs.begin(),pcs.end()); Candidate c;
    for (auto pc:pcs) {
        if (!c.runs.empty() && c.runs[c.runs.size()-2]+c.runs.back()==pc) c.runs.back()+=8;
        else { c.runs.push_back(pc); c.runs.push_back(8); }
        c.code.insert(c.code.end(),g_vu1_code+pc,g_vu1_code+pc+8);
    }
    return c;
}
ProgramFn lookup(std::uint32_t pc) {
    if ((pc&7u) || pc>=sizeof(g_vu1_code)) missing(1,pc,0,"invalid statically compiled entry");
    auto& e=g_entries[pc];
    if (e.generation==g_generation[1]) return e.function;
    e.generation=g_generation[1];
    for (const auto& c:e.candidates) if (candidate_matches(c)) {
        const auto it=g_programs.find(c.key);
        e.function=it==g_programs.end()?nullptr:it->second;
        e.key=c.key; return e.function;
    }
    // Inspect serialized program identity; this does not execute its words.
    const auto pcs=reachable(g_vu1_code,16383u,pc);
    e.key=program_key(g_vu1_code,pcs,pc);
    const auto it=g_programs.find(e.key);
    e.function=it==g_programs.end()?nullptr:it->second;
    auto c=make_candidate(pcs); c.key=e.key; c.function=e.function;
    e.candidates.push_back(std::move(c)); return e.function;
}
void run_entry(int unit,std::uint32_t pc) {
    if (unit!=1) missing(unit,pc,0,"no statically registered VU0 microprogram dispatcher");
    g_trace_vu1_pc=pc;
    const auto fn=lookup(pc);
    if (!fn) missing(unit,pc,g_entries[pc].key,"missing statically registered C++ program");
    fn(g_vu1,g_vu1_mem);
}
} // namespace

void reset() {
    if (gs::queue::vu_active() && gs::queue::role() != gs::queue::Role::Vu) {
        gs::queue::vu().post(gs::queue::kVuReset, 0, 0, nullptr, 0);
        return;
    }
    std::memset(&g_vu1, 0, sizeof g_vu1);
    g_vu1.vf[0].w[3] = fp::kOne;
    g_top = g_itop = 0;
}

Vu0Regs &regs(int unit) { return unit ? g_vu1 : g_cpu.vu0; }

std::uint32_t tpc(int unit) { return regs(unit).tpc; }

void set_tops(std::uint32_t top, std::uint32_t itop) {
    g_top = top;
    g_itop = itop;
}


void code_changed(int unit) {
    if (unit<0 || unit>1) missing(unit,0,0,"invalid vector unit");
    if (++g_generation[unit]==0) {
        g_generation[unit]=1;
        for (auto& e:g_entries) e.second.generation=0;
    }
}
void register_programs(const Program* programs,std::uint32_t count) {
    if (count && !programs) missing(1,0,0,"null compiled program registry");
    for (std::uint32_t i=0;i<count;++i) {
        if (!programs[i].function) missing(1,0,programs[i].key,"null compiled program function");
        auto result=g_programs.emplace(programs[i].key,programs[i].function);
        if (!result.second && result.first->second!=programs[i].function)
            missing(1,0,programs[i].key,"conflicting compiled program identities");
    }
    // Registering a newly linked table also invalidates cached missing entries.
    code_changed(1);
}
bool strict_aot_enabled() { return true; }
std::uint64_t interpreted_calls() { return 0; } // No interpreter implementation exists here.
std::uint32_t top() { return g_top; }
std::uint32_t itop() { return g_itop; }
// ABI name retained for existing generated callers. It dispatches only linked
// C++ functions, including tail continuations; no instruction decoder exists.
void interpret(int unit,std::uint32_t pc) {
    if (unit!=1) missing(unit,pc,0,"no statically registered VU0 microprogram dispatcher");
    if (g_chaining) { g_chained=pc; return; }
    g_chaining=true;
    for (;;) {
        run_entry(unit,pc);
        if (g_chained<0) break;
        pc=static_cast<std::uint32_t>(g_chained); g_chained=-1;
    }
    g_chaining=false;
}
void run(int unit,std::uint32_t pc) { interpret(unit,pc); }
void kick(int unit,std::uint32_t address) {
    if (unit<0 || unit>1) missing(unit,address,0,"invalid packet source unit");
    const auto* data=unit?g_vu1_mem:g_vu0_mem;
    const std::uint32_t qwords=unit?1024u:256u;
    std::vector<std::uint8_t> packet;
    std::uint32_t q=address&(qwords-1), count=0;
    bool end=false;
    while (!end && count<qwords) {
        Reg128 tag; std::memcpy(&tag,data+(q&(qwords-1))*16,16);
        const auto* raw=reinterpret_cast<const std::uint8_t*>(&tag);
        packet.insert(packet.end(),raw,raw+16); ++q; ++count;
        const std::uint32_t loops=tag.d[0]&0x7fffu;
        const auto format=static_cast<std::uint32_t>(tag.d[0]>>58)&3u;
        auto nreg=static_cast<std::uint32_t>(tag.d[0]>>60)&15u; if (!nreg) nreg=16;
        const auto body=format==0?loops*nreg:format==1?(loops*nreg+1)/2:loops;
        if (body>qwords-count) missing(unit,address,0,"packet exceeds retained source region; refusing partial transfer");
        for (std::uint32_t i=0;i<body;++i,++q,++count) {
            const auto* word=data+(q&(qwords-1))*16;
            packet.insert(packet.end(),word,word+16);
        }
        end=(tag.d[0]>>15)&1u;
    }
    if (!end) missing(unit,address,0,"packet has no end marker; refusing partial transfer");
    gif::transfer(1,packet.data(),count);
}

// PS2_DUMP_VU1AT (see the GS draw trace): the vector registers and the 16 KiB of data memory of unit 1, as text, at one primitive.
void trace_dump(const char *path) {
    std::FILE *f = std::fopen(path, "w");
    if (!f) return;
    for (int i = 0; i < 32; ++i) {
        float v[4];
        std::memcpy(v, &g_vu1.vf[i], 16);
        std::fprintf(f, "vf%02d %g %g %g %g\n", i, v[0], v[1], v[2], v[3]);
    }
    for (int qw = 0; qw < 1024; ++qw) {
        float v[4];
        std::uint32_t w[4];
        std::memcpy(v, g_vu1_mem + qw * 16, 16);
        std::memcpy(w, g_vu1_mem + qw * 16, 16);
        std::fprintf(f, "m%03X %08X %08X %08X %08X | %g %g %g %g\n", qw, w[0], w[1], w[2], w[3], v[0], v[1], v[2], v[3]);
    }
    std::fclose(f);
}

void state(ps2::state::Stream &s) {
    s.tag("vu");
    s.pod(g_vu1);
    s.pod(g_top);
    s.pod(g_itop);
    s.bytes(g_vu0_code, sizeof g_vu0_code);
    s.bytes(g_vu0_mem, sizeof g_vu0_mem);
    s.bytes(g_vu1_code, sizeof g_vu1_code);
    s.bytes(g_vu1_mem, sizeof g_vu1_mem);
    if (s.loading()) {
        code_changed(0);
        code_changed(1);
    }
}

} // namespace ps2::vu
namespace ps2 { void vcallms(Cpu&,std::uint32_t address) { vu::run(0,address); } }
__attribute__((weak)) void register_generated_vu() {}
