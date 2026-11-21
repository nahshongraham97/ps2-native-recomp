#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <thread>
namespace mka::retail {
enum class RetailAudioStatisticsError { none, allocation, native_exception, wrong_thread, invalid_sample_span, arithmetic_range };
struct RetailAudioStatisticsFailure { RetailAudioStatisticsError kind{RetailAudioStatisticsError::none}; };
struct RetailAudioStatisticsConfiguration {
    std::int32_t window_bytes{0x2b11};
    // Source configuration+8/+C are retained, but these routines do not
    // consume them. Their transport/format interpretation remains unbound.
    std::uint32_t source_parameter_8{1}, source_parameter_c{0x10};
};
struct RetailAudioStatisticsObservation {
    RetailAudioStatisticsConfiguration configuration;
    std::uint32_t auxiliary_10{}, auxiliary_14{};
    std::int32_t accumulated_bytes{}, clipped_samples{}, clipped_magnitude_sum{};
};
// Actual368C10/368BD0/368CD0/368D30/368F00 ordinary algorithm owner.
// Main-thread ownership. Typed samples are supplied explicitly; this API does
// not claim any raw-device buffer format, capture endpoint or volume binding.
class NativeRetailAudioStatistics final {
public:
    static std::unique_ptr<NativeRetailAudioStatistics> create(RetailAudioStatisticsFailure&) noexcept;
    NativeRetailAudioStatistics(const NativeRetailAudioStatistics&)=delete;
    NativeRetailAudioStatistics& operator=(const NativeRetailAudioStatistics&)=delete;
    bool reset(RetailAudioStatisticsFailure&) noexcept;
    bool configure(RetailAudioStatisticsConfiguration, RetailAudioStatisticsFailure&) noexcept;
    std::optional<RetailAudioStatisticsObservation> observe(RetailAudioStatisticsFailure&) const noexcept;
    // source_byte_count is signed source metadata: scan trunc(count/2)
    // samples only when positive. Required typed span is checked before edits.
    // Result is signed source adjustment, not an unsigned gain or success flag.
    std::optional<std::int32_t> process_samples(std::span<const std::int16_t>,
        std::int32_t source_byte_count, RetailAudioStatisticsFailure&) noexcept;
private:
    NativeRetailAudioStatistics()=default;
    bool access(RetailAudioStatisticsFailure&) const noexcept;
    const std::thread::id thread_{std::this_thread::get_id()};
    RetailAudioStatisticsObservation state_{};
};
} // namespace mka::retail
