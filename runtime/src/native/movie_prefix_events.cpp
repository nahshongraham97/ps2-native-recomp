#include "mka/movie_prefix_events.hpp"
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>
namespace mka::video {
std::uint32_t movie_prefix_classification(std::uint32_t raw) noexcept {
    if(raw==0x100)return 4;
    if(raw==0x101)return 3;
    if(raw>=0x102&&raw<=0x1af)return 1;
    switch(raw){case 0x1b2:return 0x20;case 0x1b3:return 0x40;case 0x1b5:return 0x10;case 0x1b7:return 0x80;case 0x1b8:return 8;default:return 0;}
}
NativeMoviePrefixEvents::NativeMoviePrefixEvents(std::uint64_t first) noexcept {
    state_.initial_es_begin=state_.next_es_byte=state_.tail_begin=first;
}
std::optional<NativeMoviePrefixEvents> NativeMoviePrefixEvents::create(std::uint64_t first,MoviePrefixFailure& f) noexcept {
    f={};return NativeMoviePrefixEvents(first);
}
NativeMoviePrefixEvents::NativeMoviePrefixEvents(NativeMoviePrefixEvents&& other) noexcept
    :state_(other.state_),failure_(other.failure_),valid_(other.valid_){other.valid_=false;}
NativeMoviePrefixEvents& NativeMoviePrefixEvents::operator=(NativeMoviePrefixEvents&& other) noexcept {
    if(this!=&other){state_=other.state_;failure_=other.failure_;valid_=other.valid_;other.valid_=false;}return *this;
}
bool NativeMoviePrefixEvents::admit(MoviePrefixFailure& f) const noexcept {
    f={};
    if(!valid_){f.kind=MoviePrefixError::invalid_owner;return false;}
    if(state_.failed){f={MoviePrefixError::failed_owner,failure_.kind,state_.next_es_byte,failure_.provided_offset};return false;}
    return true;
}
void NativeMoviePrefixEvents::terminal(MoviePrefixFailure& f,MoviePrefixError kind,std::uint64_t provided) noexcept {
    f={kind,MoviePrefixError::none,state_.next_es_byte,provided};failure_=f;state_.failed=true;
}
std::optional<MoviePrefixBatch> NativeMoviePrefixEvents::feed(std::uint64_t begin,std::span<const std::uint8_t> bytes,MoviePrefixFailure& f) noexcept {
    if(!admit(f))return {};
    if(state_.finished){terminal(f,MoviePrefixError::after_finish,begin);return {};}
    if(begin!=state_.next_es_byte){terminal(f,MoviePrefixError::noncontiguous,begin);return {};}
    if(bytes.size()>std::numeric_limits<std::uint64_t>::max()-begin){terminal(f,MoviePrefixError::offset_overflow,begin);return {};}
    try {
        auto staged=state_;MoviePrefixBatch result{begin,begin+static_cast<std::uint64_t>(bytes.size()),{}};
        std::uint32_t window=0;std::size_t available=staged.tail_size;
        for(std::size_t i=0;i<staged.tail_size;++i)window=(window<<8)|staged.tail[i];
        for(std::size_t i=0;i<bytes.size();++i){
            const auto byte=bytes[i];window=(window<<8)|byte;if(available<4)++available;
            if(available==4&&(window&0xffffff00u)==0x00000100u){
                // Complete window guarantees at least3 preceding source bytes.
                result.events.push_back({begin+static_cast<std::uint64_t>(i)-3,window,movie_prefix_classification(window)});
            }
            if(staged.tail_size<staged.tail.size())staged.tail[staged.tail_size++]=byte;
            else {staged.tail[0]=staged.tail[1];staged.tail[1]=staged.tail[2];staged.tail[2]=byte;}
        }
        staged.next_es_byte=result.es_end;staged.tail_begin=result.es_end-staged.tail_size;
        state_=staged;return result;
    } catch(const std::bad_alloc&){terminal(f,MoviePrefixError::allocation,begin);}
      catch(const std::length_error&){terminal(f,MoviePrefixError::host_capacity,begin);}
    return {};
}
std::optional<MoviePrefixTail> NativeMoviePrefixEvents::finish(MoviePrefixFailure& f) noexcept {
    if(!admit(f))return {};
    try {
        MoviePrefixTail result{state_.tail_begin,state_.next_es_byte,{}};
        result.bytes.assign(state_.tail.begin(),state_.tail.begin()+state_.tail_size);
        state_.finished=true;return result;
    } catch(const std::bad_alloc&){terminal(f,MoviePrefixError::allocation,state_.next_es_byte);}
      catch(const std::length_error&){terminal(f,MoviePrefixError::host_capacity,state_.next_es_byte);}
    return {};
}
std::optional<MoviePrefixState> NativeMoviePrefixEvents::snapshot(MoviePrefixFailure& f) const noexcept {
    f={};if(!valid_){f.kind=MoviePrefixError::invalid_owner;return {};}
    return state_;
}
} // namespace mka::video
