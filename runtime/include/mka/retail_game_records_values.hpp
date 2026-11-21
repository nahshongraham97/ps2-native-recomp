#pragma once

#include <cstdint>

namespace mka::retail {
// Source-proven persistent words outside the 185EE0 reset block. Names carry
// provenance, not inferred gameplay meaning or a guest-memory access API.
struct RetailGameRecordsValues {
    std::uint32_t word_660098{};
    std::uint32_t word_6602e4{}, word_6602e8{}, word_6600a4{};
    std::uint32_t word_6b6f94{}, word_6b6f98{};
    std::uint32_t word_6602ac{}, word_6602c4{}, word_6602b0{};
    std::uint32_t word_6602b4{}, word_6602b8{}, word_6602bc{};
    std::uint32_t word_6602c0{};
    std::uint32_t word_64e1f0{};
    std::uint32_t word_64debc{};
    std::uint32_t word_64e2a0{}; // 12D9C0 deferred-free phase, BSS zero.
    std::uint32_t word_64e01c{}, word_64e000{};
    std::uint32_t word_64dff4{}, word_64dff0{};
    std::uint32_t word_64dffc{}, word_64dff8{};
    std::uint32_t word_64e018{}, word_64e014{}, word_64e010{};
    std::uint32_t word_64e00c{}, word_64e008{}, word_64e004{};
    std::uint32_t word_64dfec{}, word_64dfe8{};
    std::uint32_t word_64e180{};
};
} // namespace mka::retail
