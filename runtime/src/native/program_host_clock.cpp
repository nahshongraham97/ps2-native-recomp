#include "mka/program_host_clock.hpp"
#include <mutex>
#include <chrono>
#include <limits>
#include <new>
#include <utility>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
namespace mka::sound {
struct NativeProgramHostClock::Impl {
    std::int64_t frequency,origin,last;
    ProgramHostTickReader reader;
    std::mutex mutex;
    Impl(std::int64_t f,std::int64_t first,ProgramHostTickReader r):frequency(f),origin(first),last(first),reader(std::move(r)){}
};
NativeProgramHostClock::NativeProgramHostClock(std::unique_ptr<Impl> state):state_(std::move(state)){}
NativeProgramHostClock::~NativeProgramHostClock()=default;
namespace {
thread_local bool sampling=false;
struct SamplingGuard {SamplingGuard(){sampling=true;}~SamplingGuard(){sampling=false;}};
std::optional<std::int64_t> sample(ProgramHostTickReader& reader,ProgramHostClockFailure& f){
    if(sampling){f={ProgramHostClockError::reentry,0};return {};}
    SamplingGuard guard;
    try {
        auto tick=reader(f);
        if(!tick){if(f.kind==ProgramHostClockError::none)f.kind=ProgramHostClockError::query;return {};}
        if(f.kind!=ProgramHostClockError::none)return {};
        return tick;
    }catch(...){f={ProgramHostClockError::native_exception,0};return {};}
}
}
std::shared_ptr<NativeProgramHostClock> NativeProgramHostClock::create_with_reader(std::int64_t frequency,ProgramHostTickReader reader,ProgramHostClockFailure& f){
    f={};if(frequency<=0){f.kind=ProgramHostClockError::invalid_frequency;return {};}
    if(!reader){f.kind=ProgramHostClockError::invalid_source;return {};}
    auto first=sample(reader,f);if(!first)return {};
    try {return std::shared_ptr<NativeProgramHostClock>(new NativeProgramHostClock(std::make_unique<Impl>(frequency,*first,std::move(reader))));}
    catch(const std::bad_alloc&){f.kind=ProgramHostClockError::allocation_failed;return {};}
    catch(...){f.kind=ProgramHostClockError::native_exception;return {};}
}
std::shared_ptr<NativeProgramHostClock> NativeProgramHostClock::create(ProgramHostClockFailure& f){
    f={};
#ifdef _WIN32
    LARGE_INTEGER frequency;
    if(!QueryPerformanceFrequency(&frequency)){f={ProgramHostClockError::query,static_cast<std::int64_t>(GetLastError())};return {};}
    try {return create_with_reader(frequency.QuadPart,[](ProgramHostClockFailure& failure)->std::optional<std::int64_t>{
        LARGE_INTEGER ticks;if(!QueryPerformanceCounter(&ticks)){failure={ProgramHostClockError::query,static_cast<std::int64_t>(GetLastError())};return {};}
        return ticks.QuadPart;
    },f);}
    catch(const std::bad_alloc&){f.kind=ProgramHostClockError::allocation_failed;return {};}
    catch(...){f.kind=ProgramHostClockError::native_exception;return {};}
#else
    try {return create_with_reader(1000000000,[](ProgramHostClockFailure& failure)->std::optional<std::int64_t>{
        // Native steady counter expressed in integer nanoseconds. Bounds are
        // checked before conversion rather than relying on duration_cast wrap.
        using Clock=std::chrono::steady_clock;using Period=Clock::period;
        using Wide=unsigned __int128;
        const auto count=static_cast<__int128>(Clock::now().time_since_epoch().count());
        const auto magnitude=static_cast<Wide>(count<0?-count:count);
        const auto scale=Wide(Period::num)*Wide(1000000000);
        if(magnitude&&scale>~Wide(0)/magnitude){failure.kind=ProgramHostClockError::overflow;return {};}
        const auto nanos=magnitude*scale/Wide(Period::den);
        const auto limit=Wide(std::numeric_limits<std::int64_t>::max())+(count<0?1:0);
        if(nanos>limit){failure.kind=ProgramHostClockError::overflow;return {};}
        if(count<0&&nanos==limit)return std::numeric_limits<std::int64_t>::min();
        return count<0?-static_cast<std::int64_t>(nanos):static_cast<std::int64_t>(nanos);
    },f);}
    catch(const std::bad_alloc&){f.kind=ProgramHostClockError::allocation_failed;return {};}
    catch(...){f.kind=ProgramHostClockError::native_exception;return {};}
#endif
}
std::optional<ProgramClockWord> NativeProgramHostClock::read(ProgramHostClockFailure& f) noexcept{
    f={};
    if(sampling){f.kind=ProgramHostClockError::reentry;return {};}
    try {
        std::lock_guard lock(state_->mutex);
        auto tick=sample(state_->reader,f);if(!tick)return {};
        if(*tick<state_->last){f.kind=ProgramHostClockError::regressed;return {};}
        // Signed64 endpoints differ by at most2^64-1; multiplication by1000
        // fits unsigned128. The final source word intentionally wraps.
        using Wide=unsigned __int128;
        const auto elapsed=static_cast<Wide>(static_cast<__int128>(*tick)-static_cast<__int128>(state_->origin));
        const auto milliseconds=elapsed*Wide(1000)/static_cast<Wide>(state_->frequency);
        auto word=static_cast<std::uint32_t>(milliseconds);
        if(!word)word=1;
        state_->last=*tick;
        return ProgramClockWord{word};
    }catch(...){f={ProgramHostClockError::native_exception,0};return {};}
}
} // namespace mka::sound
