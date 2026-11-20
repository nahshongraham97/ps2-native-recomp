#include "mka/geometry_attributes.hpp"
#include "mka/geometry_companion_quantizer.hpp"
#include <new>
#include <stdexcept>

namespace mka::game {
namespace {
bool fail(GeometryAttributeFailure& f,GeometryAttributeError kind,const char* operation) noexcept {
    f.kind=kind;f.operation=operation;return false;
}
template<class T> bool valid_optional(const std::optional<std::vector<T>>& x,std::size_t count) noexcept {
    return !x||x->size()==count;
}
bool valid(const GeometryAttributeData& data,GeometryAttributeFailure& f) noexcept {
    for(std::size_t i=0;i<data.streams.size();++i){
        const auto& stream=data.streams[i];
        if(stream.primary.size()!=data.vertex_count||!valid_optional(stream.companion,data.vertex_count)){
            f.stream=i;return fail(f,GeometryAttributeError::invalid_extent,"native geometry primary/companion extent");}
    }
    if(!valid_optional(data.first_pairs,data.vertex_count)||!valid_optional(data.second_pairs,data.vertex_count)||
        !valid_optional(data.packed,data.vertex_count)||!valid_optional(data.quadruples,data.vertex_count))
        return fail(f,GeometryAttributeError::invalid_extent,"native geometry optional attribute extent");
    switch(data.extra_mode){
    case GeometryExtraChannels::none:
    case GeometryExtraChannels::current_default:
        if(!data.extra_channels.empty())return fail(f,GeometryAttributeError::invalid_extra_mode,"native geometry default/no-extra route with explicit channels");
        break;
    case GeometryExtraChannels::explicit_streams:
        if(data.extra_channels.empty())return fail(f,GeometryAttributeError::invalid_extra_mode,"native geometry explicit extra channels missing");
        for(const auto& channel:data.extra_channels)if(channel.size()!=data.vertex_count)
            return fail(f,GeometryAttributeError::invalid_extent,"native geometry extra channel extent");
        break;
    default:return fail(f,GeometryAttributeError::invalid_extra_mode,"native geometry unknown extra channel route");
    }
    return true;
}
GeometryStreamEmission emission(MaterialVec3 primary,MaterialVec3 companion) noexcept {
    return {primary,{quantize_geometry_companion_component(companion.x),
        quantize_geometry_companion_component(companion.y),quantize_geometry_companion_component(companion.z)}};
}
}
std::shared_ptr<const NativeGeometryAttributes> NativeGeometryAttributes::create(
    GeometryAttributeData&& data,GeometryAttributeFailure& f) noexcept {
    f={};if(!valid(data,f))return {};
    try{return std::shared_ptr<const NativeGeometryAttributes>(new NativeGeometryAttributes(std::move(data)));}
    catch(const std::bad_alloc&){fail(f,GeometryAttributeError::allocation,"native geometry attribute owner allocation");return {};}
}
std::optional<NativeGeometryVertex> NativeGeometryAttributes::vertex(std::size_t index,
    GeometryPacked fallback,GeometryAttributeFailure& f) const noexcept {
    f={};if(index>=data_.vertex_count){fail(f,GeometryAttributeError::invalid_index,"native geometry source index extent");return {};}
    if(data_.streams.empty()){fail(f,GeometryAttributeError::missing_primary,"native geometry referenced vertex lacks primary stream");return {};}
    try{
        NativeGeometryVertex out{};out.source_index=index;
        out.primary.reserve(data_.streams.size());out.companion.reserve(data_.streams.size());
        for(const auto& stream:data_.streams){
            out.primary.push_back(stream.primary[index]);
            out.companion.push_back(stream.companion?(*stream.companion)[index]:MaterialVec3{{0},{0},{0}});
        }
        if(data_.first_pairs)out.first_pair=(*data_.first_pairs)[index];
        if(data_.second_pairs)out.second_pair=(*data_.second_pairs)[index];
        out.packed=data_.packed?(*data_.packed)[index]:fallback;
        if(data_.extra_mode==GeometryExtraChannels::current_default)out.extra.push_back(fallback);
        else if(data_.extra_mode==GeometryExtraChannels::explicit_streams){
            out.extra.reserve(data_.extra_channels.size());
            for(const auto& channel:data_.extra_channels)out.extra.push_back(channel[index]);
        }
        if(data_.quadruples)out.quadruple=(*data_.quadruples)[index];
        // Raw source31CB14/1C forms3D4CCCCD, followed by C.LT.S.
        // It is a positive source-word threshold; exponent255 remains extended
        // finite in the pinned reference. No host NaN/rounding policy is used.
        for(const auto word:out.quadruple)
            if(!(word.bits&0x80000000u)&&(word.bits&0x7fffffffu)>=0x3d4ccccdu)
                ++out.components_not_below_threshold;
        return out;
    }catch(const std::bad_alloc&){fail(f,GeometryAttributeError::allocation,"native geometry gathered vertex allocation");}
    catch(const std::length_error&){fail(f,GeometryAttributeError::host_capacity,"native geometry gathered vertex host extent");}
    return {};
}
std::optional<NativeGeometryVertexEmission> emit_geometry_vertex_attributes(
    const NativeGeometryVertex& vertex,GeometryAttributeProgram program,GeometryAttributeFailure& f) noexcept {
    f={};
    if(vertex.primary.empty()){fail(f,GeometryAttributeError::missing_primary,"native geometry emission requires actual primary stream");return {};}
    if(vertex.primary.size()!=vertex.companion.size()){
        fail(f,GeometryAttributeError::invalid_extent,"native geometry emission stream pairing");return {};}
    if(program!=GeometryAttributeProgram::with_quad&&program!=GeometryAttributeProgram::pair_banks&&program!=GeometryAttributeProgram::first_pair){
        fail(f,GeometryAttributeError::invalid_program,"native geometry attribute program missing");return {};}
    NativeGeometryVertexEmission out{};
    out.first=emission(vertex.primary[0],vertex.companion[0]);
    out.pair_words[0]=vertex.first_pair[0];out.pair_words[1]=vertex.first_pair[1];out.pair_word_count=2;
    if(program==GeometryAttributeProgram::pair_banks){
        out.pair_words[2]=vertex.second_pair[0];out.pair_words[3]=vertex.second_pair[1];out.pair_word_count=4;
    }
    out.packed=vertex.packed;
    if(program==GeometryAttributeProgram::with_quad)out.quadruple=vertex.quadruple;
    if(vertex.primary.size()>1)out.second=emission(vertex.primary[1],vertex.companion[1]);
    return out;
}
} // namespace mka::game
