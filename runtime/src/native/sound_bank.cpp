#include "mka/sound_bank.hpp"

#include <algorithm>
#include <utility>

namespace mka::sound {
namespace {
std::uint32_t u32(const std::vector<std::uint8_t>& b, std::size_t p) {
    return std::uint32_t(b[p]) | (std::uint32_t(b[p + 1]) << 8) |
           (std::uint32_t(b[p + 2]) << 16) | (std::uint32_t(b[p + 3]) << 24);
}
std::int32_t s16(const std::vector<std::uint8_t>& b, std::size_t p) {
    const auto v = std::uint32_t(b[p]) | (std::uint32_t(b[p + 1]) << 8);
    return v < 0x8000u ? static_cast<std::int32_t>(v) : static_cast<std::int32_t>(v) - 65536;
}
bool fits(std::size_t size, std::size_t offset, std::size_t count, std::size_t stride) {
    // Subtract before multiplying: safe even with offsets near UINT32_MAX,
    // including a 32-bit host size_t. No serialized address-space ceiling.
    return offset <= size && count <= (size - offset) / stride;
}
}

std::optional<SoundBank> SoundBank::parse(std::vector<std::uint8_t> bytes, BankError& error) {
    error = BankError::none;
    auto fail = [&](BankError e) -> std::optional<SoundBank> { error = e; return std::nullopt; };
    if (bytes.size() < 44) return fail(BankError::truncated_header);
    // Retail MSL PC 0x2A3B4..0x2A3E8 checks type 11, flags 1 and 2,
    // and rejects flag 8 before relocation at 0x2A4F0.
    if (u32(bytes, 0) != 11) return fail(BankError::wrong_type);
    const auto flags = u32(bytes, 4);
    if ((flags & 3u) != 3u || (flags & 8u)) return fail(BankError::unsupported_flags);
    const auto programs = s16(bytes, 0x0a), selectors = s16(bytes, 0x0c);
    if (programs < 0 || selectors < 0) return fail(BankError::negative_count);
    const auto po = u32(bytes, 0x14), so = u32(bytes, 0x20);
    if (!fits(bytes.size(), po, programs, 16)) return fail(BankError::programs_out_of_bounds);
    if (!fits(bytes.size(), so, selectors, 2)) return fail(BankError::selectors_out_of_bounds);
    // Nonpositive mappings are absent in retail. Positive mappings are 1-based.
    for (std::size_t i = 0; i < static_cast<std::size_t>(selectors); ++i)
        if (s16(bytes, so + i * 2) > programs) return fail(BankError::invalid_program_mapping);
    SoundBank bank;
    bank.bytes_ = std::move(bytes);
    bank.programs_ = programs;
    bank.selectors_ = selectors;
    bank.program_offset_ = po;
    bank.selector_offset_ = so;
    return bank;
}

ProgramLookup SoundBank::lookup(std::int32_t selector) const {
    ProgramLookup result;
    if (selector < 0 || static_cast<std::size_t>(selector) >= selectors_) return result;
    // Exact retail lookup: MSL PC 0x299FC..0x29A78. Unlike the sample table,
    // the program table has 16-byte records and an intervening selector map.
    const auto mapped = s16(bytes_, selector_offset_ + static_cast<std::size_t>(selector) * 2) - 1;
    if (mapped < 0) { result.status = LookupStatus::absent_program; return result; }
    if (static_cast<std::size_t>(mapped) >= programs_) {
        result.status = LookupStatus::malformed_bank;
        return result;
    }
    result.status = LookupStatus::found;
    result.index = static_cast<std::size_t>(mapped);
    std::copy_n(bytes_.begin() + program_offset_ + result.index * 16, 16, result.bytes.begin());
    return result;
}
} // namespace mka::sound
