#include "mka/movie_header_boundary.hpp"
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>
namespace mka::video {
namespace {
bool fail(MovieHeaderBoundaryFailure& f,MovieHeaderBoundaryError kind) noexcept {f.kind=kind;return false;}
}
NativeMovieHeaderBoundary::NativeMovieHeaderBoundary(std::string key,
        std::shared_ptr<const MovieSourceObservationIdentity> id,std::uint64_t first) noexcept
    :state_{std::move(key),std::move(id),first,first,first,{},{},{}},owner_(std::this_thread::get_id()) {}
std::optional<NativeMovieHeaderBoundary> NativeMovieHeaderBoundary::create(std::string_view key,
        std::shared_ptr<const MovieSourceObservationIdentity> id,std::uint64_t first,MovieHeaderBoundaryFailure& f) noexcept {
    f={};
    if(!id||key.empty()||key.find('\0')!=std::string_view::npos){fail(f,MovieHeaderBoundaryError::invalid_source);return {};}
    try {return NativeMovieHeaderBoundary(std::string(key),std::move(id),first);}
    catch(const std::bad_alloc&){fail(f,MovieHeaderBoundaryError::allocation);}
    catch(const std::length_error&){fail(f,MovieHeaderBoundaryError::host_capacity);}
    return {};
}
NativeMovieHeaderBoundary::NativeMovieHeaderBoundary(NativeMovieHeaderBoundary&& other) noexcept
    :state_(std::move(other.state_)),owner_(other.owner_),valid_(other.valid_){other.valid_=false;}
NativeMovieHeaderBoundary& NativeMovieHeaderBoundary::operator=(NativeMovieHeaderBoundary&& other) noexcept {
    if(this!=&other){state_=std::move(other.state_);owner_=other.owner_;valid_=other.valid_;other.valid_=false;}return *this;
}
bool NativeMovieHeaderBoundary::admit(MovieHeaderBoundaryFailure& f) const noexcept {
    f={};if(!valid_)return fail(f,MovieHeaderBoundaryError::invalid_owner);
    if(std::this_thread::get_id()!=owner_)return fail(f,MovieHeaderBoundaryError::wrong_thread);
    return true;
}
bool NativeMovieHeaderBoundary::source(std::string_view key,
        const std::shared_ptr<const MovieSourceObservationIdentity>& id,MovieHeaderBoundaryFailure& f) const noexcept {
    if(!admit(f))return false;
    if(key!=state_.source_key||!id||id!=state_.identity)return fail(f,MovieHeaderBoundaryError::foreign_source);
    return true;
}
bool NativeMovieHeaderBoundary::append(std::string_view key,
        const std::shared_ptr<const MovieSourceObservationIdentity>& id,const MovieVideoEsObserved& input,
        MovieHeaderBoundaryFailure& f) noexcept {
    if(!source(key,id,f))return false;
    if(input.es_begin!=state_.es_end)return fail(f,MovieHeaderBoundaryError::noncontiguous);
    if(input.bytes.size()>std::numeric_limits<std::uint64_t>::max()-input.es_begin)return fail(f,MovieHeaderBoundaryError::overflow);
    if(input.es_end<input.es_begin||input.es_end-input.es_begin!=input.bytes.size())return fail(f,MovieHeaderBoundaryError::malformed_range);
    if(input.bytes.size()>state_.bytes.max_size()-state_.bytes.size())return fail(f,MovieHeaderBoundaryError::host_capacity);
    try {
        // uint8_t copies/moves cannot throw: vector::insert allocation failure
        // retains prior contents. All following scalar publication is infallible.
        state_.bytes.insert(state_.bytes.end(),input.bytes.begin(),input.bytes.end());
        state_.es_end=input.es_end;return true;
    }catch(const std::bad_alloc&){return fail(f,MovieHeaderBoundaryError::allocation);}
     catch(const std::length_error&){return fail(f,MovieHeaderBoundaryError::host_capacity);}
}
std::uint32_t NativeMovieHeaderBoundary::classify(std::uint64_t offset) const noexcept {
    const auto at=static_cast<std::size_t>(offset-state_.es_begin);const auto* b=state_.bytes.data()+at;
    const auto raw=(std::uint32_t(b[0])<<24)|(std::uint32_t(b[1])<<16)|(std::uint32_t(b[2])<<8)|b[3];
    return movie_prefix_classification(raw);
}
std::optional<std::uint64_t> NativeMovieHeaderBoundary::first(std::uint32_t mask) const noexcept {
    if(state_.bytes.size()<4)return {};
    for(std::size_t i=0;i<=state_.bytes.size()-4;++i){const auto offset=state_.es_begin+i;if(classify(offset)&mask)return offset;}
    return {};
}
std::optional<std::uint64_t> NativeMovieHeaderBoundary::last(std::uint32_t mask) const noexcept {
    if(state_.bytes.size()<4)return {};
    for(std::size_t i=state_.bytes.size()-4;;--i){const auto offset=state_.es_begin+i;if(classify(offset)&mask)return offset;if(!i)break;}
    return {};
}
bool NativeMovieHeaderBoundary::refresh(std::uint32_t initial,std::optional<std::uint64_t> candidate) const noexcept {
    if(!candidate||*candidate==state_.es_begin||*candidate<state_.es_begin||*candidate>=state_.es_end||state_.es_end-*candidate<4)return true;
    const auto type=classify(*candidate);
    if(type==0x40||type==0x80)return false;
    if(type==8){if(!(initial&0x40))return false;const auto f=first(8);return !f||f==candidate;}
    if(type==4){if(!(initial&0x48))return false;const auto f=first(4);return !f||f==candidate;}
    return true;
}
std::optional<MovieHeaderBoundaryProposal> NativeMovieHeaderBoundary::propose(MovieHeaderBoundaryFailure& f) noexcept {
    if(!admit(f))return {};
    MovieHeaderBoundaryProposal result;result.es_begin=state_.es_begin;
    if(state_.bytes.empty()){result.empty=true;return result;}
    const auto initial=first(0xce);
    if(!initial||*initial!=state_.es_begin){
        result.count=initial?*initial-state_.es_begin:(state_.bytes.size()>3?state_.bytes.size()-3:0);
        result.other=result.count>0;result.need_lookahead=result.count==0;return result;
    }
    result.count=4;result.classification=classify(*initial);
    if(result.classification&0x80)return result;
    auto candidate=state_.cached_boundary;auto cached=state_.cached_boundary;auto searched=state_.searched_available_end;
    if(refresh(result.classification,candidate)){
        candidate.reset();
        if(!searched||*searched!=state_.es_end){candidate=last(0xcc);cached=candidate;searched=state_.es_end;}
    }
    const auto publish=[&]() noexcept {state_.cached_boundary=cached;state_.searched_available_end=searched;return result;};
    if(!candidate){result.need_lookahead=true;return publish();}
    const auto type=classify(*candidate);
    std::optional<std::uint64_t> refined;
    if(type==8&&(result.classification&0x40))refined=first(8);
    else if(type==4&&(result.classification&0x48))refined=first(4);
    else {result.count=*candidate-state_.es_begin;result.need_lookahead=result.count==0;return publish();}
    if(!refined||refined==candidate){result.need_lookahead=true;return publish();}
    result.count=*refined-state_.es_begin;result.need_lookahead=result.count==0;return publish();
}
bool NativeMovieHeaderBoundary::consume(std::string_view key,
        const std::shared_ptr<const MovieSourceObservationIdentity>& id,MovieHeaderConsumeKind kind,
        std::uint64_t amount,MovieHeaderBoundaryFailure& f) noexcept {
    if(!source(key,id,f))return false;
    if(kind!=MovieHeaderConsumeKind::parser&&kind!=MovieHeaderConsumeKind::skip)return fail(f,MovieHeaderBoundaryError::invalid_consume_kind);
    if(amount>state_.bytes.size())return fail(f,MovieHeaderBoundaryError::range);
    if(amount>std::numeric_limits<std::uint64_t>::max()-state_.es_begin)return fail(f,MovieHeaderBoundaryError::overflow);
    if(!amount)return true;
    const auto next=state_.es_begin+amount;
    // Erasing trivial bytes cannot allocate or throw.
    state_.bytes.erase(state_.bytes.begin(),state_.bytes.begin()+static_cast<std::size_t>(amount));state_.es_begin=next;
    if(!state_.cached_boundary||*state_.cached_boundary<next||*state_.cached_boundary>=state_.es_end){
        state_.cached_boundary.reset();state_.searched_available_end.reset();
    }
    return true;
}
std::optional<MovieHeaderBoundaryState> NativeMovieHeaderBoundary::snapshot(MovieHeaderBoundaryFailure& f) const noexcept {
    if(!admit(f))return {};
    try {return state_;}
    catch(const std::bad_alloc&){fail(f,MovieHeaderBoundaryError::allocation);}
    catch(const std::length_error&){fail(f,MovieHeaderBoundaryError::host_capacity);}
    return {};
}
} // namespace mka::video
