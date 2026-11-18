#include "mka/sound_bank.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <utility>

using namespace mka::sound;
namespace {
void check(bool ok, const char* what) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", what); std::exit(1); }
}
void put(std::vector<std::uint8_t>& b, std::size_t p, std::uint32_t v, unsigned n = 4) {
    for (unsigned i = 0; i < n; ++i) b[p + i] = static_cast<std::uint8_t>(v >> (i * 8));
}
std::vector<std::uint8_t> fixture() {
    std::vector<std::uint8_t> b(372);
    put(b, 0, 11); put(b, 4, 0x13); put(b, 8, 3, 2); put(b, 0x10, 264); put(b, 10, 2, 2); put(b, 12, 5, 2);
    put(b, 0x14, 64); put(b, 0x20, 96);
    put(b, 0x18, 112); put(b, 0x1c, 144);
    for (unsigned i = 0; i < 32; ++i) b[64 + i] = static_cast<std::uint8_t>(i + 1);
    put(b, 96, 2, 2); put(b, 98, 0, 2); put(b, 100, 0xffff, 2);
    put(b, 102, 1, 2); put(b, 104, 0x8000, 2);
    put(b, 120, 1); put(b, 136, 2); // auxiliary signed command counts
    put(b, 124, 0xffffffffu); put(b, 140, 1); // ignored serialized pointer slots
    put(b, 144, 0xffffffffu); b[152] = 0x80; put(b, 168, 0x80000000u);
    put(b, 184, 0xdeadbeefu); b[192] = 1; put(b, 208, 0x7fffffffu);
    put(b, 224, 0xffffffffu); b[232] = 0xff; put(b, 248, 0xffffffffu);
    put(b, 264, 0xffffffffu); put(b, 272, 0x12340001u);
    put(b, 300, 0x87654321u); put(b, 308, 0xabcd0001u);
    put(b, 336, 0); put(b, 344, 0x12340001u);
    return b;
}
void reject(std::vector<std::uint8_t> b, BankError wanted) {
    BankError error;
    check(!SoundBank::parse(std::move(b), error) && error == wanted, "typed malformed bank error");
}
}
int main(int argc, char** argv) {
    BankError error;
    auto original = fixture();
    auto bank = SoundBank::parse(original, error);
    check(bank.has_value() && error == BankError::none, "valid bank");
    check(bank->raw_bytes() == original, "raw bytes preserved without relocation");
    auto second = bank->lookup(0);
    check(second.status == LookupStatus::found && second.index == 1 && second.bytes[0] == 17 && second.bytes[15] == 32,
          "selector zero maps to second program, not sample zero");
    check(bank->lookup(3).index == 0 && bank->lookup(3).bytes[0] == 1, "selector three maps to first program");
    auto commands = bank->lookup_commands(0);
    check(commands.status == LookupStatus::found && commands.program_index == 1 && commands.commands.size() == 2,
          "selector maps to its program's prefix-summed commands, not direct sample index");
    check(commands.commands[0].kind() == 1 && commands.commands[0].resource_bits() == 0xdeadbeefu &&
          commands.commands[0].delay_milliseconds() == 2147483647, "exact command kind/key/delay fields");
    check(commands.commands[1].kind() == -1 && commands.commands[1].resource_bits() == 0xffffffffu &&
          commands.commands[1].delay_milliseconds() == -1, "signed kind and delay/sentinel bytes preserved");
    const auto first_commands = bank->lookup_commands(3);
    check(first_commands.commands.size() == 1 && first_commands.commands[0].kind() == -128 &&
          first_commands.commands[0].delay_milliseconds() == (-2147483647 - 1), "signed extrema and first program boundary");
    for (int i : {1, 2, 4}) check(bank->lookup(i).status == LookupStatus::absent_program, "zero and negative sentinel maps absent");
    for (int i : {-1, 5, 2147483647}) check(bank->lookup(i).status == LookupStatus::absent_selector, "invalid selector absent");
    auto moved = std::move(*bank);
    check(bank->raw_bytes().empty() && bank->program_count() == 0 && bank->selector_count() == 0 &&
          bank->resource_count() == 0 && bank->lookup(0).status == LookupStatus::absent_selector &&
          bank->lookup_commands(0).status == LookupStatus::absent_selector,
          "moved-from bank is empty and safe to query");
    check(second.bytes[0] == 17 && moved.lookup(0).bytes == second.bytes, "owned result and bank move lifetime");
    check(commands.commands[0].resource_bits() == 0xdeadbeefu && moved.lookup_commands(0).commands[1].kind() == -1,
          "command copies and bank metadata survive move");
    for (int i : {1, 2, 4}) check(moved.lookup_commands(i).status == LookupStatus::absent_program, "absent command program");
    for (int i : {-1, 5}) check(moved.lookup_commands(i).status == LookupStatus::absent_selector, "absent command selector");

    reject(std::vector<std::uint8_t>(43), BankError::truncated_header);
    auto b = fixture(); put(b, 0, 12); reject(b, BankError::wrong_type);
    for (auto flags : {0u, 1u, 2u, 11u}) { b = fixture(); put(b, 4, flags); reject(b, BankError::unsupported_flags); }
    for (auto pos : {10u, 12u}) { b = fixture(); put(b, pos, 0xffff, 2); reject(b, BankError::negative_count); }
    b = fixture(); b.resize(95); reject(b, BankError::programs_out_of_bounds);
    b = fixture(); b.resize(105); reject(b, BankError::selectors_out_of_bounds);
    b = fixture(); put(b, 0x14, 0xfffffff0u); reject(b, BankError::programs_out_of_bounds);
    b = fixture(); put(b, 0x20, 0xffffffffu); reject(b, BankError::selectors_out_of_bounds);
    b = fixture(); put(b, 96, 3, 2); reject(b, BankError::invalid_program_mapping);
    b = fixture(); put(b, 0x18, 0xffffffffu); reject(b, BankError::auxiliaries_out_of_bounds);
    b = fixture(); put(b, 0x18, static_cast<std::uint32_t>(b.size() - 16)); reject(b, BankError::auxiliaries_out_of_bounds);
    b = fixture(); put(b, 120, 0xffffffffu); reject(b, BankError::negative_command_count);
    b = fixture(); put(b, 136, 0x80000000u); reject(b, BankError::negative_command_count);
    b = fixture(); put(b, 136, 0x7fffffffu); reject(b, BankError::commands_out_of_bounds);
    b = fixture(); put(b, 0x1c, 0xffffffffu); reject(b, BankError::commands_out_of_bounds);
    b = fixture(); put(b, 0x1c, static_cast<std::uint32_t>(b.size() - 119)); reject(b, BankError::commands_out_of_bounds);
    b = fixture(); put(b, 120, 0); put(b, 136, 0); put(b, 0x1c, static_cast<std::uint32_t>(b.size()));
    auto empty_commands = SoundBank::parse(b, error);
    check(empty_commands && empty_commands->lookup_commands(0).commands.empty(), "zero commands with one-past-end empty range");
    b = fixture(); put(b, 10, 0, 2); put(b, 12, 0, 2);
    check(SoundBank::parse(b, error)->lookup(0).status == LookupStatus::absent_selector, "empty bank");

    // A real host allocation and table access beyond the former 32 MiB window.
    const std::size_t far = 32u * 1024u * 1024u + 123u;
    b = fixture(); b.resize(far + 32);
    put(b, 0x14, static_cast<std::uint32_t>(far)); b[far + 16] = 0xa7;
    auto large = SoundBank::parse(std::move(b), error);
    check(large && large->lookup(0).bytes[0] == 0xa7, "program beyond 32 MiB has no guest window limit");
    b = fixture(); b.resize(far + 120);
    std::copy(original.begin() + 144, original.begin() + 264, b.begin() + far);
    put(b, 0x1c, static_cast<std::uint32_t>(far));
    auto far_commands = SoundBank::parse(std::move(b), error);
    check(far_commands && far_commands->lookup_commands(0).commands[0].resource_bits() == 0xdeadbeefu,
          "command table beyond 32 MiB has no guest window limit");

    auto numeric = SoundBank::parse(fixture(), error);
    check(numeric && numeric->resource_count() == 3, "resource table count");
    auto owned = numeric->lookup_resource(0xabcd0001u);
    check(owned.status == ResourceLookupStatus::found && owned.index == 1 && owned.bytes[0] == 0x21,
          "full 32-bit keys with same low 16 bits and opaque pointer bytes");
    check(numeric->lookup_resource(0x12340001u).index == 0, "first duplicate key wins");
    check(numeric->lookup_resource(1u).status == ResourceLookupStatus::absent &&
          numeric->lookup_resource(0u).status == ResourceLookupStatus::absent, "absent keys never select record zero");
    check(numeric->lookup_resource("name").status == ResourceLookupStatus::unsupported_key_mode, "numeric bank rejects name overload");
    b = fixture(); put(b, 272, 0); put(b, 308, 0xffffffffu);
    auto extrema = SoundBank::parse(b, error);
    check(extrema->lookup_resource(0u).status == ResourceLookupStatus::found &&
          extrema->lookup_resource(0xffffffffu).index == 1, "resolver itself does not filter zero or sentinel keys");
    auto relocated = std::move(*numeric); numeric.reset();
    check(relocated.lookup_resource(0xabcd0001u).bytes == owned.bytes, "resource metadata survives bank move");
    check(owned.bytes[0] == 0x21, "resource result owns its bytes");
    b = fixture(); put(b, 8, 0xffff, 2); reject(b, BankError::negative_count);
    b = fixture(); put(b, 0x10, 0xffffffffu); reject(b, BankError::resources_out_of_bounds);
    b = fixture(); b.pop_back(); reject(b, BankError::resources_out_of_bounds);
    b = fixture(); put(b, 8, 0, 2); put(b, 0x10, static_cast<std::uint32_t>(b.size()));
    check(SoundBank::parse(b, error)->lookup_resource(0u).status == ResourceLookupStatus::absent, "empty resource table one-past-end");

    auto named_bytes = fixture(); put(named_bytes, 4, 3); put(named_bytes, 0x28, 372);
    const unsigned char names[] = {'A','l','p','h','a',0,'A','L','P','H','A',0,0};
    named_bytes.insert(named_bytes.end(), std::begin(names), std::end(names));
    put(named_bytes, 272, 0); put(named_bytes, 308, 6); put(named_bytes, 344, 12);
    auto named = SoundBank::parse(named_bytes, error);
    check(named && named->lookup_resource("aLPHa").index == 0 &&
          named->lookup_resource("aLPHa").status == ResourceLookupStatus::found, "ASCII name comparison and first duplicate");
    check(named->lookup_resource("Alph").status == ResourceLookupStatus::absent &&
          named->lookup_resource("Alphabet").status == ResourceLookupStatus::absent, "names require exact prefix and NUL boundary");
    check(named->lookup_resource(std::string_view("alpha\0tail", 10)).index == 0 &&
          named->lookup_resource("").index == 2, "embedded NUL terminates query and empty name matches");
    check(named->lookup_resource(0u).status == ResourceLookupStatus::unsupported_key_mode, "named bank rejects numeric overload");
    auto named_copy = named->lookup_resource("alpha");
    auto moved_named = std::move(*named); named.reset(); named_bytes.clear();
    check(moved_named.lookup_resource("ALPHA").bytes == named_copy.bytes, "owned name spans survive move and caller buffer lifetime");
    auto assigned = SoundBank::parse(fixture(), error);
    *assigned = std::move(moved_named);
    check(assigned->lookup_resource("alpha").bytes == named_copy.bytes && moved_named.raw_bytes().empty() &&
          moved_named.lookup_resource("alpha").status == ResourceLookupStatus::absent,
          "move assignment replaces prior bank and empties source");
    *assigned = std::move(*assigned);
    check(assigned->lookup_resource("alpha").bytes == named_copy.bytes, "self move assignment preserves valid bank");
    *assigned = moved;
    check(assigned->lookup_commands(0).commands[0].resource_bits() == 0xdeadbeefu &&
          moved.lookup_commands(0).commands[0].resource_bits() == 0xdeadbeefu, "copy assignment retains independent valid metadata");
    b = fixture(); put(b, 4, 3); put(b, 0x28, 0xffffffffu); reject(b, BankError::resource_name_out_of_bounds);
    b = fixture(); put(b, 4, 3); put(b, 0x28, 0); put(b, 272, 0xffffffffu); reject(b, BankError::resource_name_out_of_bounds);
    b = fixture(); put(b, 4, 3); put(b, 0x28, 371); put(b, 272, 0); b.back() = 'x';
    reject(b, BankError::unterminated_resource_name);
    b = fixture(); b.resize(far + 108);
    std::copy(original.begin() + 264, original.end(), b.begin() + far);
    put(b, 0x10, static_cast<std::uint32_t>(far));
    auto far_resources = SoundBank::parse(std::move(b), error);
    check(far_resources && far_resources->lookup_resource(0xabcd0001u).index == 1,
          "actual resource table access beyond 32 MiB");

    // Optional extracted raw type-11 bank, supplied explicitly; no name/index inference.
    if (argc == 2) {
        std::ifstream file(argv[1], std::ios::binary);
        check(bool(file), "open explicit retail bank chunk");
        std::vector<std::uint8_t> raw((std::istreambuf_iterator<char>(file)), {});
        auto retail = SoundBank::parse(std::move(raw), error);
        check(bool(retail), "parse explicit retail bank chunk");
        std::size_t found = 0;
        std::size_t command_count = 0;
        for (std::size_t i = 0; i < retail->selector_count(); ++i)
            if (retail->lookup(static_cast<std::int32_t>(i)).status == LookupStatus::found) {
                ++found;
                command_count += retail->lookup_commands(static_cast<std::int32_t>(i)).commands.size();
            }
        check(found != 0, "retail bank resolves at least one program");
        std::printf("Retail bank: %zu programs, %zu selectors, %zu resolved\n", retail->program_count(), retail->selector_count(), found);
        std::printf("Retail commands across resolved selectors: %zu\n", command_count);
    }
    std::puts("sound bank tests passed");
}
