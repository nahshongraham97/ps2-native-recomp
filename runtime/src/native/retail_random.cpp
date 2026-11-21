#include "mka/retail_random.hpp"

namespace mka::retail {

namespace {
constexpr std::uint32_t kUpper = 0x80000000u;
constexpr std::uint32_t kLower = 0x7fffffffu;
constexpr std::uint32_t kMatrix = 0x9908b0dfu;
constexpr std::size_t kStateWords = 624;
constexpr std::size_t kTwistOffset = 397;
} // namespace

bool NativeRetailRandom::access(
    RetailRandomFailure& failure, const char* operation) const noexcept {
    failure = {};
    if (std::this_thread::get_id() == owner_thread_) return true;
    failure = {RetailRandomError::wrong_thread, operation};
    return false;
}

bool NativeRetailRandom::seed(
    std::uint32_t value, RetailRandomFailure& failure) noexcept {
    if (!access(failure, "retail random seed thread")) return false;
    state_[0] = value == 0 ? 0x12345678u : value;
    for (std::size_t i = 1; i < kStateWords; ++i)
        state_[i] = state_[i - 1] * 69069u;

    std::uint32_t mixer = 373737u;
    for (std::size_t i = 1; i < kStateWords; ++i) {
        state_[i] ^= mixer;
        const std::uint32_t next_mixer = mixer * 5531u + 81547u;
        mixer = next_mixer ^ (next_mixer >> 9) ^ (next_mixer << 19);
    }
    cursor_ = 0;
    // 26D8E0 leaves the independent consumed counter untouched.
    return true;
}

void NativeRetailRandom::twist() noexcept {
    const auto mixed = [this](std::size_t current, std::size_t next) noexcept {
        const std::uint32_t joined =
            (state_[current] & kUpper) | (state_[next] & kLower);
        return (joined >> 1) ^ ((state_[next] & 1u) != 0 ? kMatrix : 0u);
    };
    std::size_t i = 0;
    for (; i < kStateWords - kTwistOffset; ++i)
        state_[i] = state_[i + kTwistOffset] ^ mixed(i, i + 1);
    for (; i < kStateWords - 1; ++i)
        state_[i] = state_[i + kTwistOffset - kStateWords] ^ mixed(i, i + 1);
    state_[kStateWords - 1] = state_[kTwistOffset - 1] ^
        mixed(kStateWords - 1, 0);
    consumed_ = 0;
    cursor_ = 0;
}

std::optional<std::uint32_t> NativeRetailRandom::next(
    RetailRandomFailure& failure) noexcept {
    if (!access(failure, "retail random next thread")) return std::nullopt;
    if (consumed_ > kStateWords - 1) twist();
    std::uint32_t value = state_[cursor_++];
    ++consumed_;
    value ^= value >> 11;
    value ^= (value << 7) & 0x9d2c5680u;
    value ^= (value << 15) & 0xefc60000u;
    value ^= value >> 18;
    return value;
}

std::optional<std::uint16_t> NativeRetailRandom::low16_range(
    std::uint32_t limit, RetailRandomFailure& failure) noexcept {
    // 185C30 consumes the next value before truncating or checking the limit.
    const auto value = next(failure);
    if (!value) return std::nullopt;
    const std::uint32_t bound = limit & 0xffffu;
    std::uint32_t scaled = ((*value & 0xffffu) * bound) >> 16;
    if (bound == 0) {
        if (scaled != 0) scaled = 0;
    } else if (scaled >= bound) {
        scaled = 0;
    }
    return static_cast<std::uint16_t>(scaled);
}

} // namespace mka::retail
