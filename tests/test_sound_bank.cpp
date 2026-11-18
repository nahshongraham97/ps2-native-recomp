#include "mka/sound_bank.hpp"

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
    std::vector<std::uint8_t> b(112);
    put(b, 0, 11); put(b, 4, 3); put(b, 10, 2, 2); put(b, 12, 5, 2);
    put(b, 0x14, 64); put(b, 0x20, 96);
    for (unsigned i = 0; i < 32; ++i) b[64 + i] = static_cast<std::uint8_t>(i + 1);
    put(b, 96, 2, 2); put(b, 98, 0, 2); put(b, 100, 0xffff, 2);
    put(b, 102, 1, 2); put(b, 104, 0x8000, 2);
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
    for (int i : {1, 2, 4}) check(bank->lookup(i).status == LookupStatus::absent_program, "zero and negative sentinel maps absent");
    for (int i : {-1, 5, 2147483647}) check(bank->lookup(i).status == LookupStatus::absent_selector, "invalid selector absent");
    auto moved = std::move(*bank);
    check(second.bytes[0] == 17 && moved.lookup(0).bytes == second.bytes, "owned result and bank move lifetime");

    reject(std::vector<std::uint8_t>(43), BankError::truncated_header);
    auto b = fixture(); put(b, 0, 12); reject(b, BankError::wrong_type);
    for (auto flags : {0u, 1u, 2u, 11u}) { b = fixture(); put(b, 4, flags); reject(b, BankError::unsupported_flags); }
    for (auto pos : {10u, 12u}) { b = fixture(); put(b, pos, 0xffff, 2); reject(b, BankError::negative_count); }
    b = fixture(); b.resize(95); reject(b, BankError::programs_out_of_bounds);
    b = fixture(); b.resize(105); reject(b, BankError::selectors_out_of_bounds);
    b = fixture(); put(b, 0x14, 0xfffffff0u); reject(b, BankError::programs_out_of_bounds);
    b = fixture(); put(b, 0x20, 0xffffffffu); reject(b, BankError::selectors_out_of_bounds);
    b = fixture(); put(b, 96, 3, 2); reject(b, BankError::invalid_program_mapping);
    b = fixture(); put(b, 10, 0, 2); put(b, 12, 0, 2);
    check(SoundBank::parse(b, error)->lookup(0).status == LookupStatus::absent_selector, "empty bank");

    // A real host allocation and table access beyond the former 32 MiB window.
    const std::size_t far = 32u * 1024u * 1024u + 123u;
    b = fixture(); b.resize(far + 32);
    put(b, 0x14, static_cast<std::uint32_t>(far)); b[far + 16] = 0xa7;
    auto large = SoundBank::parse(std::move(b), error);
    check(large && large->lookup(0).bytes[0] == 0xa7, "program beyond 32 MiB has no guest window limit");

    // Optional extracted raw type-11 bank, supplied explicitly; no name/index inference.
    if (argc == 2) {
        std::ifstream file(argv[1], std::ios::binary);
        check(bool(file), "open explicit retail bank chunk");
        std::vector<std::uint8_t> raw((std::istreambuf_iterator<char>(file)), {});
        auto retail = SoundBank::parse(std::move(raw), error);
        check(bool(retail), "parse explicit retail bank chunk");
        std::size_t found = 0;
        for (std::size_t i = 0; i < retail->selector_count(); ++i)
            if (retail->lookup(static_cast<std::int32_t>(i)).status == LookupStatus::found) ++found;
        check(found != 0, "retail bank resolves at least one program");
        std::printf("Retail bank: %zu programs, %zu selectors, %zu resolved\n", retail->program_count(), retail->selector_count(), found);
    }
    std::puts("sound bank tests passed");
}
