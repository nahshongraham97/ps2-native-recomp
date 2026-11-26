// Constructed machine-state capability tests, independent actual v8 execute()
// reference. The generated DUT has specialized C++ functions, no opcode decode.
#include "iop/iop.hpp"
#include "iop/kernel.hpp"
#include "native/iop_native_support.h"
#include "v8-fixtures.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>
namespace ps2::iop { void execute(Cpu &, std::int64_t &); }
namespace {
using namespace ps2::iop;
using ps2x::iop::detail::IopCpuState;
using ps2x::iop::detail::IopMemory;
struct Write { std::uint32_t a, n, v; bool operator==(const Write &) const = default; };
bool observe{};
std::vector<Write> writes;
void native(Cpu &c, fixtures::Function fn) {
    IopCpuState state{}; std::copy(c.r, c.r + 32, state.gpr.begin());
    state.hi = c.hi; state.lo = c.lo; state.pc = c.npc;
    state.cop0[12] = c.status; state.cop0[13] = c.cause; state.cop0[14] = c.epc; state.cop0[15] = 0x1F;
    const auto npc = c.npc + 4;
    IopMemory memory; fn(state, memory);
    std::copy(state.gpr.begin(), state.gpr.end(), c.r); c.r[0] = 0;
    c.hi = state.hi; c.lo = state.lo; c.pc = state.pc; c.npc = npc;
    c.status = state.cop0[12]; c.cause = state.cop0[13]; c.epc = state.cop0[14];
}
std::uint32_t random_word(std::uint32_t &seed) { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed; }
}
extern "C" void __real__ZN3ps23iop6write8Ejh(std::uint32_t, std::uint8_t);
extern "C" void __real__ZN3ps23iop7write16Ejt(std::uint32_t, std::uint16_t);
extern "C" void __real__ZN3ps23iop7write32Ejj(std::uint32_t, std::uint32_t);
extern "C" void __wrap__ZN3ps23iop6write8Ejh(std::uint32_t a, std::uint8_t v) { if (observe) writes.push_back({a, 1, v}); __real__ZN3ps23iop6write8Ejh(a, v); }
extern "C" void __wrap__ZN3ps23iop7write16Ejt(std::uint32_t a, std::uint16_t v) { if (observe) writes.push_back({a, 2, v}); __real__ZN3ps23iop7write16Ejt(a, v); }
extern "C" void __wrap__ZN3ps23iop7write32Ejj(std::uint32_t a, std::uint32_t v) { if (observe) writes.push_back({a, 4, v}); __real__ZN3ps23iop7write32Ejj(a, v); }
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    static_assert(sizeof(Cpu) == 156);
    std::uint32_t rng = 0x19280001u;
    const std::uint32_t edges[]{0u, 1u, 0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFFu, 0x12345678u};
    std::vector<std::uint8_t> seed(kRamSize), actual(kRamSize);
    unsigned failures{}, comparisons{}, pair_comparisons{}, queue_plant_refused{};
    for (unsigned index = 0; index < 20000; ++index) {
        const auto &test = fixtures::cases[index % fixtures::count];
        Cpu initial{};
        for (auto &r : initial.r) r = random_word(rng);
        initial.r[0] = 0; initial.r[4] = edges[index % 6]; initial.r[5] = edges[(index / 6) % 6];
        if (test.memory) initial.r[4] = 0x100000u + ((index / 3) & 3u);
        initial.hi = random_word(rng); initial.lo = random_word(rng);
        initial.status = random_word(rng); initial.cause = random_word(rng); initial.epc = random_word(rng);
        initial.pc = 0x30000; initial.npc = initial.pc + 4;
        for (unsigned i = 0; i < 32; ++i) seed[0xFFFF0 + i] = static_cast<std::uint8_t>(random_word(rng));
        std::memcpy(seed.data() + initial.pc, &test.word, 4);
        std::memcpy(g_ram, seed.data(), seed.size()); writes.clear(); observe = true;
        Cpu got = initial; native(got, test.fn); observe = false;
        const auto expected_writes = writes; std::memcpy(actual.data(), g_ram, actual.size());
        std::memcpy(g_ram, seed.data(), seed.size()); writes.clear(); observe = true;
        Cpu expected = initial; std::int64_t one = 1; execute(expected, one); observe = false;
        const bool equal = std::memcmp(&got, &expected, sizeof got) == 0 && writes == expected_writes &&
                           std::memcmp(g_ram, actual.data(), actual.size()) == 0;
        if (!equal) { if (failures < 10) std::fprintf(stderr, "REFUSED %s %08X case %u\n", test.name, test.word, index); ++failures; }
        ++comparisons;
    }
    // Standard unaligned little-endian pair; compare after EACH original
    // instruction, so an accidentally queued implementation cannot hide.
    for (unsigned alignment = 0; alignment < 4; ++alignment) for (unsigned edge = 0; edge < 6; ++edge) {
        Cpu expected{}, got{}; expected.pc = 0x30000; expected.npc = 0x30004;
        expected.r[4] = 0x100000 + alignment; expected.r[2] = edges[edge]; got = expected;
        for (unsigned step = 0; step < 2; ++step) {
            const auto &test = fixtures::unaligned_pair[step];
            std::memcpy(g_ram, seed.data(), seed.size()); set_ram32(expected.pc, test.word);
            std::int64_t one = 1; execute(expected, one);
            std::memcpy(g_ram, seed.data(), seed.size()); native(got, test.fn);
            if (std::memcmp(&got, &expected, sizeof got)) ++failures;
            ++pair_comparisons;
        }
    }
    Cpu initial{}; initial.pc = 0x30000; initial.npc = initial.pc + 4;
    initial.r[4] = 0x100000; initial.r[2] = 0xAABBCCDD;
    set_ram32(initial.r[4], 0x12345678); set_ram32(initial.pc, fixtures::queued_plant_word);
    Cpu expected = initial; std::int64_t one = 1; execute(expected, one);
    Cpu wrong = initial; native(wrong, fixtures::queued_plant);
    queue_plant_refused = std::memcmp(&wrong, &expected, sizeof wrong) != 0;
    if (!queue_plant_refused) ++failures;
    std::ofstream out(argv[1]);
    out << "{\n  \"status\": \"" << (failures ? "REFUSED" : "PASS") << "\",\n"
        << "  \"classification\": \"reference compatibility capability only\",\n"
        << "  \"comparisons\": " << comparisons << ",\n  \"instruction_shapes\": " << fixtures::count << ",\n"
        << "  \"unaligned_pair_step_comparisons\": " << pair_comparisons << ",\n"
        << "  \"queued_load_plant_rejected\": " << queue_plant_refused << ",\n  \"failures\": " << failures << ",\n"
        << "  \"native_credit\": 0\n}\n";
    return failures ? 1 : 0;
}
