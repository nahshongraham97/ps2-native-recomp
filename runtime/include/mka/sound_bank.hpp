#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace mka::sound {

enum class BankError {
    none, truncated_header, wrong_type, unsupported_flags, negative_count,
    programs_out_of_bounds, selectors_out_of_bounds, invalid_program_mapping,
    auxiliaries_out_of_bounds, negative_command_count, commands_out_of_bounds,
    resources_out_of_bounds, resource_name_out_of_bounds, unterminated_resource_name
};
enum class LookupStatus { found, absent_selector, absent_program, malformed_bank };

struct ProgramLookup {
    LookupStatus status = LookupStatus::absent_selector;
    std::size_t index = 0;
    std::array<std::uint8_t, 16> bytes{};
};

// Serialized retail command data, never a relocated guest pointer or a native
// sample handle. Unknown kinds can be inspected here; playback must implement
// their proven meaning separately rather than silently skipping them.
struct CommandRecord {
    std::array<std::uint8_t, 40> bytes{};
    std::int32_t kind() const noexcept;
    std::uint32_t resource_bits() const noexcept;
    std::int32_t delay_milliseconds() const noexcept;
};

struct CommandLookup {
    LookupStatus status = LookupStatus::absent_selector;
    std::size_t program_index = 0;
    std::vector<CommandRecord> commands;
};

enum class ResourceLookupStatus { found, absent, unsupported_key_mode };
enum class ResourceKeyMode { unknown, numeric, name };
struct ResourceLookup {
    ResourceLookupStatus status = ResourceLookupStatus::absent;
    std::size_t index = 0;
    // Includes opaque serialized runtime-pointer slots; never dereferenced.
    std::array<std::uint8_t, 36> bytes{};
    // Required provenance: raw record+8 is a key only in numeric mode;
    // in named mode it is a serialized string offset, never a numeric key.
    ResourceKeyMode key_mode = ResourceKeyMode::unknown;
};

// Owns unmodified serialized bytes. No pointers from the serialized bank are
// relocated. Program bytes are copied out, so lookup results survive bank moves.
class SoundBank {
public:
    SoundBank(const SoundBank&) = default;
    // Moves leave the source empty and safe to query. Returned records own
    // their bytes independently of either bank's lifetime.
    SoundBank(SoundBank&& other) noexcept;
    SoundBank& operator=(SoundBank other) noexcept;
    static std::optional<SoundBank> parse(std::vector<std::uint8_t> bytes,
                                          BankError& error);
    ProgramLookup lookup(std::int32_t selector) const;
    CommandLookup lookup_commands(std::int32_t selector) const;
    ResourceLookup lookup_resource(std::uint32_t key) const;
    // Retail compares NUL-terminated names; an embedded NUL ends the query.
    ResourceLookup lookup_resource(std::string_view name) const;
    std::size_t resource_count() const noexcept { return resources_; }
    const std::vector<std::uint8_t>& raw_bytes() const noexcept { return bytes_; }
    std::size_t program_count() const noexcept { return programs_; }
    std::size_t selector_count() const noexcept { return selectors_; }

private:
    SoundBank() = default;
    std::vector<std::uint8_t> bytes_;
    std::size_t programs_ = 0, selectors_ = 0;
    std::size_t program_offset_ = 0, selector_offset_ = 0;
    std::vector<std::size_t> command_offsets_, command_counts_;
    std::size_t resources_ = 0, resource_offset_ = 0;
    bool numeric_resources_ = false;
    std::vector<std::size_t> name_offsets_, name_lengths_;
    ResourceLookup resource_result(std::size_t index) const;
    void swap(SoundBank& other) noexcept;
};

} // namespace mka::sound
