#include "mka/retail_audio_statistics.hpp"
#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"
#include <bit>
#include <cmath>
#include <new>
#include <utility>
namespace mka::retail {
namespace {
std::int32_t add_word(std::int32_t a, std::int32_t b) noexcept {
    return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(a) + std::bit_cast<std::uint32_t>(b));
}
}
std::unique_ptr<NativeRetailAudioStatistics> NativeRetailAudioStatistics::create(RetailAudioStatisticsFailure& failure) noexcept {
    failure = {};
    try { return std::unique_ptr<NativeRetailAudioStatistics>(new NativeRetailAudioStatistics); }
    catch (const std::bad_alloc&) { failure.kind = RetailAudioStatisticsError::allocation; }
    catch (...) { failure.kind = RetailAudioStatisticsError::native_exception; }
    return {};
}
bool NativeRetailAudioStatistics::access(RetailAudioStatisticsFailure& failure) const noexcept {
    failure = {};
    if (thread_ == std::this_thread::get_id()) return true;
    failure.kind = RetailAudioStatisticsError::wrong_thread;
    return false;
}
bool NativeRetailAudioStatistics::reset(RetailAudioStatisticsFailure& failure) noexcept {
    if (!access(failure)) return false;
    state_.auxiliary_10 = state_.auxiliary_14 = 0;
    state_.accumulated_bytes = state_.clipped_samples = state_.clipped_magnitude_sum = 0;
    return true;
}
bool NativeRetailAudioStatistics::configure(RetailAudioStatisticsConfiguration configuration,
    RetailAudioStatisticsFailure& failure) noexcept {
    if (!access(failure)) return false;
    state_.configuration = configuration;
    //368CD0 resets these two fields, not the three accumulated statistics.
    state_.auxiliary_10 = state_.auxiliary_14 = 0;
    return true;
}
std::optional<RetailAudioStatisticsObservation> NativeRetailAudioStatistics::observe(RetailAudioStatisticsFailure& failure) const noexcept {
    if (!access(failure)) return {};
    return state_;
}
std::optional<std::int32_t> NativeRetailAudioStatistics::process_samples(std::span<const std::int16_t> samples,
    std::int32_t source_byte_count, RetailAudioStatisticsFailure& failure) noexcept {
    if (!access(failure)) return {};
    const auto count = source_byte_count / 2;
    if (count > 0 && static_cast<std::size_t>(count) > samples.size()) {
        failure.kind = RetailAudioStatisticsError::invalid_sample_span;
        return {};
    }
    auto candidate = state_;
    candidate.accumulated_bytes = add_word(candidate.accumulated_bytes, source_byte_count);
    std::int32_t clipped = 0;
    for (std::int32_t i = 0; i < count; ++i) {
        const auto sample = samples[static_cast<std::size_t>(i)];
        const auto magnitude = sample < 0
            ? std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(0u - std::bit_cast<std::uint16_t>(sample)))
            : sample;
        // Source narrows the negation back to signed16. INT16_MIN therefore
        // stays negative and does not enter this threshold branch.
        if (magnitude > 29999) {
            clipped = add_word(clipped, 1);
            candidate.clipped_magnitude_sum = add_word(candidate.clipped_magnitude_sum, magnitude);
        }
    }
    candidate.clipped_samples = add_word(candidate.clipped_samples, clipped);
    std::int32_t adjustment = 0;
    if (candidate.accumulated_bytes >= candidate.configuration.window_bytes) {
        adjustment = candidate.clipped_samples < 1 ? 1 : 0;
        if (candidate.clipped_samples > 0) {
            // Preserve the original operation grouping and single precision.
            // This is ordinary native finite arithmetic, not a device-FPU model.
            volatile float average = static_cast<float>(candidate.clipped_magnitude_sum) /
                static_cast<float>(candidate.clipped_samples);
            volatile float count_term = static_cast<float>(candidate.clipped_samples) / 200.0f;
            volatile float scaled_count = 10.0f * count_term;
            volatile float magnitude_term = average / 32768.0f;
            volatile float scaled_magnitude = 2.0f * magnitude_term;
            volatile float total = scaled_count + scaled_magnitude;
            const float negative = -total;
            if (!std::isfinite(negative) || static_cast<double>(negative) < -2147483648.0 ||
                static_cast<double>(negative) >= 2147483648.0) {
                failure.kind = RetailAudioStatisticsError::arithmetic_range;
                return {};
            }
            adjustment = static_cast<std::int32_t>(negative);
        }
        candidate.accumulated_bytes = candidate.clipped_samples = candidate.clipped_magnitude_sum = 0;
    }
    state_ = candidate;
    return adjustment;
}
namespace main_binding {
bool construct_retail_audio_statistics(NativeRetailMainState& main, StartupFailure& failure) {
    failure = {};
    if (main.audio_statistics) {
        failure.kind = StartupError::already_initialized;
        failure.operation = "retail audio statistics owner exists";
        return false;
    }
    RetailAudioStatisticsFailure construction;
    auto owner = NativeRetailAudioStatistics::create(construction);
    if (!owner) {
        failure.kind = construction.kind == RetailAudioStatisticsError::allocation
            ? StartupError::allocation : StartupError::native_exception;
        failure.operation = "retail audio statistics construction";
        return false;
    }
    main.audio_statistics = std::move(owner);
    return true;
}
} // namespace main_binding
} // namespace mka::retail
