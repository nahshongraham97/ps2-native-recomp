#include "mka/sound_bank.hpp"

#include <algorithm>
#include <utility>

namespace mka::sound {
namespace {
template<class Bytes>
std::uint32_t u32(const Bytes& b, std::size_t p) {
    return std::uint32_t(b[p]) | (std::uint32_t(b[p + 1]) << 8) |
           (std::uint32_t(b[p + 2]) << 16) | (std::uint32_t(b[p + 3]) << 24);
}
template<class Bytes>
std::int32_t s32(const Bytes& b, std::size_t p) {
    const auto v = u32(b, p);
    return static_cast<std::int32_t>(v < 0x80000000u ? static_cast<std::int64_t>(v)
                                                       : static_cast<std::int64_t>(v) - 0x100000000ll);
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

std::int32_t CommandRecord::kind() const noexcept {
    return bytes[8] < 128 ? bytes[8] : static_cast<std::int32_t>(bytes[8]) - 256;
}
std::uint32_t CommandRecord::resource_bits() const noexcept { return u32(bytes, 0); }
std::int32_t CommandRecord::delay_milliseconds() const noexcept { return s32(bytes, 0x18); }

void SoundBank::swap(SoundBank& other) noexcept {
    using std::swap;
    bytes_.swap(other.bytes_);
    swap(programs_, other.programs_);
    swap(selectors_, other.selectors_);
    swap(program_offset_, other.program_offset_);
    swap(selector_offset_, other.selector_offset_);
    command_offsets_.swap(other.command_offsets_);
    command_counts_.swap(other.command_counts_);
    swap(resources_, other.resources_);
    swap(resource_offset_, other.resource_offset_);
    swap(numeric_resources_, other.numeric_resources_);
    name_offsets_.swap(other.name_offsets_);
    name_lengths_.swap(other.name_lengths_);
}
SoundBank::SoundBank(SoundBank&& other) noexcept { swap(other); }
SoundBank& SoundBank::operator=(SoundBank other) noexcept {
    swap(other);
    return *this;
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
    // MSL 0x2A59C..0x2A668: program[i]+0 is overwritten with auxiliary[i],
    // auxiliary[i]+0x0C with the command cursor. The signed count at +8,
    // not either serialized pointer slot, advances that cursor by count*40.
    const auto auxiliary = u32(bytes, 0x18);
    std::size_t cursor = u32(bytes, 0x1c);
    if (!fits(bytes.size(), auxiliary, programs, 16)) return fail(BankError::auxiliaries_out_of_bounds);
    if (!fits(bytes.size(), cursor, 0, 40)) return fail(BankError::commands_out_of_bounds);
    bank.command_offsets_.reserve(programs);
    bank.command_counts_.reserve(programs);
    for (std::size_t i = 0; i < static_cast<std::size_t>(programs); ++i) {
        const auto count = s32(bytes, auxiliary + i * 16 + 8);
        if (count < 0) return fail(BankError::negative_command_count);
        if (!fits(bytes.size(), cursor, static_cast<std::size_t>(count), 40))
            return fail(BankError::commands_out_of_bounds);
        bank.command_offsets_.push_back(cursor);
        bank.command_counts_.push_back(static_cast<std::size_t>(count));
        cursor += static_cast<std::size_t>(count) * 40;
    }
    // MSL 0x2A78C..0x2A850 scans only this bank, in order, at stride 36.
    const auto resources = s16(bytes, 8);
    if (resources < 0) return fail(BankError::negative_count);
    const auto ro = u32(bytes, 0x10);
    if (!fits(bytes.size(), ro, resources, 36)) return fail(BankError::resources_out_of_bounds);
    bank.resources_ = resources;
    bank.resource_offset_ = ro;
    bank.numeric_resources_ = (flags & 0x10u) != 0;
    if (!bank.numeric_resources_ && resources != 0) {
        // 0x2A674..0x2A6BC skips relocation for numeric keys. Otherwise
        // helper 0x2A6D8 adds raw resource+8 to the header+0x28 string base.
        const std::size_t base = u32(bytes, 0x28);
        if (base > bytes.size()) return fail(BankError::resource_name_out_of_bounds);
        for (std::size_t i = 0; i < static_cast<std::size_t>(resources); ++i) {
            const std::size_t relative = u32(bytes, ro + i * 36 + 8);
            if (relative >= bytes.size() - base) return fail(BankError::resource_name_out_of_bounds);
            const auto offset = base + relative;
            const auto end = std::find(bytes.begin() + offset, bytes.end(), 0);
            if (end == bytes.end()) return fail(BankError::unterminated_resource_name);
            bank.name_offsets_.push_back(offset);
            bank.name_lengths_.push_back(static_cast<std::size_t>(end - (bytes.begin() + offset)));
        }
    }
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

CommandLookup SoundBank::lookup_commands(std::int32_t selector) const {
    CommandLookup result;
    const auto program = lookup(selector);
    result.status = program.status;
    if (program.status != LookupStatus::found) return result;
    result.program_index = program.index;
    const auto offset = command_offsets_[program.index];
    result.commands.resize(command_counts_[program.index]);
    for (std::size_t i = 0; i < result.commands.size(); ++i)
        std::copy_n(bytes_.begin() + offset + i * 40, 40, result.commands[i].bytes.begin());
    return result;
}
ResourceLookup SoundBank::resource_result(std::size_t index) const {
    ResourceLookup result;
    result.status = ResourceLookupStatus::found;
    result.index = index;
    result.key_mode = numeric_resources_ ? ResourceKeyMode::numeric : ResourceKeyMode::name;
    std::copy_n(bytes_.begin() + resource_offset_ + index * 36, 36, result.bytes.begin());
    return result;
}

ResourceLookup SoundBank::lookup_resource(std::uint32_t key) const {
    if (!numeric_resources_) return {ResourceLookupStatus::unsupported_key_mode};
    for (std::size_t i = 0; i < resources_; ++i)
        if (u32(bytes_, resource_offset_ + i * 36 + 8) == key) return resource_result(i);
    return {};
}

ResourceLookup SoundBank::lookup_resource(std::string_view name) const {
    if (numeric_resources_) return {ResourceLookupStatus::unsupported_key_mode};
    name = name.substr(0, name.find('\0'));
    const auto fold = [](unsigned char c) { return c >= 'a' && c <= 'z' ? c - 32 : c; };
    // Retail helper 0x328F0 folds only ASCII a..z and compares through NUL.
    for (std::size_t i = 0; i < resources_; ++i) {
        if (name.size() != name_lengths_[i]) continue;
        bool equal = true;
        for (std::size_t j = 0; j < name.size(); ++j)
            if (fold(bytes_[name_offsets_[i] + j]) != fold(static_cast<unsigned char>(name[j]))) {
                equal = false;
                break;
            }
        if (equal) return resource_result(i);
    }
    return {};
}
} // namespace mka::sound
