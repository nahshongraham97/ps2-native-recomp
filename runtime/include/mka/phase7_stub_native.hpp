#pragma once

#include "ps2/fpu.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <functional>
#include <span>
#include <stdexcept>
#include <vector>

namespace mka::phase7 {
// Historical filename retained. Original 002E60E0 is a stream/field update
// routine, not a null-emitter probe. 002E6180 is its indirect-call continuation.
struct ParticleRefusal : std::runtime_error { using std::runtime_error::runtime_error; };
struct NativeParticleStream {
    std::span<std::byte> source, destination;
    std::uint32_t stride{};
};
struct NativeParticleOperation {
    std::int16_t kind{}, primary_stream{}, primary_offset{}, secondary_stream{}, secondary_offset{};
    bool has_secondary{};
    std::array<std::uint32_t,6> parameter{};
    std::span<const std::byte> table;
};
struct NativeParticleBatch {
    std::int32_t count{}, preceding{};
    std::array<NativeParticleStream,2> streams;
    std::span<const NativeParticleOperation> operations;
    bool has_dynamic_callback{};
};
struct NativeParticleObserver {
    // Host pointer and exact store width; qword copy records remain qword stores.
    std::function<void(std::byte*,unsigned,std::uint32_t)> store;
    std::function<void(unsigned,std::uint32_t)> metadata;
    std::function<void(unsigned,std::size_t,std::size_t)> transfer;
};
struct NativeParticleCallbackRequest {
    std::int32_t count{}, preceding{};
    std::array<NativeParticleStream,2> streams;
    std::uint32_t delta_bits{};
};
struct NativeParticleCallbackService {
    virtual ~NativeParticleCallbackService()=default;
    virtual bool update(const NativeParticleCallbackRequest&,std::int32_t& resulting_count)=0;
};
struct ParticleStats { unsigned chunks{}; std::array<unsigned,23> operations{}; unsigned interpolated{}, clamped{}, animation_wrapped{}; };
inline std::uint32_t particle_read(std::span<const std::byte> bytes,std::size_t offset) {
    if(offset>bytes.size() || 4>bytes.size()-offset) throw ParticleRefusal("particle native stream bounds");
    std::uint32_t result; std::memcpy(&result,bytes.data()+offset,4); return result;
}
inline void particle_write(std::span<std::byte> bytes,std::size_t offset,std::uint32_t value,
                           const NativeParticleObserver& observer) {
    if(offset>bytes.size() || 4>bytes.size()-offset) throw ParticleRefusal("particle native output bounds");
    if(observer.store) observer.store(bytes.data()+offset,4,value);
    std::memcpy(bytes.data()+offset,&value,4);
}
inline ParticleStats update_particle_streams_002e60e0(NativeParticleBatch& batch,
    std::uint32_t delta_bits,std::uint32_t& numeric_flags,
    std::array<std::span<std::byte>,2> workspace,const NativeParticleObserver& observer={},
    NativeParticleCallbackService* callback=nullptr) {
    // Dynamic executable heap stubs require their own authenticated native
    // implementation. This refusal occurs before any source-visible effect.
    if(batch.has_dynamic_callback) {
        if(!callback) throw ParticleRefusal("002E60E0 dynamic heap callback unadmitted");
        std::int32_t count{};
        NativeParticleCallbackRequest request{batch.count,batch.preceding,batch.streams,delta_bits};
        for(auto& stream:request.streams) {
            const auto offset=std::uint32_t(batch.preceding)*stream.stride;
            if(offset>stream.destination.size())throw ParticleRefusal("002E60E0 callback endpoint bounds");
            stream.destination=stream.destination.subspan(offset);
        }
        if(!callback->update(request,count))
            throw ParticleRefusal("002E60E0 native callback service refused");
        batch.count=count;
        if(observer.metadata)observer.metadata(0,std::bit_cast<std::uint32_t>(count));
        return {};
    }
    if(batch.count<0 || batch.preceding<0) throw ParticleRefusal("002E60E0 negative batch unproved");
    for(const auto& op:batch.operations) {
        if(op.primary_stream<0 || op.primary_stream>1 || op.primary_offset<0 ||
           (op.has_secondary && (op.secondary_stream<0 || op.secondary_stream>1 || op.secondary_offset<0)))
            throw ParticleRefusal("002E60E0 stream binding unproved");
        if(op.kind==2 && op.has_secondary &&
           ((op.parameter[0]&0xf00u)==0x200u||(op.parameter[0]&0xf00u)==0x500u))
            throw ParticleRefusal("002E60E0 texture secondary service unadmitted");
        switch(op.kind) { case 1:case 2:case 4:case 5:case 6:case 17:case 18:case 19:break;
            default:throw ParticleRefusal("002E60E0 field operation unadmitted"); }
    }
    using namespace ps2;
    auto add=[&](auto a,auto b){return fp::ee::add(numeric_flags,a,b);};
    auto mul=[&](auto a,auto b){return fp::ee::mul(numeric_flags,a,b);};
    auto sub=[&](auto a,auto b){return fp::ee::sub(numeric_flags,a,b);};
    auto div=[&](auto a,auto b){return fp::ee::div(numeric_flags,a,b);};
    auto meta=[&](unsigned field,std::uint32_t value){if(observer.metadata) observer.metadata(field,value);};
    ParticleStats stats;
    const auto original_count=batch.count;
    const auto original_preceding=batch.preceding;
    const auto original_streams=batch.streams;
    // Native workspaces replace source scratch DMA; no PS2 device or polling.
    for(unsigned i=0;i<2;++i) {
        const auto displacement=std::uint32_t(batch.preceding)*batch.streams[i].stride;
        if(displacement>batch.streams[i].destination.size()) throw ParticleRefusal("002E60E0 preceding stream bounds");
        batch.streams[i].destination=batch.streams[i].destination.subspan(displacement);
        meta(40+8*i,displacement); // relative destination cursor
    }
    batch.preceding=0;meta(4,0);meta(8,0);meta(16,1);
    std::size_t processed=0;
    while(processed<std::size_t(original_count)) {
        const auto count=std::min<std::size_t>(32,std::size_t(original_count)-processed);
        ++stats.chunks;batch.count=static_cast<std::int32_t>(count);meta(0,static_cast<std::uint32_t>(count));
        for(unsigned i=0;i<2;++i) {
            const auto bytes=count*batch.streams[i].stride;
            if(bytes>workspace[i].size()) throw ParticleRefusal("002E60E0 native workspace bounds");
            const auto source_offset=processed*batch.streams[i].stride;
            if(source_offset+bytes>original_streams[i].source.size()) throw ParticleRefusal("002E60E0 source bounds");
            // Source DMA transfers floor(bytes/16) qwords. A partial final
            // qword is retained in the workspace, not fabricated from source.
            const auto transferred=(bytes/16)*16;
            std::memmove(workspace[i].data(),original_streams[i].source.data()+source_offset,transferred);
            if(observer.transfer) observer.transfer(i,source_offset,transferred);
            // Original groups four qword loads before the four stores.
            std::size_t n=0;
            while(n+64<=bytes) {
                std::array<std::byte,64> values;std::memcpy(values.data(),workspace[i].data()+n,64);
                for(unsigned q=0;q<4;++q) {
                    auto out=batch.streams[i].destination.subspan(n+q*16);
                    if(out.size()<16) throw ParticleRefusal("002E60E0 copy bounds");
                    if(observer.store) observer.store(out.data(),16,particle_read(values,q*16));
                    std::memcpy(out.data(),values.data()+q*16,16);
                } n+=64;
            }
            while(n+16<=bytes) {
                std::array<std::byte,16> value;std::memcpy(value.data(),workspace[i].data()+n,16);
                auto out=batch.streams[i].destination.subspan(n);
                if(out.size()<16) throw ParticleRefusal("002E60E0 copy bounds");
                if(observer.store) observer.store(out.data(),16,particle_read(value,0));
                std::memcpy(out.data(),value.data(),16);n+=16;
            }
        }
        for(const auto& op:batch.operations) {
            ++stats.operations[op.kind];
            const auto primary=unsigned(op.primary_stream);
            const auto stride=batch.streams[primary].stride;
            auto input=workspace[primary];auto output=batch.streams[primary].destination;
            auto secondary=op.has_secondary?workspace[unsigned(op.secondary_stream)]:std::span<std::byte>{};
            auto secondary_output=op.has_secondary?batch.streams[unsigned(op.secondary_stream)].destination:std::span<std::byte>{};
            const auto secondary_stride=op.has_secondary?batch.streams[unsigned(op.secondary_stream)].stride:0;
            const auto& p=op.parameter;
            const auto scaled0=op.kind==5||op.kind==6?mul(p[0],delta_bits):0;
            const auto scaled1=op.kind==5?mul(p[1],delta_bits):0;
            const auto scaled2=op.kind==5?mul(p[2],delta_bits):0;
            std::uint32_t fade_step=0,fade_start=0,fade_acc=0;
            if(op.kind==17) {
                fade_start=fp::cvt_s_w(p[4]);
                fade_step=div(fp::cvt_s_w(p[4]-p[5]),p[3]);
                fade_acc=add(0,fade_start);
            }
            const auto color_inverse=op.kind==18?div(0x3f800000,p[2]):0;
            for(std::size_t n=0;n<count;++n) {
                const auto at=n*stride+unsigned(op.primary_offset);
                const auto other=n*secondary_stride+unsigned(op.secondary_offset);
                auto read=[&](unsigned lane){return particle_read(input,at+lane*4);};
                auto current=[&](unsigned lane){return particle_read(output,at+lane*4);};
                auto write=[&](unsigned lane,std::uint32_t value){particle_write(output,at+lane*4,value,observer);};
                switch(op.kind) {
                case 1:write(0,add(read(0),delta_bits));break;
                case 2: {
                    // The admitted secondary source is a bound stream. Texture
                    // service variants at 002EA650 remain an explicit blocker.
                    if(!op.has_secondary) throw ParticleRefusal("002E60E0 secondary stream missing");
                    for(unsigned lane=0;lane<3;++lane) {
                        const auto acc=add(0,read(lane));
                        write(lane,fp::ee::madd(numeric_flags,acc,particle_read(secondary,other+lane*4),delta_bits));
                    } break;
                }
                case 4:for(unsigned lane=0;lane<3;++lane) write(lane,mul(current(lane),p[lane]));break;
                case 5:write(0,add(read(0),scaled0));write(1,add(read(1),scaled1));write(2,add(read(2),scaled2));break;
                case 6:write(0,add(read(0),scaled0));break;
                case 17: {
                    for(unsigned lane=0;lane<4;++lane) write(lane,read(lane));
                    auto phase=sub(particle_read(secondary_output,other),p[2]);
                    phase=fp::min(fp::max(phase,0),p[3]);
                    const auto faded=fp::ee::msub(numeric_flags,fade_acc,phase,fade_step);
                    write(3,fp::cvt_w_s(faded));break;
                }
                case 18: {
                    const auto phase=mul(particle_read(secondary_output,other),color_inverse);
                    const auto index=std::bit_cast<std::int32_t>(fp::cvt_w_s(phase));
                    const auto limit=std::bit_cast<std::int32_t>(p[3]);
                    const auto base=std::size_t(p[4])*16;
                    const bool within=fp::key(phase)<=fp::key(fp::cvt_s_w(p[3]));
                    numeric_flags=(numeric_flags&~(1u<<23))|(within?(1u<<23):0u);
                    if(!within) {
                        ++stats.clamped;for(unsigned lane=0;lane<4;++lane) write(lane,particle_read(op.table,base+std::size_t(limit)*16+lane*4));
                    } else {
                        if(index<0) throw ParticleRefusal("002E6EC0 negative interpolation index");
                        ++stats.interpolated;
                        const auto fraction=sub(phase,fp::cvt_s_w(std::uint32_t(index)));
                        const auto inverse=sub(0x3f800000,fraction);
                        for(unsigned lane=0;lane<4;++lane) {
                            const auto a=particle_read(op.table,base+std::size_t(index)*16+lane*4);
                            const auto b=particle_read(op.table,base+(std::size_t(index)+1)*16+lane*4);
                            const auto acc=mul(fraction,fp::cvt_s_w(b));
                            write(lane,fp::cvt_w_s(fp::ee::madd(numeric_flags,acc,inverse,fp::cvt_s_w(a)))&255u);
                        }
                    }break;
                }
                case 19: {
                    auto index=std::bit_cast<std::int32_t>(fp::cvt_w_s(div(particle_read(secondary_output,other),p[2])));
                    const auto limit=std::int32_t(std::int16_t(p[3]));
                    if(index>=limit) {
                        const auto mode=std::int16_t(p[4]);
                        if(mode==1) { ++stats.animation_wrapped;index=limit==0?index:index%limit; }
                        else if(mode==0) index=limit-1;
                    }
                    index=std::bit_cast<std::int32_t>(std::uint32_t(index)+std::uint32_t(std::int32_t(std::int16_t(p[3]>>16))));
                    if(index<0) throw ParticleRefusal("002E95A0 negative table index");
                    const auto x=particle_read(op.table,std::size_t(index)*8);
                    const auto y=particle_read(op.table,std::size_t(index)*8+4);
                    write(0,x);write(1,y);break;
                }
                }
            }
        }
        processed+=count;
        if(processed<std::size_t(original_count)) for(unsigned i=0;i<2;++i) {
            batch.streams[i].destination=batch.streams[i].destination.subspan(count*batch.streams[i].stride);
            meta(40+8*i,static_cast<std::uint32_t>((std::size_t(original_preceding)+processed)*batch.streams[i].stride));
        }
    }
    batch.count=original_count;batch.preceding=original_preceding;batch.streams=original_streams;
    meta(4,std::uint32_t(original_preceding));meta(8,2);meta(16,3);meta(40,0);meta(48,0);meta(0,std::uint32_t(original_count));
    return stats;
}
} // namespace mka::phase7
