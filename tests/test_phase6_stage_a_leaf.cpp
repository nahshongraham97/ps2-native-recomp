// Disposable differential boundary only; no product/protected-oracle linkage.
#define main phase4_main_not_invoked_by_phase6
#include "test_phase4_aot_equivalence.cpp"
#undef main
#include "stage_a_leaf.hpp"
#include "trace_reference.hpp"

namespace {
struct Write {
    std::uint32_t address;
    unsigned width;
    std::array<std::uint8_t, 16> value{};
    bool operator==(const Write&) const = default;
};
std::vector<Write> writes;
std::string plant;
unsigned comparisons = 0, captured = 0, constructed = 0, captured_entry_pc_matches = 0;
void require_leaf(bool ok, const char* why) {
    if (!ok) {
        std::fprintf(stderr, "FAIL_STAGE_A_LEAF case=%u plant=%s reason=%s\n",
                     comparisons, plant.c_str(), why);
        std::exit(1);
    }
}
template<class T> void record(std::uint32_t address, const T& value) {
    Write w{address, sizeof(T), {}};
    std::memcpy(w.value.data(), &value, sizeof(T));
    writes.push_back(w);
}
void trace_comparator_controls() {
    writes.clear();
    record(8,std::uint8_t{0x80});record(12,std::uint16_t{0x8000});
    record(16,std::uint32_t{0x80000000});record(24,std::uint64_t{0x8000000000000000ull});
    const auto expected=writes;
    for (unsigned control=0;control!=5;++control) {
        auto altered=expected;
        if(control==0)altered[0].address^=1;
        if(control==1)altered[1].width=1;
        if(control==2)altered[3].value[7]^=0x80;
        if(control==3)std::swap(altered[0],altered[1]);
        if(control==4)altered.pop_back();
        require_leaf(altered!=expected,"address/width/full-payload/order/omission plant escaped");
    }
    writes.clear();
}
void same_cpu(const ps2::Cpu& a, const ps2::Cpu& b) {
    require_leaf(std::memcmp(a.r,b.r,sizeof a.r)==0, "all GPR128 liveouts and preserved lanes");
    require_leaf(std::memcmp(&a.hi,&b.hi,sizeof a.hi)==0 &&
                 std::memcmp(&a.lo,&b.lo,sizeof a.lo)==0 && a.sa==b.sa, "HI LO SA");
    require_leaf(a.pc==b.pc && a.budget==b.budget && a.base==b.base, "return contract/accounting");
    require_leaf(std::memcmp(a.f,b.f,sizeof a.f)==0 && a.facc==b.facc &&
                 a.fcr31==b.fcr31, "FPR preservation");
    require_leaf(std::memcmp(&a.vu0,&b.vu0,sizeof a.vu0)==0 &&
                 std::memcmp(a.cop0,b.cop0,sizeof a.cop0)==0 &&
                 std::memcmp(a.pcr,b.pcr,sizeof a.pcr)==0 && a.pcr_at==b.pcr_at,
                 "VU COP PCR preservation");
}
enum class Route { aot, stage_a };
ps2::Cpu invoke(Route route, const ps2::Cpu& before) {
    if (route==Route::aot) {
        ps2::g_cpu=before;
        gen::unit_0028(ps2::g_cpu,294u,0u);
        return ps2::g_cpu;
    }
    auto result=before;
    // This adapter is test-side source-state projection, not the native ABI.
    const auto returned=stage_a::close_resource_heap({before.r[4].w[0]});
    result.r[2].d[0]=static_cast<std::uint64_t>(static_cast<std::int64_t>(returned));
    result.pc=before.r[31].w[0];
    result.budget-=2;
    if (comparisons==0) {
        if (plant=="--planted-width") result.r[2].d[0]^=0x100000000ull;
        if (plant=="--planted-stale-ssa") result.r[2].d[0]=before.r[3].d[0];
        if (plant=="--planted-delay") result.r[2].d[0]=before.r[2].d[0];
        if (plant=="--planted-invented-input") result.r[2].d[0]=before.r[4].w[0];
        if (plant=="--planted-return-contract") result.pc^=4;
        if (plant=="--planted-frame-size") result.r[29].d[0]^=16;
        if (plant=="--planted-transient-write") {
            const auto old=ps2::ld32(8);
            record(8,old^1u); ps2::st32(8,old^1u);
            record(8,old); ps2::st32(8,old);
        }
    }
    return result;
}
void compare_one(ps2::Cpu before) {
    require_leaf(!(before.r[31].w[0]&3u),"aligned original return link");
    before.pc=0x001a3c40u;
    before.budget=1000000; // Explicit test host allowance; not a guest timing claim.
    const std::vector<std::uint8_t> initial(ps2::g_ram,ps2::g_ram+ps2::kRamSize);
    writes.clear();
    const auto candidate=invoke(Route::stage_a,before);
    const auto candidate_writes=writes;
    require_leaf(std::memcmp(initial.data(),ps2::g_ram,initial.size())==0,"candidate full RAM preservation");
    const auto actual=invoke(Route::aot,before);
    same_cpu(actual,candidate);
    require_leaf(std::memcmp(initial.data(),ps2::g_ram,initial.size())==0,"actual AOT full RAM preservation");
    // Independent observed copy retains all original AOT operations. All store
    // widths/payloads are logged, so a write followed by restoration cannot hide.
    writes.clear(); ps2::g_cpu=before;
    leaf_reference::unit_0028(ps2::g_cpu,294u,0u);
    same_cpu(ps2::g_cpu,actual);
    require_leaf(writes.empty(),"original source-proved empty write/effect trace");
    require_leaf(candidate_writes==writes,"ordered address/width/value write trace");
    require_leaf(std::memcmp(initial.data(),ps2::g_ram,initial.size())==0,"observed AOT full RAM preservation");
    ++comparisons;
}
}

namespace leaf_reference {
std::uint8_t observed_ld8(std::uint32_t a) {return ps2::ld8(a);}
std::uint16_t observed_ld16(std::uint32_t a) {return ps2::ld16(a);}
std::uint32_t observed_ld32(std::uint32_t a) {return ps2::ld32(a);}
std::uint64_t observed_ld64(std::uint32_t a) {return ps2::ld64(a);}
void observed_ld128(std::uint32_t a,ps2::Reg128& v) {ps2::ld128(a,v);}
void observed_st8(std::uint32_t a,std::uint8_t v) {record(a,v);ps2::st8(a,v);}
void observed_st16(std::uint32_t a,std::uint16_t v) {record(a,v);ps2::st16(a,v);}
void observed_st32(std::uint32_t a,std::uint32_t v) {record(a,v);ps2::st32(a,v);}
void observed_st64(std::uint32_t a,std::uint64_t v) {record(a,v);ps2::st64(a,v);}
void observed_st128(std::uint32_t a,const ps2::Reg128& v) {record(a,v);ps2::st128(a,v);}
}

int main(int argc,char** argv) {
    if (argc<2 || argc>3) return 2;
    if (argc==3) plant=argv[2];
    const std::set<std::string> allowed{"", "--planted-width", "--planted-stale-ssa",
        "--planted-delay", "--planted-invented-input", "--planted-return-contract",
        "--planted-frame-size", "--planted-transient-write"};
    if (!allowed.contains(plant)) return 2;
    trace_comparator_controls();
    std::vector<fs::path> files;
    for (const auto& f:fs::directory_iterator(argv[1]))
        if(f.is_regular_file() && f.path().extension()==".state")files.push_back(f.path());
    std::sort(files.begin(),files.end());
    require_leaf(files.size()==61,"exact61 saved states");
    OracleVuLayout layout;std::string reason;
    require_leaf(load_oracle_ee_vu(read_binary(files.front()),layout,reason),"checked v8 state loader");
    const auto seed=ps2::g_cpu;
    const std::array<std::uint32_t,8> references{0u,1u,4u,0x7fffffffu,0x80000000u,
        ps2::kRamSize-1u,ps2::kRamSize,0xffffffffu};
    const std::array<std::uint64_t,4> patterns{0ull,0x80000000ull,0xffffffffffffffffull,0x8000000000000000ull};
    const std::array<std::uint32_t,4> returns{0u,4u,0x00100000u,0xfffffffcu};
    for(auto reference:references)for(auto pattern:patterns)for(auto ret:returns){
        auto c=seed;
        c.r[4].d[0]=0xdead000000000000ull|reference;
        c.r[2].d[0]=pattern;c.r[2].d[1]=~pattern;
        c.r[3].d[0]=pattern;c.r[29].d[0]=pattern;
        c.r[31].w[0]=ret;
        compare_one(c);++constructed;
    }
    for(const auto& p:files){
        require_leaf(load_oracle_ee_vu(read_binary(p),layout,reason),"checked original state ABI");
        const auto c=ps2::g_cpu;
        if(c.pc==0x001a3c40u)++captured_entry_pc_matches;
        compare_one(c);++captured;
    }
    std::printf("PASS_STAGE_A_LEAF snapshot_seeded=%u captured_entry_pc_matches=%u constructed=%u total=%u "
                "ordered_write_events=0 source_proved_external_effects=0 trace_comparator_controls=5 capability_only=1 credit_delta=0\n",
                captured,captured_entry_pc_matches,constructed,comparisons);
}
