#include "mka/phase7_stub_native.hpp"
#include "phase7_track_b_capture.hpp"
#include <cstdio>
#include <set>
using namespace mka::phase7;
static void source_service_cases() {
    struct Callback final:NativeParticleCallbackService {
        bool update(const NativeParticleCallbackRequest& request,std::int32_t& count)override {
            if(request.count!=3||request.preceding!=2||request.delta_bits!=0x3f800000)throw std::runtime_error("native callback ABI");
            count=7;return true;
        }
    } callback;
    NativeParticleBatch batch;batch.has_dynamic_callback=true;batch.count=3;batch.preceding=2;
    std::uint32_t flags=0;unsigned events=0;NativeParticleObserver observer;
    observer.metadata=[&](unsigned field,std::uint32_t value){if(field!=0||value!=7)throw std::runtime_error("native callback result store");++events;};
    update_particle_streams_002e60e0(batch,0x3f800000,flags,{},observer,&callback);
    if(events!=1||batch.count!=7)throw std::runtime_error("native callback boundary");
    events=0;try{update_particle_streams_002e60e0(batch,0,flags,{},observer);throw std::runtime_error("missing callback accepted");}
    catch(const ParticleRefusal&){}if(events)throw std::runtime_error("missing callback mutated output");
    batch.has_dynamic_callback=false;batch.count=-1;
    try{update_particle_streams_002e60e0(batch,0,flags,{});throw std::runtime_error("negative count accepted");}catch(const ParticleRefusal&){}
    batch.count=0;NativeParticleOperation unknown;unknown.kind=22;batch.operations=std::span(&unknown,1);
    try{update_particle_streams_002e60e0(batch,0,flags,{});throw std::runtime_error("unknown operation accepted");}catch(const ParticleRefusal&){}
}
int main(int argc,char**argv)try {
    source_service_cases();
    if(argc<2)return 2;
    const std::string plant=argc>2?argv[2]:"";
    auto files=track_b::files(argv[1],"cap_002e60e0_");if(files.empty())return 2;
    unsigned direct=0,refused=0,partial=0,chunks=0,interpolated=0,clamped=0,wrapped=0;std::array<unsigned,23> kinds{};
    for(const auto& path:files) {
        auto c=track_b::load(path);if(c.pc!=0x2e60e0)throw std::runtime_error("capture target mismatch");
        const auto object=c.before.r[4].w[0];
        auto u=[&](unsigned off){return track_b::read(c.ram,object+off);};
        NativeParticleBatch b;b.count=std::bit_cast<std::int32_t>(u(0));b.preceding=std::bit_cast<std::int32_t>(u(4));b.has_dynamic_callback=u(76)!=0;
        auto bytes=[&](std::uint32_t address)->std::span<std::byte> {if(address>=c.ram.size())throw std::runtime_error("capture RAM pointer");return std::span(c.ram).subspan(address);};
        std::array<std::uint32_t,2> source{},destination{};
        for(unsigned i=0;i<2;++i){source[i]=u(8+i*8);destination[i]=u(40+i*8);b.streams[i]={bytes(source[i]),bytes(destination[i]),u(44+i*8)};}
        std::vector<NativeParticleOperation> operations;
        for(unsigned i=0;i<u(192);++i) {
            const auto at=object+208+i*48;
            auto h=[&](unsigned off){return std::int16_t(track_b::read(c.ram,at+off)&65535);};
            NativeParticleOperation o;o.kind=h(0);o.has_secondary=(track_b::read(c.ram,at)&0x10000)!=0;
            o.primary_stream=h(12);o.primary_offset=h(14);o.secondary_stream=h(20);o.secondary_offset=h(22);
            for(unsigned p=0;p<6;++p)o.parameter[p]=track_b::read(c.ram,at+16+p*4);
            if(o.kind==19)o.table=bytes(track_b::read(c.ram,track_b::read(c.ram,at+36)+16));
            if(o.kind==18)o.table=bytes(track_b::read(c.ram,at+36));
            // Kinds17/18/19 fetch secondary destination independently of flags.
            if(o.kind==17||o.kind==18||o.kind==19)o.has_secondary=true;
            operations.push_back(o);
        }
        b.operations=operations;std::vector<track_b::Store> observed;
        auto metadata=[&](unsigned off,std::uint32_t value) {
            if(off==8)value=value==0?0x70000000:source[0];
            if(off==16)value=value==1?0x70002000:source[1];
            if(off==40)value+=destination[0];if(off==48)value+=destination[1];
            observed.push_back({object+off,value});std::memcpy(c.ram.data()+object+off,&value,4);
        };
        NativeParticleObserver observer;
        observer.metadata=metadata;
        observer.store=[&](std::byte* p,unsigned width,std::uint32_t value){
            const auto address=static_cast<std::uint32_t>(p-c.ram.data());
            if(width!=4&&width!=16)throw std::runtime_error("native store width");
            observed.push_back({address,value});
        };
        auto flags=c.before.fcr31;
        try {
            auto stats=update_particle_streams_002e60e0(b,c.before.f[12],flags,
                {std::span(c.scratch).first(8192),std::span(c.scratch).subspan(8192)},observer);
            if(b.has_dynamic_callback)throw std::runtime_error("dynamic callback fabricated success");
            if(!c.returned())throw std::runtime_error("partial direct capture cannot prove whole function");
            if(plant=="--planted-store")c.ram[destination[0]]^=std::byte{1};
            if(plant=="--planted-order"&&observed.size()>2)std::swap(observed[0],observed[1]);
            const auto sp=c.before.r[29].w[0];std::vector<track_b::Store> expected;
            for(auto s:c.stores)if(s.address<c.ram.size()&&!(s.address>=sp-496&&s.address<sp))expected.push_back(s);
            if(observed!=expected) {
                std::size_t n=0;while(n<observed.size()&&n<expected.size()&&observed[n]==expected[n])++n;
                std::fprintf(stderr,"FAIL store order %s index=%zu counts=%zu/%zu",path.string().c_str(),n,observed.size(),expected.size());
                if(n<observed.size())std::fprintf(stderr," actual=%x:%x",observed[n].address,observed[n].value);
                if(n<expected.size())std::fprintf(stderr," expected=%x:%x",expected[n].address,expected[n].value);std::fprintf(stderr,"\n");return 1;
            }
            for(std::size_t a=0;a<c.ram.size();++a)if(!(a>=sp-496&&a<sp)&&c.ram[a]!=c.exit_ram[a]){std::fprintf(stderr,"FAIL RAM %s address=%zx\n",path.string().c_str(),a);return 1;}
            if(c.scratch!=c.exit_scratch){std::fprintf(stderr,"FAIL workspace %s\n",path.string().c_str());return 1;}
            if(flags!=c.after.fcr31){std::fprintf(stderr,"FAIL numeric flags %s %08x/%08x\n",path.string().c_str(),flags,c.after.fcr31);return 1;}
            // 002E9D18 does not consume v0: this is an effect-only source call.
            // Caller-saved helper residue is deliberately not a native return ABI.
            ++direct;chunks+=stats.chunks;interpolated+=stats.interpolated;clamped+=stats.clamped;wrapped+=stats.animation_wrapped;
            for(unsigned k=0;k<23;++k)kinds[k]+=stats.operations[k];
        }catch(const ParticleRefusal& error) {
            if(!b.has_dynamic_callback||std::string(error.what())!="002E60E0 dynamic heap callback unadmitted"||!observed.empty())throw;
            ++refused;if(!c.returned())++partial;
            std::printf("REFUSED_HEAP_CALLBACK %s callback=%08x ra=%08x exit_pc=%08x stores=%zu\n",path.filename().string().c_str(),u(76),c.before.r[31].w[0],c.after.pc,c.stores.size());
        }
    }
    if(direct==0)throw std::runtime_error("zero direct comparisons");
    std::printf("PASS_PHASE7_STREAM_DIRECT captures=%u callback_refused=%u partial_callback_slices=%u chunks=%u interpolated=%u clamped=%u animation_wrapped=%u kinds=",direct,refused,partial,chunks,interpolated,clamped,wrapped);
    for(unsigned k=0;k<23;++k)if(kinds[k])std::printf("%u:%u,",k,kinds[k]);std::puts("");
    if(plant=="--require-all")return refused?1:0;
    return 0;
}catch(const std::exception&e){std::fprintf(stderr,"FAIL_PHASE7_STREAM %s\n",e.what());return 1;}
