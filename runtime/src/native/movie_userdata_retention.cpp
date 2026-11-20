#include "mka/movie_userdata_retention.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <new>
#include <stdexcept>
#include <tuple>
#include <utility>
namespace mka::video {
namespace {
using E=MovieUserdataRetentionError;
bool fail(MovieUserdataRetentionFailure& f,E e) noexcept {f.kind=e;return false;}
bool same(const MoviePictureCodingExtension& a,const MoviePictureCodingExtension& b) noexcept {
    if(std::tie(a.f_code,a.intra_dc_precision,a.picture_structure,a.top_field_first,a.frame_pred_frame_dct,
        a.concealment_motion_vectors,a.q_scale_type,a.intra_vlc_format,a.alternate_scan,a.repeat_first_field,
        a.chroma_420_type,a.progressive_frame)!=std::tie(b.f_code,b.intra_dc_precision,b.picture_structure,
        b.top_field_first,b.frame_pred_frame_dct,b.concealment_motion_vectors,b.q_scale_type,b.intra_vlc_format,
        b.alternate_scan,b.repeat_first_field,b.chroma_420_type,b.progressive_frame))return false;
    if(a.composite_display.has_value()!=b.composite_display.has_value())return false;
    if(!a.composite_display)return true;const auto& x=*a.composite_display;const auto& y=*b.composite_display;
    return std::tie(x.v_axis,x.field_sequence,x.sub_carrier,x.burst_amplitude,x.sub_carrier_phase)==
        std::tie(y.v_axis,y.field_sequence,y.sub_carrier,y.burst_amplitude,y.sub_carrier_phase);
}
bool whole_userdata(const MovieHeaderUnit& u) noexcept {
    const auto* p=std::get_if<MovieUserDataHeader>(&u.payload);
    if(!p||u.bytes.size()<4||u.consumed_bytes!=u.bytes.size()||p->payload.size()!=u.bytes.size()-4||
        u.bytes[0]!=0||u.bytes[1]!=0||u.bytes[2]!=1||u.bytes[3]!=0xb2||
        !std::equal(p->payload.begin(),p->payload.end(),u.bytes.begin()+4))return false;
    // A complete B2 unit excludes the first next prefix. Even a terminal
    // 000001 would form that prefix with the known following delimiter byte.
    for(std::size_t i=4;u.bytes.size()-i>=3;++i)
        if(u.bytes[i]==0&&u.bytes[i+1]==0&&u.bytes[i+2]==1)return false;
    return true;
}
}
NativeMovieUserdataRetention::NativeMovieUserdataRetention(std::string key,
        std::shared_ptr<const MovieSourceObservationIdentity> id,bool bound) noexcept
    :state_{std::move(key),std::move(id),bound,false,0,{}},thread_(std::this_thread::get_id()){}
std::optional<NativeMovieUserdataRetention> NativeMovieUserdataRetention::create(std::string_view key,
        std::shared_ptr<const MovieSourceObservationIdentity> id,bool bound,MovieUserdataRetentionFailure& f) noexcept {
    f={};if(key.empty()||key.find('\0')!=std::string_view::npos||!id){fail(f,E::invalid_source);return {};}
    try{return NativeMovieUserdataRetention(std::string(key),std::move(id),bound);}
    catch(const std::bad_alloc&){fail(f,E::allocation);}catch(const std::length_error&){fail(f,E::host_capacity);}return {};
}
NativeMovieUserdataRetention::NativeMovieUserdataRetention(NativeMovieUserdataRetention&& other) noexcept
    :state_(std::move(other.state_)),thread_(other.thread_),valid_(other.valid_){other.valid_=false;}
NativeMovieUserdataRetention& NativeMovieUserdataRetention::operator=(NativeMovieUserdataRetention&& other) noexcept {
    if(this!=&other){state_=std::move(other.state_);thread_=other.thread_;valid_=other.valid_;other.valid_=false;}return *this;
}
bool NativeMovieUserdataRetention::owner(MovieUserdataRetentionFailure& f) const noexcept {
    f={};if(!valid_)return fail(f,E::invalid_owner);
    if(std::this_thread::get_id()!=thread_)return fail(f,E::wrong_thread);return true;
}
bool NativeMovieUserdataRetention::source(std::string_view key,
        const std::shared_ptr<const MovieSourceObservationIdentity>& id,MovieUserdataRetentionFailure& f) const noexcept {
    if(!owner(f))return false;if(key!=state_.source_key||!id||id!=state_.identity)return fail(f,E::foreign_source);return true;
}
bool NativeMovieUserdataRetention::set_bound(std::string_view key,
        const std::shared_ptr<const MovieSourceObservationIdentity>& id,bool bound,MovieUserdataRetentionFailure& f) noexcept {
    if(!source(key,id,f))return false;state_.bound=bound;return true;
}
bool NativeMovieUserdataRetention::coding_extension(std::string_view key,
        const std::shared_ptr<const MovieSourceObservationIdentity>& id,const MovieHeaderUnit& unit,
        MovieUserdataRetentionFailure& f) noexcept {
    if(!source(key,id,f))return false;
    const auto* supplied=std::get_if<MoviePictureCodingExtension>(&unit.payload);
    if(!supplied||unit.bytes.size()<5||unit.consumed_bytes!=unit.bytes.size())return fail(f,E::invalid_unit);
    auto parsed=parse_movie_header_unit(unit.bytes);
    if(parsed.status==MovieHeaderUnitStatus::error){
        return fail(f,parsed.failure.kind==MovieHeaderUnitError::allocation?E::allocation:E::host_capacity);
    }
    if(parsed.status!=MovieHeaderUnitStatus::parsed||!parsed.unit||parsed.unit->consumed_bytes!=unit.consumed_bytes)
        return fail(f,E::invalid_unit);
    const auto* actual=std::get_if<MoviePictureCodingExtension>(&parsed.unit->payload);
    if(!actual||!same(*supplied,*actual))return fail(f,E::invalid_unit);
    const auto structure=actual->picture_structure;
    if(structure<1||structure>3)return fail(f,E::invalid_unit);
    if(structure==3){state_.phase=false;state_.field_counter=0;state_.bytes.clear();return true;}
    if(state_.field_counter==std::numeric_limits<std::uint64_t>::max())return fail(f,E::overflow);
    const bool phase=(state_.field_counter&1)!=0;const auto count=state_.field_counter+1;
    if(!phase)state_.bytes.clear();state_.phase=phase;state_.field_counter=count;return true;
}
bool NativeMovieUserdataRetention::append(std::string_view key,
        const std::shared_ptr<const MovieSourceObservationIdentity>& id,MovieUserdataPrimaryKind kind,
        const MovieHeaderUnit& unit,MovieUserdataRetentionFailure& f) noexcept {
    if(!source(key,id,f))return false;
    if(kind!=MovieUserdataPrimaryKind::initial&&kind!=MovieUserdataPrimaryKind::sequence&&
       kind!=MovieUserdataPrimaryKind::gop&&kind!=MovieUserdataPrimaryKind::picture)return fail(f,E::invalid_kind);
    if(!whole_userdata(unit))return fail(f,E::invalid_unit);
    if(kind!=MovieUserdataPrimaryKind::picture||!state_.bound)return true;
    constexpr std::array<std::uint8_t,8> separator{0,0,1,0xb2,0x32,0x6e,0x64,0};
    const auto extra=state_.phase?separator.size():0;
    if(extra>state_.bytes.max_size()-state_.bytes.size()||
       unit.bytes.size()>state_.bytes.max_size()-state_.bytes.size()-extra)return fail(f,E::host_capacity);
    try{
        // Build complete replacement first: source used-before-copy ordering
        // is intentionally replaced by a safe native allocation transaction.
        auto next=state_.bytes;next.reserve(next.size()+extra+unit.bytes.size());
        if(state_.phase)next.insert(next.end(),separator.begin(),separator.end());
        next.insert(next.end(),unit.bytes.begin(),unit.bytes.end());state_.bytes.swap(next);return true;
    }catch(const std::bad_alloc&){return fail(f,E::allocation);}
     catch(const std::length_error&){return fail(f,E::host_capacity);}
}
std::optional<MovieUserdataRetentionState> NativeMovieUserdataRetention::snapshot(MovieUserdataRetentionFailure& f) const noexcept {
    if(!owner(f))return {};
    try{return state_;}catch(const std::bad_alloc&){fail(f,E::allocation);}
    catch(const std::length_error&){fail(f,E::host_capacity);}return {};
}
} // namespace mka::video
