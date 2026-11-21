#include "mka/retail_audio_statistics.hpp"
#include <array>
#include <bit>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <thread>
#include <vector>
using namespace mka::retail;
namespace {
bool fail_allocation = false;
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
}
void* operator new(std::size_t n) {
    if (fail_allocation) throw std::bad_alloc();
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
int main() { try {
    RetailAudioStatisticsFailure f;
    fail_allocation = true;
    auto missing = NativeRetailAudioStatistics::create(f);
    fail_allocation = false;
    check(!missing && f.kind == RetailAudioStatisticsError::allocation, "failed constructor has no published owner");
    auto owner = NativeRetailAudioStatistics::create(f);
    auto initial = owner->observe(f);
    check(initial && initial->configuration.window_bytes == 0x2b11 && initial->configuration.source_parameter_8 == 1 &&
        initial->configuration.source_parameter_c == 0x10 && initial->accumulated_bytes == 0 &&
        initial->clipped_samples == 0 && initial->clipped_magnitude_sum == 0, "exact original constructor data");
    const std::array<std::int16_t, 4> samples{30000, -30000, 29999, INT16_MIN};
    check(owner->configure({8, 1, 0x10}, f), "explicit original configuration");
    auto value = owner->process_samples(samples, 6, f);
    auto state = owner->observe(f);
    check(value && *value == 0 && state->accumulated_bytes == 6 && state->clipped_samples == 2 &&
        state->clipped_magnitude_sum == 60000, "below window preserves exact threshold counters");
    check(!owner->process_samples({}, 2, f) && f.kind == RetailAudioStatisticsError::invalid_sample_span &&
        owner->observe(f)->accumulated_bytes == 6, "invalid native input cannot partially update prior state");
    check(owner->configure({8, 9, 17}, f) && owner->observe(f)->clipped_samples == 2,
        "configure resets auxiliary fields, retains rolling statistics");
    value = owner->process_samples(std::span(samples).last(1), 2, f);
    check(value && *value == -1, "source INT16_MIN narrowing excludes it; grouped finite formula truncates negative result");
    state = owner->observe(f);
    check(state->accumulated_bytes == 0 && state->clipped_samples == 0 && state->clipped_magnitude_sum == 0,
        "completed window clears only rolling fields");
    const std::array<std::int16_t, 2> quiet{29999, INT16_MIN};
    check(owner->configure({4, 1, 0x10}, f), "quiet window");
    value = owner->process_samples(quiet, 4, f);
    check(value && *value == 1, "no clipped samples produces actual source adjustment1");
    check(owner->configure({INT_MAX, 1, 0x10}, f), "large observation window");
    value = owner->process_samples({}, -3, f);
    check(value && *value == 0 && owner->observe(f)->accumulated_bytes == -3, "negative signed length scans no samples");
    check(owner->reset(f), "explicit full reset");
    std::vector<std::int16_t> peak(65535, 32767);
    check(owner->process_samples(peak, 131070, f) == 0 && owner->process_samples(peak, 131070, f) == 0,
        "actual long-window native sample scan");
    const std::uint32_t sum = std::uint32_t{65535} * 32767u * 2u;
    state = owner->observe(f);
    check(state->accumulated_bytes == 262140 && state->clipped_samples == 131070 &&
        state->clipped_magnitude_sum == std::bit_cast<std::int32_t>(sum), "source integer arithmetic has defined word wrap, no signed C++ overflow");
    check(owner->reset(f) && owner->observe(f)->clipped_samples == 0 &&
        owner->observe(f)->configuration.window_bytes == INT_MAX, "reset preserves configuration while clearing all rolling fields");
    const std::array<std::int16_t, 1> odd{30000};
    check(owner->process_samples(odd, 3, f) == 0 && owner->observe(f)->clipped_samples == 1 &&
        owner->observe(f)->accumulated_bytes == 3, "odd byte count scans signed division truncated toward zero");
    bool wrong_thread = false;
    std::thread worker([&] {
        RetailAudioStatisticsFailure other;
        wrong_thread = !owner->reset(other) && other.kind == RetailAudioStatisticsError::wrong_thread &&
            !owner->observe(other) && other.kind == RetailAudioStatisticsError::wrong_thread;
    }); worker.join(); check(wrong_thread && owner->observe(f)->accumulated_bytes == 3, "owning-thread check preserves live state");
    std::puts("PASS native retail audio statistics finite/integer/failure checks; no sample producer, game or endpoint");
    return 0;
} catch (const std::exception& e) {
    fail_allocation = false; std::fprintf(stderr, "FAIL audio statistics: %s\n", e.what()); return 1;
}}
