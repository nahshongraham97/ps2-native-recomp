#include "mka/bank_preparation.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <future>

using namespace mka::sound;
namespace {
void check(bool value,const char* why){if(!value){std::fprintf(stderr,"FAIL: %s\n",why);std::abort();}}
void put(std::vector<std::uint8_t>& b,std::size_t at,std::uint32_t value,unsigned n=4){for(unsigned i=0;i<n;++i)b[at+i]=value>>(8*i);}
CommandRecord command(std::uint32_t key){CommandRecord c;for(unsigned i=0;i<4;++i)c.bytes[i]=key>>(8*i);c.bytes[8]=1;return c;}
std::vector<std::uint8_t> bank_bytes(std::vector<std::uint32_t> flags,
    const std::vector<std::vector<CommandRecord>>& commands){
    const auto n=flags.size();std::size_t count=0;for(const auto& row:commands)count+=row.size();
    const std::size_t ro=48,po=84,ao=po+16*n,co=ao+16*n,so=co+count*40;
    std::vector<std::uint8_t> b(so+2*n);put(b,0,11);put(b,4,0x13);put(b,8,1,2);put(b,10,n,2);put(b,12,n,2);
    put(b,16,ro);put(b,20,po);put(b,24,ao);put(b,28,co);put(b,32,so);
    put(b,ro+8,0x20000002);put(b,ro+12,22050);put(b,ro+16,256);put(b,ro+20,36);
    std::size_t at=co;
    for(std::size_t i=0;i<n;++i){put(b,po+i*16+12,flags[i]);put(b,ao+i*16+8,commands[i].size());put(b,so+i*2,i+1,2);
        for(const auto& c:commands[i]){std::copy(c.bytes.begin(),c.bytes.end(),b.begin()+at);at+=40;}}
    return b;
}
PreparedBank prepare(std::shared_ptr<SoundArchiveBacking> backing,std::vector<std::uint8_t> bytes){
    const auto length=bytes.size();BankPreparationReport report;PreparationFailure failure;
    auto bank=PreparedBank::prepare({{1,0,length,"explicit-policy-fixture.mst"},std::move(bytes),std::move(backing)},report,failure);
    check(bool(bank),"explicit native fixture bank preparation");return std::move(*bank);
}
bool cached(PreparedBank& bank,bool preserved){PreparationFailure f;auto c=bank.sample_cache(f);check(c&&c->size()==1,"one actual fixture resource");return (*c)[0].occupied&&(*c)[0].preserved==preserved;}
}
int main(int argc,char** argv){
    if(argc!=2)return 2;
    const std::filesystem::path directory(argv[1]);check(std::filesystem::create_directory(directory),"exclusive policy directory");
    std::vector<std::uint8_t> bytes(292);put(bytes,260,65536);put(bytes,264,1);put(bytes,268,22050);put(bytes,272,16);
    bytes[276]=12;bytes[277]=7;for(unsigned i=278;i<292;++i)bytes[i]=0x11;
    {std::ofstream out(directory/"payload.bin",std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());check(bool(out),"explicit payload write");}
    ArchiveFailure af;auto backing=SoundArchiveBacking::open(directory/"payload.bin",af);check(bool(backing),"native backing");
    PreparationFailure f;
    auto fresh=prepare(backing,bank_bytes({2},{{command(0x20000002),command(0x20000002)}}));
    auto work=fresh.admit_deferred_normal(0,f);check(work&&work->build(f),"real fresh candidate build");
    auto program=work->take_result(f);check(program&&program->normal_instances.size()==2&&work->retire(f),"two ordered actual occurrences published");
    check(!program->normal_instances[0].source_preserve_resource_after_instance&&
        !program->normal_instances[1].source_preserve_resource_after_instance&&cached(fresh,false),"clear40 occurrences decrement on cleanup");
    check(program->normal_instances[0].cache_reference==program->normal_instances[1].cache_reference,"repeated source resource shares cache association");
    auto occurrence=program->normal_instances[0];auto pcm=occurrence.sample;program.reset();
    check(cached(fresh,false),"copied occurrence retains actual acquisition");
    occurrence.cache_reference.reset();check(!cached(fresh,false)&&pcm->pcm[0].samples.size()==28,"exported PCM alone cannot retain source cache association");

    auto sticky=prepare(backing,bank_bytes({0,2},{{command(0x20000002)},{command(0x20000002)}}));
    auto eager=sticky.eager_program(0,f);check(eager&&eager->normal_instances[0].source_preserve_resource_after_instance&&cached(sticky,true),"source eager sets sticky preserve40");
    check(!sticky.admit_deferred_normal(0,f)&&f.kind==PreparationError::already_prepared,"fresh admission cannot impersonate eager data or runtime cached branch");
    auto deferred=sticky.admit_deferred_normal(1,f);check(bool(deferred),"explicit source-deferred selector");
    AudioResourceFailure rf;auto domain=backing->resource_domain();auto movie=domain->surround_movie(rf);check(bool(movie),"real publication exclusion");
    auto completed=std::async(std::launch::async,[&]{PreparationFailure e;check(deferred->build(e),"cached normal data needs no new PCM publication");auto p=deferred->take_result(e);check(p&&deferred->retire(e),"same worker publishes and retires");return p;});
    check(completed.wait_for(std::chrono::seconds(10))==std::future_status::ready,"cached preparation must finish while movie publication remains excluded");
    auto inherited=completed.get();check(movie->release(rf),"actual exclusion release");
    check(inherited->effective_flags==2&&inherited->normal_instances[0].source_preserve_resource_after_instance&&
        inherited->normal_instances[0].sample==eager->normal_instances[0].sample,"deferred occurrence snapshots existing sticky40 without changing program flags");
    inherited.reset();eager.reset();check(cached(sticky,true),"preserve40 cache remains associated after returned instances release");

    auto failed=prepare(backing,bank_bytes({2},{{command(0x20000002),command(0x20000099)}}));
    auto failure=failed.admit_deferred_normal(0,f);check(failure&&!failure->build(f)&&f.kind==PreparationError::absent_resource,"failed private prefix reports exact source lookup error");
    check(!cached(failed,false)&&failure->retire(f),"failed clear40 prefix retires all its own cache occurrences");
    auto abandoned=prepare(backing,bank_bytes({2},{{command(0x20000002)}}));
    auto dropped=abandoned.admit_deferred_normal(0,f);check(dropped&&dropped->build(f)&&cached(abandoned,false),"private ready candidate actually owns cache");
    dropped.reset();check(!cached(abandoned,false)&&domain->observe()[0].depth==0,"abandonment cleans candidate before releasing actual work; no completion callback claimed");
    sticky.unload();check(!sticky.sample_cache(f)&&f.kind==PreparationError::unloaded&&pcm->pcm[0].samples.size()==28,"unload revokes associations and keeps exported immutable data valid");
    std::puts("PASS native sample cache flag40 polarity/occurrence ownership/PCM separation/private candidate cleanup; explicit source-format fixtures only");
    return std::fflush(stdout)||std::ferror(stdout)?1:0;
}
