#pragma once

#include <cstdint>
#include <vector>

namespace mka::retail {

enum class RetailModeError { none, allocation };
struct RetailModeFailure {
    RetailModeError kind = RetailModeError::none;
    const char* operation = "";
};

// The retail interaction-context history at 64C40C/69F010. Construct this
// owner at the source 185EC0 reset point; it starts in the cleared state.
// Main reentry, main game mode and transition flags are separate state.
class NativeRetailModeState final {
public:
    void clear() noexcept;                           // 185EC0
    bool push(std::int32_t, RetailModeFailure&) noexcept; // 185D60
    void pop() noexcept;                             // 185D30
    std::int32_t top() const noexcept;                // 185E90
    std::int32_t top_past_21() const noexcept;        // 185DF0
    bool contains(std::int32_t) const noexcept;      // 185E40

private:
    std::vector<std::int32_t> history_;
};

} // namespace mka::retail
