#include "mka/retail_main_timing.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_platform.hpp"
#include "native_retail_startup.hpp"
#include <charconv>
#include <cmath>
#include <limits>
#include <new>
#include <string_view>
namespace mka::retail {
std::optional<std::uint64_t> NativeRetailMainTiming::sample(NativeRetailCrt& crt,RetailMainTimingFailure& f)const noexcept{
    if(!crt.validate_current_thread(f.crt)){f.kind=RetailMainTimingError::crt;return {};}
    if(std::this_thread::get_id()!=thread_){f.kind=RetailMainTimingError::crt;f.crt.kind=RetailCrtError::wrong_thread;return {};}
    if(clock_&&clock_!=&crt){f.kind=RetailMainTimingError::foreign_clock;return {};}
    const auto ns=crt.now_nanoseconds(f.crt);if(!ns){f.kind=RetailMainTimingError::crt;return {};}
    return *ns/1000u; // Explicit native microsecond scale, one shared CRT epoch.
}
bool NativeRetailMainTiming::start(unsigned slot,NativeRetailCrt& crt,RetailMainTimingFailure& f)noexcept{
    f={};const auto now=sample(crt,f);if(!now)return false;
    try{slots_.insert_or_assign(slot,NativeRetailStopwatchObservation{true,*now,0});clock_=&crt;return true;}
    catch(const std::bad_alloc&){f.kind=RetailMainTimingError::allocation;}catch(...){f.kind=RetailMainTimingError::native_exception;}return false;
}
std::optional<std::uint64_t> NativeRetailMainTiming::coarse_milliseconds(
    NativeRetailCrt& crt,RetailMainTimingFailure& f) noexcept {
    f={};
    const auto microseconds=sample(crt,f);
    if(!microseconds) return {};
    clock_=&crt;
    return *microseconds/1000u;
}
std::optional<std::uint64_t> NativeRetailMainTiming::stop(unsigned slot,NativeRetailCrt& crt,RetailMainTimingFailure& f)noexcept{
    f={};const auto now=sample(crt,f);if(!now)return {};
    const auto found=slots_.find(slot);if(found==slots_.end()){f.kind=RetailMainTimingError::not_started;return {};}
    const auto start=found->second.started_microseconds;
    const auto elapsed=*now<start?start-*now:*now-start;
    found->second.running=false;found->second.elapsed_microseconds=elapsed;return elapsed;
}
std::optional<NativeRetailStopwatchObservation> NativeRetailMainTiming::observe(unsigned slot)const noexcept{
    if(std::this_thread::get_id()!=thread_)return {};
    const auto found=slots_.find(slot);if(found==slots_.end())return {};return found->second;
}
bool NativeRetailMainTiming::initialize_parsed_setting(RetailMainTimingFailure& f)noexcept{
    f={};if(std::this_thread::get_id()!=thread_){f.kind=RetailMainTimingError::crt;f.crt.kind=RetailCrtError::wrong_thread;return false;}
    // Original ELF64C3C4 ->5F93F8: "0.084\0"; format5F9610:"%f\0".
    // The known source literal is parsed as binary32, not replaced with84.
    constexpr std::string_view source_text="0.084";
    float value{};const auto result=std::from_chars(source_text.data(),source_text.data()+source_text.size(),value,std::chars_format::general);
    if(result.ec!=std::errc{}||result.ptr!=source_text.data()+source_text.size()){f.kind=RetailMainTimingError::parse;return false;}
    // Separate volatile stores preserve original MUL.S then ADD.S rounding.
    volatile float multiplied=value*1000.0f;const float rounded=multiplied+0.5f;
    if(!std::isfinite(rounded)||rounded<0.0f||static_cast<double>(rounded)>=4294967296.0){f.kind=RetailMainTimingError::range;return false;}
    std::uint32_t word;
    if(rounded<2147483648.0f)word=static_cast<std::uint32_t>(rounded);
    else word=static_cast<std::uint32_t>(rounded-2147483648.0f)|0x80000000u;
    parsed_=word;return true;
}
namespace main_binding {
RetailStartupGate initialize_retail_timer_and_viewport(NativeRetailMainState& main,StartupFailure& f) {
    f={};
    if(!main.startup.crt) {
        f.kind=StartupError::crt; f.operation="225DB0 native shared clock missing";
        return RetailStartupGate::native_failure;
    }
    if(!main.startup.crt->validate_current_thread(f.crt)) {
        f.kind=StartupError::crt; f.operation="225DB0 native shared clock owner";
        return RetailStartupGate::native_failure;
    }
    if(!main.startup.platform) {
        f.kind=StartupError::platform; f.operation="225DB0 native presentation owner missing";
        return RetailStartupGate::native_failure;
    }
    const RetailLogicalViewport viewport{640,448,832};
    if(!main.startup.platform->configure_logical_viewport(viewport,f.platform)) {
        f.kind=StartupError::platform; f.operation="225DB0 actual logical viewport publication";
        return RetailStartupGate::native_failure;
    }
    main.logical_viewport=viewport;
    // Timer/interrupt/EE-priority backend is removed. Native time is sampled
    // live from the same retained steady clock, never an IRQ counter copy.
    return RetailStartupGate::ready;
}
std::optional<std::uint64_t> read_retail_coarse_milliseconds(
    NativeRetailMainState& main,StartupFailure& f) {
    f={};
    if(!main.startup.crt) {
        f.kind=StartupError::main_timing; f.operation="native coarse clock225DB0 owner not initialized"; return {};
    }
    if(!main.startup.crt->validate_current_thread(f.crt)) {
        f.kind=StartupError::crt; f.operation="native coarse clock owner"; return {};
    }
    if(!main.logical_viewport) {
        f.kind=StartupError::main_timing; f.operation="native coarse clock225DB0 viewport not initialized"; return {};
    }
    const auto result=main.main_timing.coarse_milliseconds(*main.startup.crt,f.timing);
    if(!result) { f.kind=StartupError::main_timing; f.operation="native coarse clock sample"; }
    return result;
}
bool start_retail_stopwatch(NativeRetailMainState& main,unsigned slot,StartupFailure& f){
    f={};if(!main.startup.crt){f.kind=StartupError::main_timing;f.timing.kind=RetailMainTimingError::crt;f.timing.crt.kind=RetailCrtError::invalid_capability;f.operation="retail stopwatch CRT missing";return false;}
    if(main.main_timing.start(slot,*main.startup.crt,f.timing))return true;
    f.kind=StartupError::main_timing;f.operation="retail stopwatch start";return false;
}
std::optional<std::uint32_t> stop_retail_stopwatch(NativeRetailMainState& main,unsigned slot,StartupFailure& f){
    f={};if(!main.startup.crt){f.kind=StartupError::main_timing;f.timing.kind=RetailMainTimingError::crt;f.timing.crt.kind=RetailCrtError::invalid_capability;f.operation="retail stopwatch CRT missing";return {};}
    auto value=main.main_timing.stop(slot,*main.startup.crt,f.timing);
    if(!value){f.kind=StartupError::main_timing;f.operation="retail stopwatch stop";return {};}
    // Original12296C consumer assigns1860D0's64bit result to a32bit word.
    return static_cast<std::uint32_t>(*value);
}
bool initialize_retail_parsed_timing_setting(NativeRetailMainState& main,StartupFailure& f){
    f={};if(!main.startup.crt){f.kind=StartupError::crt;f.crt.kind=RetailCrtError::invalid_capability;f.operation="retail parsed setting CRT missing";return false;}
    if(!main.startup.crt->validate_current_thread(f.crt)){f.kind=StartupError::crt;f.operation="retail parsed setting CRT readiness";return false;}
    if(main.main_timing.initialize_parsed_setting(f.timing))return true;
    f.kind=StartupError::main_timing;f.operation="retail source parsed setting";return false;
}
} // namespace main_binding
} // namespace mka::retail
