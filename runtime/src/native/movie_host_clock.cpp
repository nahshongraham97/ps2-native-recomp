#include "mka/movie_host_clock.hpp"
#include <limits>
#include <new>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
namespace mka::video {
namespace {
using Wide=unsigned __int128;
bool fail(MovieHostClockFailure& f,MovieHostClockError kind) noexcept {f.kind=kind;return false;}
struct Guard {bool& value;explicit Guard(bool& v):value(v){value=true;}~Guard(){value=false;}};
Wide gcd(Wide a,Wide b) noexcept {while(b){auto r=a%b;a=b;b=r;}return a;}
}
struct NativeMovieHostClock::Impl {
    std::string key;std::shared_ptr<const MovieSourceObservationIdentity> identity;
    std::int64_t frequency,anchor=0,last=0;Wide accumulated=0;
    MovieHostTickReader reader;MovieHostClockState state=MovieHostClockState::not_started;
    std::thread::id owner=std::this_thread::get_id();bool busy=false,native=false;
    Impl(std::string k,std::shared_ptr<const MovieSourceObservationIdentity> id,std::int64_t f,MovieHostTickReader r)
        :key(std::move(k)),identity(std::move(id)),frequency(f),reader(std::move(r)){}
};
NativeMovieHostClock::NativeMovieHostClock(std::unique_ptr<Impl> p):impl_(std::move(p)){}
NativeMovieHostClock::~NativeMovieHostClock(){if(impl_->owner!=std::this_thread::get_id()||impl_->busy)std::terminate();}
std::unique_ptr<NativeMovieHostClock> NativeMovieHostClock::create_with_reader(std::string_view key,
    std::shared_ptr<const MovieSourceObservationIdentity> id,std::int64_t frequency,MovieHostTickReader reader,MovieHostClockFailure& f) noexcept {
    f={};if(key.empty()||key.find('\0')!=key.npos||!id){fail(f,MovieHostClockError::invalid_source);return {};}
    if(frequency<=0){fail(f,MovieHostClockError::invalid_frequency);return {};}
    if(!reader){fail(f,MovieHostClockError::invalid_reader);return {};}
    try {return std::unique_ptr<NativeMovieHostClock>(new NativeMovieHostClock(std::make_unique<Impl>(std::string(key),std::move(id),frequency,std::move(reader))));}
    catch(const std::bad_alloc&){fail(f,MovieHostClockError::allocation);return {};}
}
std::unique_ptr<NativeMovieHostClock> NativeMovieHostClock::create(std::string_view key,
    std::shared_ptr<const MovieSourceObservationIdentity> id,MovieHostClockFailure& f) noexcept {
#ifdef _WIN32
    f={};LARGE_INTEGER frequency;if(!QueryPerformanceFrequency(&frequency)){f={MovieHostClockError::query,GetLastError()};return {};}
    try {auto owner=create_with_reader(key,std::move(id),frequency.QuadPart,[]{return std::optional<std::int64_t>{};},f);if(owner)owner->impl_->native=true;return owner;}
    catch(const std::bad_alloc&){fail(f,MovieHostClockError::allocation);return {};}
#else
    (void)key;(void)id;f={MovieHostClockError::unsupported_host,0};return {};
#endif
}
bool NativeMovieHostClock::access(std::string_view key,const std::shared_ptr<const MovieSourceObservationIdentity>& id,MovieHostClockFailure& f) const noexcept {
    f={};if(impl_->owner!=std::this_thread::get_id())return fail(f,MovieHostClockError::wrong_thread);
    if(impl_->busy)return fail(f,MovieHostClockError::reentry);
    if(key!=impl_->key||!id||id!=impl_->identity)return fail(f,MovieHostClockError::foreign_source);return true;
}
std::optional<std::int64_t> NativeMovieHostClock::tick(MovieHostClockFailure& f) noexcept {
    try {std::optional<std::int64_t> n;
#ifdef _WIN32
        if(impl_->native){LARGE_INTEGER ticks;if(!QueryPerformanceCounter(&ticks)){f={MovieHostClockError::query,GetLastError()};return {};}n=ticks.QuadPart;}
        else
#endif
        n=impl_->reader();
        if(!n){fail(f,MovieHostClockError::query);return {};}
        if(impl_->state!=MovieHostClockState::not_started&&*n<impl_->last){fail(f,MovieHostClockError::regression);return {};}return n;
    }catch(...){fail(f,MovieHostClockError::query);return {};}
}
bool NativeMovieHostClock::begin(std::string_view key,std::shared_ptr<const MovieSourceObservationIdentity> id,MovieHostClockFailure& f) noexcept {
    if(!access(key,id,f))return false;Guard guard(impl_->busy);
    if(impl_->state!=MovieHostClockState::not_started)return fail(f,MovieHostClockError::invalid_state);
    auto n=tick(f);if(!n)return false;impl_->anchor=impl_->last=*n;impl_->state=MovieHostClockState::running;return true;
}
bool NativeMovieHostClock::pause(std::string_view key,std::shared_ptr<const MovieSourceObservationIdentity> id,MovieHostClockFailure& f) noexcept {
    if(!access(key,id,f))return false;Guard guard(impl_->busy);
    if(impl_->state==MovieHostClockState::paused)return true;
    if(impl_->state!=MovieHostClockState::running)return fail(f,MovieHostClockError::invalid_state);
    auto n=tick(f);if(!n)return false;const auto delta=Wide(__int128(*n)-impl_->anchor);
    if(delta>~Wide(0)-impl_->accumulated)return fail(f,MovieHostClockError::overflow);
    const auto total=impl_->accumulated+delta;if(total/gcd(total,Wide(impl_->frequency))>Wide(INT64_MAX))return fail(f,MovieHostClockError::overflow);
    impl_->accumulated=total;impl_->last=*n;impl_->state=MovieHostClockState::paused;return true;
}
bool NativeMovieHostClock::resume(std::string_view key,std::shared_ptr<const MovieSourceObservationIdentity> id,MovieHostClockFailure& f) noexcept {
    if(!access(key,id,f))return false;Guard guard(impl_->busy);
    if(impl_->state==MovieHostClockState::running)return true;
    if(impl_->state!=MovieHostClockState::paused)return fail(f,MovieHostClockError::invalid_state);
    auto n=tick(f);if(!n)return false;impl_->anchor=impl_->last=*n;impl_->state=MovieHostClockState::running;return true;
}
bool NativeMovieHostClock::stop(std::string_view key,std::shared_ptr<const MovieSourceObservationIdentity> id,MovieHostClockFailure& f) noexcept {
    if(!access(key,id,f))return false;Guard guard(impl_->busy);
    if(impl_->state==MovieHostClockState::stopped)return true;
    if(impl_->state==MovieHostClockState::not_started)return fail(f,MovieHostClockError::invalid_state);
    if(impl_->state==MovieHostClockState::running){auto n=tick(f);if(!n)return false;const auto delta=Wide(__int128(*n)-impl_->anchor);if(delta>~Wide(0)-impl_->accumulated)return fail(f,MovieHostClockError::overflow);const auto total=impl_->accumulated+delta;if(total/gcd(total,Wide(impl_->frequency))>Wide(INT64_MAX))return fail(f,MovieHostClockError::overflow);impl_->accumulated=total;impl_->last=*n;}
    impl_->state=MovieHostClockState::stopped;return true;
}
std::optional<MovieHostClockReading> NativeMovieHostClock::read(std::string_view key,std::shared_ptr<const MovieSourceObservationIdentity> id,MovieHostClockFailure& f) noexcept {
    if(!access(key,id,f))return {};Guard guard(impl_->busy);
    if(impl_->state==MovieHostClockState::not_started){fail(f,MovieHostClockError::invalid_state);return {};}
    auto total=impl_->accumulated;auto last=impl_->last;
    if(impl_->state==MovieHostClockState::running){auto n=tick(f);if(!n)return {};const auto delta=Wide(__int128(*n)-impl_->anchor);if(delta>~Wide(0)-total){fail(f,MovieHostClockError::overflow);return {};}total+=delta;last=*n;}
    const auto divisor=gcd(total,Wide(impl_->frequency));const auto numerator=total/divisor,denominator=Wide(impl_->frequency)/divisor;
    if(numerator>Wide(INT64_MAX)){fail(f,MovieHostClockError::overflow);return {};}
    try {MovieHostClockReading result{impl_->key,impl_->identity,{std::int64_t(numerator),std::int64_t(denominator)},impl_->state};impl_->last=last;return result;}
    catch(const std::bad_alloc&){fail(f,MovieHostClockError::allocation);return {};}
}
} // namespace mka::video
