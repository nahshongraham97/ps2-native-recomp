#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace mka::sound {

enum class BankError {
    none, truncated_header, wrong_type, unsupported_flags, negative_count,
    programs_out_of_bounds, selectors_out_of_bounds, invalid_program_mapping
};
enum class LookupStatus { found, absent_selector, absent_program, malformed_bank };

struct ProgramLookup {
    LookupStatus status = LookupStatus::absent_selector;
    std::size_t index = 0;
    std::array<std::uint8_t, 16> bytes{};
};

// Owns unmodified serialized bytes. No pointers from the serialized bank are
// relocated. Program bytes are copied out, so lookup results survive bank moves.
class SoundBank {
public:
    static std::optional<SoundBank> parse(std::vector<std::uint8_t> bytes,
                                          BankError& error);
    ProgramLookup lookup(std::int32_t selector) const;
    const std::vector<std::uint8_t>& raw_bytes() const noexcept { return bytes_; }
    std::size_t program_count() const noexcept { return programs_; }
    std::size_t selector_count() const noexcept { return selectors_; }

private:
    SoundBank() = default;
    std::vector<std::uint8_t> bytes_;
    std::size_t programs_ = 0, selectors_ = 0;
    std::size_t program_offset_ = 0, selector_offset_ = 0;
};

} // namespace mka::sound
