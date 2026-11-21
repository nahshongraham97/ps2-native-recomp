#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <thread>

namespace mka::retail {

enum class RetailRandomError { none, wrong_thread };
struct RetailRandomFailure {
    RetailRandomError kind{RetailRandomError::none};
    const char* operation{""};
};

// Original retail 26D4A0/26D530/26D8E0 state and 185C30 range reduction.
// The 624 words are the generator algorithm's state, not a device allocation.
class NativeRetailRandom final {
public:
    NativeRetailRandom() noexcept = default; // Original BSS: zero words and counters.
    NativeRetailRandom(const NativeRetailRandom&) = delete;
    NativeRetailRandom& operator=(const NativeRetailRandom&) = delete;

    bool seed(std::uint32_t value, RetailRandomFailure&) noexcept;
    std::optional<std::uint32_t> next(RetailRandomFailure&) noexcept;
    std::optional<std::uint16_t> low16_range(
        std::uint32_t limit, RetailRandomFailure&) noexcept;

private:
    bool access(RetailRandomFailure&, const char* operation) const noexcept;
    void twist() noexcept;

    std::array<std::uint32_t, 624> state_{};
    std::uint32_t cursor_{};
    std::uint32_t consumed_{};
    const std::thread::id owner_thread_{std::this_thread::get_id()};
};

} // namespace mka::retail
