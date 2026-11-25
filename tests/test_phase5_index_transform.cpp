// Reference-only AOT/serializer/RAM adapter. The generated native library has
// only IndexSelection& and a u64 liveout; no guest state or address interface.
#define main phase4_main_not_invoked_by_index_transform
#include "test_phase4_aot_equivalence.cpp"
#undef main
#include "native_index_selection.hpp"
#include <chrono>

namespace gen { void unit_0013(ps2::Cpu&, std::uint32_t, std::uint32_t); }

namespace {
using mka::retail::generated::IndexSelection;
using TypedIndex = std::uint64_t (*)(IndexSelection&) noexcept;
constexpr std::uint32_t index_pc = 0x0014de80u, index_entry = 103u, index_offset = 0xca8u;
std::string index_context;
volatile std::uint64_t index_sink = 0;
void index_require(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr,"FAIL IndexTransform context=%s: %s\n",index_context.c_str(),message);std::exit(1); }
}
void index_compare_cpu(const ps2::Cpu& a,const ps2::Cpu& e) {
    index_require(std::memcmp(a.r,e.r,sizeof a.r)==0,"complete GPR128 delta");
    index_require(std::memcmp(&a.hi,&e.hi,sizeof a.hi)==0&&std::memcmp(&a.lo,&e.lo,sizeof a.lo)==0,"HI/LO");
    index_require(a.sa==e.sa&&a.pc==e.pc,"SA/return PC");
    index_require(std::memcmp(a.f,e.f,sizeof a.f)==0&&a.facc==e.facc&&a.fcr31==e.fcr31,"FPU");
    const auto& av=a.vu0;const auto& ev=e.vu0;
    index_require(std::memcmp(av.vf,ev.vf,sizeof av.vf)==0&&std::memcmp(&av.acc,&ev.acc,sizeof av.acc)==0&&std::memcmp(av.vi,ev.vi,sizeof av.vi)==0,"VU vectors");
    index_require(av.q==ev.q&&av.p==ev.p&&av.i==ev.i&&av.r==ev.r&&av.mac==ev.mac&&av.status==ev.status&&av.clip==ev.clip&&av.cmsar0==ev.cmsar0&&av.tpc==ev.tpc,"VU scalars");
    index_require(std::memcmp(a.cop0,e.cop0,sizeof a.cop0)==0&&std::memcmp(a.pcr,e.pcr,sizeof a.pcr)==0,"COP0/PCR");
    index_require(a.budget==e.budget&&a.base==e.base&&a.pcr_at==e.pcr_at,"host accounting");
}
bool index_admit_receiver(std::uint32_t pointer) {
    return pointer && !(pointer&3u) && pointer<=ps2::kRamSize-index_offset-8u;
}
void index_compare_one(const ps2::Cpu& input,const std::vector<std::uint8_t>& ram,std::uint32_t receiver,bool wrong_value,bool wrong_field) {
    index_require(index_admit_receiver(receiver),"receiver extent/alignment");
    auto saved=input;
    // Exact original caller LW a0 in its delay slot: low32 sign-extended to
    // low64, upper64 preserved. This is test input projection, never native code.
    saved.r[4].d[0]=static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(receiver)));
    index_require(!(saved.r[31].w[0]&3u)&&saved.budget>=std::numeric_limits<std::int32_t>::min()+3,"return/budget admission");
    const auto address=receiver+index_offset;
    IndexSelection owner{std::bit_cast<std::int32_t>(read_u32_le(ram.data()+address))};
    ps2::g_cpu=saved;
    const auto liveout=mka::retail::generated::invalidate_index(owner);
    index_compare_cpu(ps2::g_cpu,saved);
    index_require(std::memcmp(ps2::g_ram,ram.data(),ps2::kRamSize)==0,"native kernel changed reference RAM");
    if(wrong_value)owner.index=0;
    auto expected=saved;expected.r[3].d[0]=liveout;expected.pc=saved.r[31].w[0];expected.budget=saved.budget-3;
    auto expected_ram=ram;
    const auto bits=std::bit_cast<std::uint32_t>(owner.index);
    const auto projected=address+(wrong_field?4u:0u);
    for(unsigned i=0;i!=4;++i)expected_ram[projected+i]=std::uint8_t(bits>>(i*8));
    ps2::g_cpu.pc=index_pc;
    gen::unit_0013(ps2::g_cpu,index_entry,0u);
    index_compare_cpu(ps2::g_cpu,expected);
    index_require(std::memcmp(ps2::g_ram,expected_ram.data(),ps2::kRamSize)==0,"full RAM/canary differential: index field only");
    std::copy_n(ram.data()+address,4,ps2::g_ram+address);ps2::g_cpu=saved;
}
void index_admit_original(const fs::path& path) {
    const auto raw=read_binary(path);index_require(raw.size()>64&&std::memcmp(raw.data(),"\177ELF",4)==0,"original ELF header");
    const auto table=read_u32_le(raw.data()+28);const auto stride=read_u32_le(raw.data()+40)>>16;const auto count=read_u32_le(raw.data()+44)&0xffffu;
    bool found=false;
    for(unsigned i=0;i<count;++i){
        const auto header=std::uint64_t(table)+std::uint64_t(i)*stride;
        index_require(header+32u<=raw.size(),"original ELF program header bounds");
        const auto p=raw.data()+header;
        const auto base=read_u32_le(p+8),size=read_u32_le(p+16),offset=read_u32_le(p+4);
        if(read_u32_le(p)==1u&&base<=index_pc&&index_pc+12u<=base+size){
            const auto at=offset+index_pc-base;index_require(at+12u<=raw.size(),"original method file extent");
            const std::array<std::uint32_t,3> words{0x2403ffffu,0x03e00008u,0xac830ca8u};
            for(unsigned k=0;k!=3;++k)index_require(read_u32_le(raw.data()+at+k*4u)==words[k],"original signed-index leaf words");
            found=true;
        }
    }
    index_require(found,"unique file-backed original signed-index leaf");
}
unsigned index_synthetic(bool wrong_value,bool wrong_field) {
    const std::array<std::uint32_t,6> old_values{0u,1u,0xffffffffu,0x80000000u,0x164u,0x17eu};
    const std::array<std::uint32_t,3> receivers{4u,0x123450u,ps2::kRamSize-index_offset-8u};
    unsigned total=0;
    for(auto receiver:receivers)for(auto value:old_values){
        ps2::Cpu c;std::memset(&c,0xa5,sizeof c);c.r[0].d[0]=c.r[0].d[1]=0;
        c.r[31].w[0]=0x001f8124u;c.budget=std::numeric_limits<std::int32_t>::max();
        for(std::size_t k=0;k!=ps2::kRamSize;++k)ps2::g_ram[k]=std::uint8_t((k*17u)^(k>>8)^total);
        for(unsigned k=0;k!=4;++k)ps2::g_ram[receiver+index_offset+k]=std::uint8_t(value>>(k*8));
        const std::vector<std::uint8_t> ram(ps2::g_ram,ps2::g_ram+ps2::kRamSize);
        index_context="synthetic-"+std::to_string(total);
        index_compare_one(c,ram,receiver,wrong_value&&total==0,wrong_field&&total==0);++total;
    }
    index_require(!index_admit_receiver(0)&&!index_admit_receiver(3)&&!index_admit_receiver(ps2::kRamSize-index_offset),"illegal receiver controls");
    return total;
}
void index_benchmark(ps2::Cpu c,std::uint32_t receiver) {
    constexpr std::uint32_t n=200000;const auto address=receiver+index_offset;
    c.r[4].d[0]=receiver;c.budget=std::numeric_limits<std::int32_t>::max();
    IndexSelection owner{};std::uint64_t raw_sum=0,typed_sum=0;
    using Raw=void(*)(ps2::Cpu&,std::uint32_t,std::uint32_t);
    const auto start=std::chrono::steady_clock::now();
    for(unsigned i=0;i!=n;++i){Raw volatile raw=gen::unit_0013;c.pc=index_pc;raw(c,index_entry,0);raw_sum+=c.r[3].d[0]+read_u32_le(ps2::g_ram+address);}
    const auto middle=std::chrono::steady_clock::now();
    for(unsigned i=0;i!=n;++i){TypedIndex volatile typed=mka::retail::generated::invalidate_index;const auto value=typed(owner);typed_sum+=value+std::bit_cast<std::uint32_t>(owner.index);}
    const auto end=std::chrono::steady_clock::now();index_sink=raw_sum^(typed_sum<<1);
    index_require(raw_sum==typed_sum&&raw_sum!=0,"benchmark observable checksums");
    std::printf("INDEX_BENCH calls_per_route=%u generated_ns=%lld typed_ns=%lld checksum=%llu native_owner_bytes=%zu liveout_bytes=%zu scope=isolated_kernel_not_frame\n",n,
      static_cast<long long>(std::chrono::duration_cast<std::chrono::nanoseconds>(middle-start).count()),
      static_cast<long long>(std::chrono::duration_cast<std::chrono::nanoseconds>(end-middle).count()),static_cast<unsigned long long>(raw_sum),sizeof owner,sizeof(std::uint64_t));
}
}
int main(int argc,char** argv) {
    if(argc<3||argc>4||(argc==4&&std::string(argv[3])!="--planted-value"&&std::string(argv[3])!="--planted-field")){std::fprintf(stderr,"Usage: %s <oracle_states_dir> <original_ELF> [--planted-value|--planted-field]\n",argv[0]);return 2;}
    static_assert(sizeof(IndexSelection)==sizeof(std::int32_t));
    index_admit_original(fs::path(argv[2]));
    const auto synthetic=index_synthetic(argc==4&&std::string(argv[3])=="--planted-value",argc==4&&std::string(argv[3])=="--planted-field");
    ps2::Cpu constructed{};constructed.r[31].w[0]=0x001f8124u;
    index_benchmark(constructed,0x123450u);
    std::vector<fs::path> states;for(const auto& e:fs::directory_iterator(fs::path(argv[1])))if(e.is_regular_file()&&e.path().extension()==".state")states.push_back(e.path());std::sort(states.begin(),states.end());
    index_require(states.size()==61u,"expected61 pinned states");unsigned admitted=0,inapplicable=0;ps2::Cpu last{};std::uint32_t last_receiver=0;
    for(const auto& path:states){index_context=path.filename().string();const auto raw=read_binary(path);OracleVuLayout layout;std::string reason;
        index_require(load_oracle_ee_vu(raw,layout,reason),"oracle serializer admission");check_restored_vu(raw,layout);const auto saved=ps2::g_cpu;
        const bool gp=saved.r[28].w[0]==0x006540f0u;const auto receiver=gp?read_u32_le(ps2::g_ram+0x0064dfc0u):0u;
        if(!gp||!index_admit_receiver(receiver)||saved.r[31].w[0]&3u||saved.budget<std::numeric_limits<std::int32_t>::min()+3){
            std::printf("INDEX_STATE_INAPPLICABLE file=%s gp=%08X receiver=%08X reason=%s\n",path.filename().string().c_str(),saved.r[28].w[0],receiver,!gp?"source-GP mismatch":!receiver?"null original receiver":!index_admit_receiver(receiver)?"receiver extent/alignment":"return/budget");++inapplicable;continue;}
        const std::vector<std::uint8_t> ram(ps2::g_ram,ps2::g_ram+ps2::kRamSize);index_compare_one(saved,ram,receiver,false,false);++admitted;last=saved;last_receiver=receiver;
        std::printf("INDEX_STATE_ADMITTED file=%s original_receiver=%08X field=%08X\n",path.filename().string().c_str(),receiver,receiver+index_offset);
    }
    std::printf("INDEX_ADMISSION states=%zu admitted=%u inapplicable=%u generated_typed_comparisons=%u synthetic_comparisons=%u benchmark_receiver=constructed production_callers=0\n",states.size(),admitted,inapplicable,admitted,synthetic);
    index_require(admitted>0,"no legal saved-state receiver: prototype cannot claim saved-state E2");index_benchmark(last,last_receiver);
    std::printf("PASS_PHASE5_INDEX_TRANSFORM states=%zu admitted=%u inapplicable=%u generated_typed_comparisons=%u synthetic_comparisons=%u full_RAM_bytes=%zu production_callers=0 scope=typed_field_projection_only\n",states.size(),admitted,inapplicable,admitted,synthetic,std::size_t(ps2::kRamSize));return 0;
}
