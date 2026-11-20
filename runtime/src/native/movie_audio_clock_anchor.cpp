#include "mka/movie_audio_clock_anchor.hpp"
#include <numeric>
#include <limits>
#include <new>
#include <exception>
namespace mka::video {
namespace {
struct Guard {bool& busy;explicit Guard(bool& b):busy(b){busy=true;}~Guard(){busy=false;}};
bool fail(MovieAudioClockAnchorFailure& f,MovieAudioClockAnchorError k) noexcept{f.kind=k;return false;}
bool valid(const sound::AudioQueueStatus& q,const sound::AudioClockReading& c) noexcept{return q.capacity_frames&&std::uint64_t(q.padding_frames)+q.writable_frames==q.capacity_frames&&q.padding_frames<=q.submitted_frames&&q.reset_epoch==c.reset_epoch&&c.frequency;}
bool zero(const sound::AudioQueueStatus& q,const sound::AudioClockReading& c) noexcept{return !q.started&&!q.padding_frames&&!q.submitted_frames&&!c.position;}
std::optional<MovieClockRational> rational(const sound::AudioClockReading& c) noexcept{
    auto n=c.position,d=c.frequency;if(n>INT64_MAX||d>INT64_MAX){const auto g=std::gcd(n,d);n/=g;d/=g;}
    if(n>INT64_MAX||d>INT64_MAX)return {};return MovieClockRational{std::int64_t(n),std::int64_t(d)};
}
}
NativeMovieAudioClockAnchor::NativeMovieAudioClockAnchor(std::string k,std::shared_ptr<const MovieSourceObservationIdentity> s,std::shared_ptr<const MovieNativeAudioClockAnchor> a,sound::AudioClockReading c):key_(std::move(k)),source_(std::move(s)),anchor_(std::move(a)),last_(c){}
NativeMovieAudioClockAnchor::~NativeMovieAudioClockAnchor(){if(owner_!=std::this_thread::get_id()||busy_)std::terminate();}
std::unique_ptr<NativeMovieAudioClockAnchor> NativeMovieAudioClockAnchor::create(std::string_view key,std::shared_ptr<const MovieSourceObservationIdentity> id,const sound::AudioQueueStatus& q,const sound::AudioClockReading& c,MovieAudioClockAnchorFailure& f) noexcept{
    f={};if(key.empty()||key.find('\0')!=key.npos||!id){fail(f,MovieAudioClockAnchorError::invalid_source);return {};}
    if(!valid(q,c)){fail(f,MovieAudioClockAnchorError::invalid_status);return {};}
    if(!zero(q,c)){fail(f,MovieAudioClockAnchorError::invalid_origin);return {};}
    if(!rational(c)){fail(f,MovieAudioClockAnchorError::overflow);return {};}
    try{return std::unique_ptr<NativeMovieAudioClockAnchor>(new NativeMovieAudioClockAnchor(std::string(key),std::move(id),std::make_shared<const MovieNativeAudioClockAnchor>(),c));}catch(const std::bad_alloc&){fail(f,MovieAudioClockAnchorError::allocation);return {};}
}
std::optional<MovieAnchoredAudioClockSample> NativeMovieAudioClockAnchor::observe(const sound::AudioQueueStatus& q,const sound::AudioClockReading& c,MovieAudioClockAnchorFailure& f) noexcept{
    f={};if(owner_!=std::this_thread::get_id()){fail(f,MovieAudioClockAnchorError::wrong_thread);return {};}
    if(busy_){fail(f,MovieAudioClockAnchorError::reentry);return {};}Guard guard(busy_);
    if(!valid(q,c)){fail(f,MovieAudioClockAnchorError::invalid_status);return {};}
    if(c.reset_epoch!=last_.reset_epoch){fail(f,MovieAudioClockAnchorError::epoch);return {};}
    if(c.frequency!=last_.frequency){fail(f,MovieAudioClockAnchorError::frequency);return {};}
    if(c.position<last_.position){fail(f,MovieAudioClockAnchorError::regression);return {};}
    const auto time=rational(c);if(!time){fail(f,MovieAudioClockAnchorError::overflow);return {};}
    try{MovieAnchoredAudioClockSample result{key_,source_,anchor_,*time,true,q.started,c,false};last_=c;active_=q.started;return result;}catch(const std::bad_alloc&){fail(f,MovieAudioClockAnchorError::allocation);return {};}
}
std::optional<MovieAnchoredAudioClockSample> NativeMovieAudioClockAnchor::reset(const sound::AudioQueueStatus& q,const sound::AudioClockReading& c,MovieAudioClockAnchorFailure& f) noexcept{
    f={};if(owner_!=std::this_thread::get_id()){fail(f,MovieAudioClockAnchorError::wrong_thread);return {};}
    if(busy_){fail(f,MovieAudioClockAnchorError::reentry);return {};}Guard guard(busy_);
    if(c.frequency!=last_.frequency){fail(f,MovieAudioClockAnchorError::frequency);return {};}
    if(last_.reset_epoch==UINT64_MAX||c.reset_epoch!=last_.reset_epoch+1){fail(f,MovieAudioClockAnchorError::epoch);return {};}
    if(!valid(q,c)||!zero(q,c)){fail(f,MovieAudioClockAnchorError::invalid_origin);return {};}
    const auto old_time=rational(last_);if(!old_time||!rational(c)){fail(f,MovieAudioClockAnchorError::overflow);return {};}
    try{auto fresh=std::make_shared<const MovieNativeAudioClockAnchor>();MovieAnchoredAudioClockSample old{key_,source_,anchor_,*old_time,true,active_,last_,true};anchor_=std::move(fresh);last_=c;active_=false;return old;}catch(const std::bad_alloc&){fail(f,MovieAudioClockAnchorError::allocation);return {};}
}
} // namespace mka::video
