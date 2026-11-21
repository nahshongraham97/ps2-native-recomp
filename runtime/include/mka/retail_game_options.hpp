#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <thread>

namespace mka::retail {

enum class RetailGameOptionsError {
    none,
    wrong_thread,
    truncated_selected_record
};
struct RetailGameOptionsFailure {
    RetailGameOptionsError kind{RetailGameOptionsError::none};
    const char* operation{""};
};

// Native values of the source 64EDF0 mode and six distinct 164130 stores.
// The address suffixes identify the original stores, not a guest memory map.
struct RetailGameOptionsSnapshot {
    std::uint32_t live_mode{};
    std::uint32_t word_660380{};
    std::uint32_t word_660384{};
    std::uint32_t word_66038c{};
    std::uint32_t word_660388{};
    std::uint32_t word_660390{};
    std::uint32_t word_660394{};
};

class NativeRetailGameOptions final {
public:
    NativeRetailGameOptions() noexcept = default;
    NativeRetailGameOptions(const NativeRetailGameOptions&) = delete;
    NativeRetailGameOptions& operator=(const NativeRetailGameOptions&) = delete;

    bool validate_current_thread(RetailGameOptionsFailure&) const noexcept;
    // nullopt is the original null pointer. A present span is the selected
    // source record; the source reads at most its first two bytes.
    // The source clears the mode before checking the pointer or first byte.
    bool parse_selected_record_from_3433c0(
        std::optional<std::span<const std::uint8_t>> selected_record,
        RetailGameOptionsFailure&) noexcept;
    bool publish_six_words_from_164130(RetailGameOptionsFailure&) noexcept;
    std::optional<RetailGameOptionsSnapshot> observe(
        RetailGameOptionsFailure&) const noexcept;

private:
    RetailGameOptionsSnapshot values_{};
    const std::thread::id owner_thread_{std::this_thread::get_id()};
};

} // namespace mka::retail
