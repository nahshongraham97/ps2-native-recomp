#pragma once
#include "mka/retail_crt.hpp"
#include <cstdint>
#include <map>
#include <optional>
#include <thread>
namespace mka::retail {
enum class RetailMainTimingError {none,crt,foreign_clock,not_started,parse,range,allocation,native_exception};
struct RetailMainTimingFailure {RetailMainTimingError kind=RetailMainTimingError::none;RetailCrtFailure crt;};
struct NativeRetailStopwatchObservation {bool running;std::uint64_t started_microseconds,elapsed_microseconds;};
// Main-thread-owned source stopwatch slots, sharing the retained startup CRT.
// Native microsecond deltas do not claim the original kernel absolute epoch.
class NativeRetailMainTiming final {
public:
    bool start(unsigned,NativeRetailCrt&,RetailMainTimingFailure&) noexcept;
    std::optional<std::uint64_t> stop(unsigned,NativeRetailCrt&,RetailMainTimingFailure&) noexcept;
    std::optional<std::uint64_t> coarse_milliseconds(NativeRetailCrt&,RetailMainTimingFailure&) noexcept;
    std::optional<NativeRetailStopwatchObservation> observe(unsigned)const noexcept;
    bool initialize_parsed_setting(RetailMainTimingFailure&) noexcept;
    std::optional<std::uint32_t> parsed_setting()const noexcept{if(std::this_thread::get_id()!=thread_)return {};return parsed_;}
private:
    // Startup owns the CRT for the full main-state lifetime; no pointer escapes.
    const std::thread::id thread_=std::this_thread::get_id();
    NativeRetailCrt* clock_=nullptr;
    std::map<unsigned,NativeRetailStopwatchObservation> slots_;
    std::optional<std::uint32_t> parsed_;
    std::optional<std::uint64_t> sample(NativeRetailCrt&,RetailMainTimingFailure&)const noexcept;
};
} // namespace mka::retail
