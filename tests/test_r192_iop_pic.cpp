// Capability-only PIC comparison against the actual original IRX loader and
// frozen R192 interpreter. No module thread/game loop/card is executed.
#include "iop/r192_pic.hpp"
#include "iop/kernel.hpp"
#include "ps2/system.hpp"
#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace ps2::iop { void execute(Cpu &, std::int64_t &); }
namespace ps2::iop::kernel { void boot(); }
namespace {
namespace aot = ps2::iop::r192_aot;
using ps2::iop::Cpu;
struct Write { std::uint32_t address, width, value; bool operator==(const Write &) const = default; };
std::vector<Write> writes;
bool observe{};
unsigned failures{};
void check(bool yes, const char *name) { if (!yes) { ++failures; std::fprintf(stderr, "FAIL %s\n", name); } }
void reference(Cpu &c, unsigned n) { while (n--) { std::int64_t one = 1; ps2::iop::execute(c, one); } }
void reset(const std::vector<std::uint8_t> &seed) {
    observe = false; std::memcpy(ps2::iop::g_ram, seed.data(), seed.size()); writes.clear();
}
struct Patches { std::vector<std::array<std::uint32_t, 3>> pairs; };
bool validate(void *p, std::uint32_t address, const aot::Import &) {
    for (const auto &pair : static_cast<Patches *>(p)->pairs)
        if (pair[0] == address)
            return ps2::iop::ram32(address) == pair[1] && ps2::iop::ram32(address + 4) == pair[2];
    return false;
}
}
extern "C" void __real__ZN3ps23iop6write8Ejh(std::uint32_t, std::uint8_t);
extern "C" void __real__ZN3ps23iop7write16Ejt(std::uint32_t, std::uint16_t);
extern "C" void __real__ZN3ps23iop7write32Ejj(std::uint32_t, std::uint32_t);
extern "C" void __wrap__ZN3ps23iop6write8Ejh(std::uint32_t a, std::uint8_t v) {
    if (observe) writes.push_back({a, 1, v}); __real__ZN3ps23iop6write8Ejh(a, v);
}
extern "C" void __wrap__ZN3ps23iop7write16Ejt(std::uint32_t a, std::uint16_t v) {
    if (observe) writes.push_back({a, 2, v}); __real__ZN3ps23iop7write16Ejt(a, v);
}
extern "C" void __wrap__ZN3ps23iop7write32Ejj(std::uint32_t a, std::uint32_t v) {
    if (observe) writes.push_back({a, 4, v}); __real__ZN3ps23iop7write32Ejj(a, v);
}

int main(int argc, char **argv) {
    if (argc != 4 && argc != 5) return 2;
    static_assert(sizeof(Cpu) == 156);
    std::ifstream input(argv[1], std::ios::binary);
    const std::vector<std::uint8_t> elf{std::istreambuf_iterator<char>(input), {}};
    const auto base = static_cast<std::uint32_t>(std::stoul(argv[2], nullptr, 0));
    const auto alias = argc == 5 ? static_cast<std::uint32_t>(std::stoul(argv[4], nullptr, 0)) : 0u;
    if (alias != 0u && alias != 0x80000000u && alias != 0xA0000000u) return 2;
    const auto &source = aot::selected_pic_template();
    check(!elf.empty() && source.text_size == 4912, "pinned_representative_IRX_input");
    check(ps2::g_options.memcard_path.empty(), "no_card_configured");
    ps2::iop::kernel::boot(); // Original HLE/service registrations; no CPU run.
    check(ps2::g_options.memcard_path.empty(), "no_card_after_kernel_setup");
    if (base > 0x20000u)
        check(ps2::iop::kernel::alloc(base - 0x20000u) == 0x20000u, "actual_loader_prefix_reservation");
    const auto id = ps2::iop::load_module_buffer(elf, "SDRDRV.IRX", "");
    const auto found = ps2::iop::kernel::modules().find(id);
    check(found != ps2::iop::kernel::modules().end(), "actual_loader_module_registered");
    if (failures) return 3;
    const auto loaded = found->second;
    check(loaded.base == base && loaded.name == "SDRDRV", "previously_unobserved_actual_loader_base");
    auto *thread = ps2::iop::kernel::thread(1);
    check(thread && thread->entry == loaded.entry, "actual_loader_created_entry_context");
    if (failures) return 4;
    Cpu captured = thread->context;
    captured.pc |= alias; captured.npc |= alias; // Physical loader GP/SP/data remain unchanged.
    aot::BoundPic bound;
    check(bound.bind(source, loaded.base), "compiled_original_relocation_program_bound");
    const auto &module = bound.module;
    check(module.original_words[0xDC8 / 4] == 0x3C02FFFFu && module.original_words[0xDCC / 4] == 0x34427000u,
          "ordinary_integer_immediates_unchanged");
    Patches patches;
    for (unsigned i = 0; i < module.import_count; ++i) {
        const auto a = base + module.imports[i].offset;
        patches.pairs.push_back({a, ps2::iop::ram32(a), ps2::iop::ram32(a + 4)});
    }
    const aot::Services services{&patches, validate, nullptr};
    check(aot::matches(module, services), "PIC_expected_words_equal_actual_original_loader");
    if (failures) return 5;
    const std::vector<std::uint8_t> seed(ps2::iop::g_ram, ps2::iop::g_ram + ps2::iop::kRamSize);
    const std::array ids{0u, 0x8FFFu, 0x9000u, 0x9001u, 0x900Fu, 0x9010u, 0x90EFu,
                         0x90F0u, 0x90F1u, 0x90FFu, 0x9100u, 0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFFu};
    const std::array values{0u, 0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFFu};
    unsigned comparisons{}, unequal{}, instruction_count{}, stores{};
    for (auto handle : ids) for (auto value : values) {
        Cpu initial = captured;
        initial.r[0] = 0; initial.r[4] = handle; initial.r[5] = value; initial.r[31] = alias | 0x1F000u;
        initial.pc = alias | (base + 0xDC8); initial.npc = initial.pc + 4;
        Cpu native = initial;
        reset(seed); aot::Pipeline pipeline{}; observe = true;
        const auto result = aot::dispatch(module, native, pipeline, 64, services);
        observe = false;
        const auto native_writes = writes;
        const std::vector<std::uint8_t> native_ram(ps2::iop::g_ram, ps2::iop::g_ram + ps2::iop::kRamSize);
        reset(seed); Cpu expected = initial; observe = true;
        reference(expected, result.instructions); observe = false;
        const bool equal = result.status == aot::Status::external_destination &&
            native.pc == initial.r[31] && !pipeline.pending_load &&
            std::memcmp(&expected, &native, sizeof(Cpu)) == 0 &&
            std::memcmp(native_ram.data(), ps2::iop::g_ram, native_ram.size()) == 0 && native_writes == writes;
        if (!equal) ++unequal;
        ++comparisons; instruction_count += result.instructions; stores += static_cast<unsigned>(native_writes.size());
    }
    // Original loader-generated argc/argv/SP/GP, up to the first unbound import.
    reset(seed); Cpu entry = captured; aot::Pipeline pipeline{}; observe = true;
    const auto prefix = aot::dispatch(module, entry, pipeline, 4096, services); observe = false;
    const auto entry_writes = writes;
    const std::vector<std::uint8_t> entry_ram(ps2::iop::g_ram, ps2::iop::g_ram + ps2::iop::kRamSize);
    reset(seed); Cpu expected = captured; observe = true; reference(expected, prefix.instructions); observe = false;
    const bool prefix_equal = prefix.status == aot::Status::import_unbound && prefix.import &&
        std::memcmp(&expected, &entry, sizeof(Cpu)) == 0 && entry_writes == writes &&
        std::memcmp(entry_ram.data(), ps2::iop::g_ram, entry_ram.size()) == 0;
    check(prefix_equal, "actual_loader_entry_context_prefix_equivalence");
    check(unequal == 0, "PIC_complete_export_CPU_RAM_ordered_writes");
    std::ofstream out(argv[3]);
    out << "{\n  \"status\": \"" << (failures ? "REFUSED" : "PASS") << "\",\n"
        << "  \"classification\": \"capability only\",\n  \"base\": " << base << ",\n"
        << "  \"execution_alias\": " << alias << ",\n"
        << "  \"whole_export5_comparisons\": " << comparisons << ",\n  \"unequal\": " << unequal << ",\n"
        << "  \"instructions\": " << instruction_count << ",\n  \"ordered_stores\": " << stores << ",\n"
        << "  \"module_entry_prefix_instructions\": " << prefix.instructions << ",\n"
        << "  \"whole_module_complete\": false,\n  \"native_credit\": 0\n}\n";
    return failures ? 1 : 0;
}
