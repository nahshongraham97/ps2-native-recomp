// Independent original THREADMAN method executes in the unchanged v8 oracle.
// Native query has no guest addresses. All RAM/frame fixtures are constructed
// inside this test process; no game loop/card is used.
#ifndef R192_REFERENCE_KERNEL_SOURCE
#error Pinned reference source required
#endif
#include R192_REFERENCE_KERNEL_SOURCE
#include "iop/r192_thbase42.hpp"
#include "ps2/system.hpp"
#include <fstream>
#include <iterator>
#include <optional>

namespace {
using namespace ps2::iop;
unsigned failures{};
struct Write { std::uint32_t address, width, value; bool operator==(const Write &) const = default; };
std::vector<Write> writes;
bool observe{};
void check(bool yes, const char *name) { if (!yes) { ++failures; std::fprintf(stderr, "FAIL %s\n", name); } }
std::vector<std::uint8_t> read(const char *name) { std::ifstream in(name, std::ios::binary); return {std::istreambuf_iterator<char>(in), {}}; }
}
extern "C" void __real__ZN3ps23iop6write8Ejh(std::uint32_t, std::uint8_t);
extern "C" void __real__ZN3ps23iop7write16Ejt(std::uint32_t, std::uint16_t);
extern "C" void __real__ZN3ps23iop7write32Ejj(std::uint32_t, std::uint32_t);
extern "C" void __wrap__ZN3ps23iop6write8Ejh(std::uint32_t a, std::uint8_t v) { if (observe) writes.push_back({a, 1, v}); __real__ZN3ps23iop6write8Ejh(a, v); }
extern "C" void __wrap__ZN3ps23iop7write16Ejt(std::uint32_t a, std::uint16_t v) { if (observe) writes.push_back({a, 2, v}); __real__ZN3ps23iop7write16Ejt(a, v); }
extern "C" void __wrap__ZN3ps23iop7write32Ejj(std::uint32_t a, std::uint32_t v) { if (observe) writes.push_back({a, 4, v}); __real__ZN3ps23iop7write32Ejj(a, v); }

int main(int argc, char **argv) {
    if (argc != 3 && argc != 4) return 2;
    static_assert(sizeof(Cpu) == 156);
    std::optional<int> saved_priority;
    int saved_thread{};
    if (argc == 4) {
        auto state = read(argv[3]); kernel::install_sif();
        check(ps2::g_options.memcard_path.empty(), "no_card_before_state_parser");
        check(ps2::load_state(state.data(), state.size()), "actual_byte_state_parser");
        check(ps2::g_options.memcard_path.empty(), "no_card_after_state_parser");
        if (const auto *thread = kernel::current()) { saved_priority = thread->priority; saved_thread = thread->id; }
    }
    kernel::boot();
    r192_thbase42::install();
    const auto original = read(argv[1]);
    check(kernel::alloc(0x7000) == 0x20000, "actual_loader_prefix_allocation");
    const auto id = load_module_buffer(original, "REFERENCE_THREADMAN.IRX", "");
    check(id > 0, "actual_original_loader");
    if (failures) return 3;
    const auto module = kernel::modules().at(id);
    check(module.base == 0x27000 && ram32(module.base + 0x686C) == module.base + 0x1EE8,
          "original_export42_pointer_relocation");
    check(ram32(module.base + 0x1F0C) == 0x8602000Eu, "original_LH_signed16_priority_load");
    const auto native_id = kernel::g_hle_ids.at({"thbase", 42});
    std::vector<int> values{0, 1, 8, 9, 123, 126, -1, -32768, 32767};
    if (saved_priority) values.push_back(*saved_priority);
    unsigned comparisons{}, reference_spill_stores{}, sign_controls{}, irq_controls{};
    const auto tcb = 0x500000u, stack = 0x700000u;
    for (const auto value : values) for (const bool irq : {false, true}) {
        kernel::g_current = 1; kernel::g_nesting = irq ? 1 : 0;
        kernel::thread(1)->priority = value;
        std::memset(ram_ptr(tcb), 0xAD, 64); write16(tcb + 14, static_cast<std::uint16_t>(value));
        set_ram32(module.base + 0x7080, tcb);
        std::memset(ram_ptr(stack - 40), 0xCD, 64);
        Cpu initial{};
        for (unsigned r = 1; r < 32; ++r) initial.r[r] = 0xA5A50000u + r;
        initial.r[16] = 0x13579BDF; initial.r[29] = stack; initial.r[31] = 0x1F000;
        initial.pc = module.base + 0x1EE8; initial.npc = initial.pc + 4; initial.status = 0x401;
        Cpu reference = initial; writes.clear(); observe = true;
        unsigned steps{};
        while (reference.pc != initial.r[31] && steps < 64) { std::int64_t one = 1; execute(reference, one); ++steps; }
        observe = false;
        check(reference.pc == initial.r[31] && steps == (irq ? 14u : 15u), "whole_original_method_return");
        check(writes == std::vector<Write>{{stack - 8, 4, initial.r[16]}, {stack - 4, 4, initial.r[31]}}, "exact_original_frame_transport_writes");
        reference_spill_stores += static_cast<unsigned>(writes.size());
        const auto expected = irq ? -100 : static_cast<std::int32_t>(static_cast<std::int16_t>(value));
        check(reference.r[2] == static_cast<std::uint32_t>(expected), "independent_original_method_result");
        Cpu native = initial; native.pc = initial.r[31]; native.npc = native.pc + 4;
        writes.clear(); observe = true; kernel::g_hle[native_id].function(native); observe = false;
        check(std::memcmp(&native, &reference, sizeof(Cpu)) == 0 && writes.empty(), "full_CPU_and_native_query_no_guest_writes");
        check(read16(tcb + 14) == static_cast<std::uint16_t>(value), "TCB_priority_not_modified");
        for (unsigned n = 0; n < 64; ++n)
            if (n < 32 || n >= 40) check(ram_ptr(stack - 40)[n] == 0xCD, "adjacent_frame_canaries");
        if (!irq && value < 0 && static_cast<std::uint32_t>(static_cast<std::uint16_t>(value)) != reference.r[2]) ++sign_controls;
        if (irq && reference.r[2] != 0u) ++irq_controls;
        ++comparisons;
    }
    // The original checks interrupt context before dereferencing the TCB
    // priority. Prove the error path also with no native running thread and
    // an original null current-TCB pointer, instead of inventing a zero value.
    kernel::g_nesting = 1; kernel::g_current = 0;
    set_ram32(module.base + 0x7080, 0);
    Cpu irq_initial{}; irq_initial.pc = module.base + 0x1EE8; irq_initial.npc = irq_initial.pc + 4;
    irq_initial.r[16] = 0x13579BDF; irq_initial.r[29] = stack; irq_initial.r[31] = 0x1F000; irq_initial.status = 0x401;
    Cpu irq_reference = irq_initial; writes.clear(); observe = true;
    unsigned irq_steps{};
    while (irq_reference.pc != irq_initial.r[31] && irq_steps < 64) { std::int64_t one = 1; execute(irq_reference, one); ++irq_steps; }
    observe = false; reference_spill_stores += static_cast<unsigned>(writes.size());
    Cpu irq_native = irq_initial; irq_native.pc = irq_initial.r[31]; irq_native.npc = irq_native.pc + 4;
    kernel::g_hle[native_id].function(irq_native);
    check(irq_steps == 14 && irq_reference.r[2] == 0xFFFFFF9Cu && std::memcmp(&irq_native, &irq_reference, sizeof(Cpu)) == 0,
          "original_IRQ_error_with_no_running_thread");
    ++comparisons; ++irq_controls;
    bool missing_refused{}, wide_refused{};
    kernel::g_nesting = 0; kernel::g_current = 0;
    Cpu unknown{}; unknown.r[2] = 0xD15EA5ED;
    try { kernel::g_hle[native_id].function(unknown); }
    catch (const std::runtime_error &e) { missing_refused = std::string(e.what()) == "IOP_THBASE42_REFUSED:no_running_thread" && unknown.r[2] == 0xD15EA5ED; }
    kernel::g_current = 1; kernel::thread(1)->priority = 32768;
    try { kernel::g_hle[native_id].function(unknown); }
    catch (const std::runtime_error &e) { wide_refused = std::string(e.what()) == "IOP_THBASE42_REFUSED:priority_not_proved_signed16" && unknown.r[2] == 0xD15EA5ED; }
    check(missing_refused && wide_refused && sign_controls > 0 && irq_controls > 0, "missing_width_sign_and_IRQ_controls");
    check(kernel::g_missing_reported.empty(), "no_unknown_reference_callback_executed");
    std::ofstream out(argv[2]);
    out << "{\n  \"status\": \"" << (failures ? "REFUSED" : "PASS") << "\",\n"
        << "  \"classification\": \"source-supported native-service capability only\",\n"
        << "  \"whole_original_method_comparisons\": " << comparisons << ",\n"
        << "  \"reference_frame_spill_stores\": " << reference_spill_stores << ",\n"
        << "  \"signed16_unsigned_plant_rejections\": " << sign_controls << ",\n"
        << "  \"IRQ_zero_plant_rejections\": " << irq_controls << ",\n"
        << "  \"no_thread_zero_refused\": " << missing_refused << ",\n  \"unproved_width_refused\": " << wide_refused << ",\n"
        << "  \"snapshot_thread\": " << saved_thread << ",\n  \"snapshot_priority\": " << (saved_priority ? std::to_string(*saved_priority) : "null") << ",\n"
        << "  \"unknown_fallback_calls\": " << kernel::g_missing_reported.size() << ",\n"
        << "  \"failures\": " << failures << ",\n"
        << "  \"native_credit\": 0\n}\n";
    return failures ? 1 : 0;
}
