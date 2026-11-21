#pragma once

#include <cstdint>

namespace mka::retail {
// Explicit source-proven scalar stores, grouped by their original writer.
// The suffixes identify provenance only; no gameplay meaning or guest-address
// access API is inferred. Future source consumers must join these same fields.
struct RetailGameRecordResetSnapshot {
    std::uint32_t word_152880_88{}, word_152880_8c{}, word_152880_90{};
    std::uint32_t word_152880_9c{}, word_152880_ac{}, word_152880_b0{};
    std::uint32_t word_152880_f8{}, word_152880_128{}, word_152880_124{};
    std::uint32_t word_152880_a0{};
    std::uint32_t word_16a9d0_2d4{};
    std::uint32_t word_185ee0_7b7348{}, word_185ee0_e070{};
    std::uint32_t word_185ee0_dfc0{}, word_185ee0_dfbc{};
    std::uint32_t word_185ee0_dfb8{}, word_185ee0_dfb4{};
    std::uint32_t word_185ee0_dfb0{}, word_185ee0_df80{};
    std::uint32_t word_185ee0_e308{}, word_185ee0_e30c{};
    std::uint32_t word_185ee0_e300{}, word_185ee0_e304{};
    std::uint32_t word_185ee0_e2f8{}, word_185ee0_e2fc{};
    std::uint32_t word_185ee0_e2f0{}, word_185ee0_e2f4{};
    std::uint32_t word_185ee0_ed80{}, word_185ee0_ed84{};
    std::uint32_t word_185ee0_ed78{}, word_185ee0_ed7c{};
    std::uint32_t word_185ee0_ecf4{}, word_185ee0_eaa4{};
    std::uint32_t word_185ee0_f2b4{}, word_185ee0_e120{};
    std::uint32_t word_185ee0_e11c{}, word_185ee0_e118{};
    std::uint32_t word_185ee0_c3c8{1}; // Original file-backed word is 1.
    std::uint32_t word_27a6a0_e9c8{};
    bool partial_reset{};
};

} // namespace mka::retail
