#include "mka/movie_timestamp_association.hpp"
#include "mka/movie_clock_math.hpp"
#include <array>
#include <limits>
#include <new>
#include <exception>
#include <stdexcept>
#include <type_traits>
namespace mka::video {
namespace {
using Wide=__int128;
constexpr std::array<std::int64_t,9> rates{1,23976,24000,25000,29970,30000,50000,59940,60000};
static_assert(std::is_nothrow_copy_assignable_v<MovieTimestampAssociationState>);
struct Guard {bool& flag;explicit Guard(bool& f):flag(f){flag=true;}~Guard(){flag=false;}};
bool fail(MovieTimestampAssociationFailure& f,MovieTimestampAssociationError e) noexcept {f.kind=e;return false;}
bool store(Wide v,std::int64_t& out,MovieTimestampAssociationFailure& f) noexcept {
    if(v<INT64_MIN||v>INT64_MAX)return fail(f,MovieTimestampAssociationError::overflow);
    out=std::int64_t(v);return true;
}
bool valid(const MovieVideoPrefixObserved& p) noexcept {
    const auto& s=p.pes;constexpr std::uint64_t timestamp_limit=(std::uint64_t(1)<<33)-1;
    return p.prefix.raw_codeword==0x100&&p.prefix.classification==4&&
        p.prefix.es_prefix_first_byte<=UINT64_MAX-4&&s.stream_id>=0xe0&&s.stream_id<=0xef&&
        s.packet_offset<s.payload_offset&&s.payload_offset<s.packet_end&&s.es_begin<s.es_end&&
        s.packet_end-s.payload_offset==s.es_end-s.es_begin&&
        s.es_begin<=p.prefix.es_prefix_first_byte&&p.prefix.es_prefix_first_byte<s.es_end&&
        (!s.pts33||*s.pts33<=timestamp_limit)&&(!s.dts33||*s.dts33<=timestamp_limit)&&
        p.quarter_pts==s.quarter_pts();
}
}
NativeMovieTimestampAssociation::NativeMovieTimestampAssociation(std::string key,
        std::shared_ptr<const MovieSourceObservationIdentity> identity):key_(std::move(key)),identity_(std::move(identity)){}
NativeMovieTimestampAssociation::~NativeMovieTimestampAssociation(){if(busy_||owner_!=std::this_thread::get_id())std::terminate();}
bool NativeMovieTimestampAssociation::access(MovieTimestampAssociationFailure& f) const noexcept {
    f={};if(owner_!=std::this_thread::get_id())return fail(f,MovieTimestampAssociationError::wrong_thread);
    if(busy_)return fail(f,MovieTimestampAssociationError::reentry);return true;
}
std::unique_ptr<NativeMovieTimestampAssociation> NativeMovieTimestampAssociation::create(std::string_view key,
        std::shared_ptr<const MovieSourceObservationIdentity> id,MovieTimestampAssociationFailure& f) noexcept {
    f={};if(key.empty()||key.find('\0')!=key.npos||!id){fail(f,MovieTimestampAssociationError::invalid_source);return {};}
    try{return std::unique_ptr<NativeMovieTimestampAssociation>(new NativeMovieTimestampAssociation(std::string(key),std::move(id)));}
    catch(const std::bad_alloc&){fail(f,MovieTimestampAssociationError::allocation);}
    catch(const std::length_error&){fail(f,MovieTimestampAssociationError::host_capacity);}return {};
}
std::optional<MovieTimestampAssociationSnapshot> NativeMovieTimestampAssociation::snapshot(MovieTimestampAssociationFailure& f) const noexcept {
    if(!access(f))return {};Guard guard(busy_);
    try{return MovieTimestampAssociationSnapshot{key_,identity_,state_};}
    catch(const std::bad_alloc&){fail(f,MovieTimestampAssociationError::allocation);return {};}
}
std::optional<MovieTimestampAssociationResult> NativeMovieTimestampAssociation::update(std::string_view key,
        const std::shared_ptr<const MovieSourceObservationIdentity>& id,
        const std::optional<MovieVideoPrefixObserved>& query,std::uint8_t rate,
        std::uint16_t temporal,std::uint8_t coding,bool changed,MovieTimestampAssociationFailure& f) noexcept {
    if(!access(f))return {};Guard guard(busy_);
    if(key!=key_||!id||id!=identity_){fail(f,MovieTimestampAssociationError::foreign_source);return {};}
    if(query&&!valid(*query)){fail(f,MovieTimestampAssociationError::invalid_prefix);return {};}
    auto next=state_;bool initial=false,new_record=false;std::optional<std::int64_t> adjusted;
    const auto raw=query?query->quarter_pts:std::nullopt;
    if(raw){
        if(rate<1||rate>8){fail(f,MovieTimestampAssociationError::unsupported_rate);return {};}
        if(temporal>1023){fail(f,MovieTimestampAssociationError::invalid_temporal);return {};}
        if(coding<1||coding>3){fail(f,MovieTimestampAssociationError::invalid_coding);return {};}
        if(!next.origin){
            MovieClockMathFailure math;auto interval=scale_movie_clock(temporal,22500000,rates[rate],math);
            if(!interval){fail(f,MovieTimestampAssociationError::overflow);return {};}
            const Wide value=Wide(*raw)-*interval;std::int64_t origin;
            if(!store(value<0?0:value,origin,f))return {};next.origin=origin;initial=true;
        }
        Wide value=Wide(*raw)-*next.origin;if(value<0)value=0;
        new_record=!next.cached_raw_timestamp22500||*next.cached_raw_timestamp22500!=*raw;
        if(new_record){
            next.cached_raw_timestamp22500=raw;next.cached_query=query;
            next.base_temporal=temporal;next.offset=0;next.max_delta=coding==3?1:0;
        }else{
            if(changed){
                std::int64_t offset;if(!store(Wide(next.offset)+next.max_delta+1,offset,f))return {};
                next.base_temporal=0;next.offset=offset;next.max_delta=0;
            }
            const Wide delta=Wide(temporal)-next.base_temporal;
            if(delta>next.max_delta){if(!store(delta,next.max_delta,f))return {};}
            // offset is signed64; delta is within +/-1023. Product is below
            // 2^89, avoiding a narrowed intermediate before truncating division.
            value+=((Wide(next.offset)+delta)*22500000)/rates[rate];
            if(value<0)value=0;
        }
        std::int64_t final;if(!store(value,final,f))return {};adjusted=final;
    }
    try{
        MovieTimestampAssociationResult result{{key_,identity_,next},query,raw,adjusted,initial,new_record};
        if(raw)state_=next;return result;
    }catch(const std::bad_alloc&){fail(f,MovieTimestampAssociationError::allocation);return {};}
}
}
