// Standalone component test, linked with the frozen R192 runtime as reference.
// No boot, card mount, game run loop or production binary modification.
#include "iop/r192_aot.hpp"
#include "iop/kernel.hpp"
#include "ps2/system.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace ps2::iop { void execute(Cpu &, std::int64_t &); }
namespace {
using ps2::iop::Cpu;
namespace aot = ps2::iop::r192_aot;
struct Write {
    std::uint32_t address, width, value;
    bool operator==(const Write &) const = default;
};
std::vector<Write> writes;
bool observe = false;
bool plant_width = false;
int failures = 0;
void check(bool ok, const char *what) {
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", what); }
}
std::vector<std::uint8_t> read(const char *path) {
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), {}};
}
struct Patches { std::vector<std::array<std::uint32_t, 3>> pairs; };
bool validate(void *user, std::uint32_t address, const aot::Import &) {
    const auto &patches = *static_cast<Patches *>(user);
    for (const auto &pair : patches.pairs)
        if (pair[0] == address)
            return ps2::iop::ram32(address) == pair[1] && ps2::iop::ram32(address+4) == pair[2];
    return false;
}
void reset_ram(const std::vector<std::uint8_t> &seed) {
    observe = false;
    std::memcpy(ps2::iop::g_ram, seed.data(), seed.size());
    writes.clear();
}
void reference(Cpu &cpu, std::uint32_t instructions) {
    for (std::uint32_t i = 0; i < instructions; ++i) {
        std::int64_t one = 1;
        ps2::iop::execute(cpu, one);
    }
}
} // namespace

extern "C" void __real__ZN3ps23iop6write8Ejh(std::uint32_t, std::uint8_t);
extern "C" void __real__ZN3ps23iop7write16Ejt(std::uint32_t, std::uint16_t);
extern "C" void __real__ZN3ps23iop7write32Ejj(std::uint32_t, std::uint32_t);
extern "C" void __wrap__ZN3ps23iop6write8Ejh(std::uint32_t address, std::uint8_t value) {
    if (observe) writes.push_back({address, 1, value});
    __real__ZN3ps23iop6write8Ejh(address, value);
}
extern "C" void __wrap__ZN3ps23iop7write16Ejt(std::uint32_t address, std::uint16_t value) {
    if (observe) writes.push_back({address, 2, value});
    __real__ZN3ps23iop7write16Ejt(address, value);
}
extern "C" void __wrap__ZN3ps23iop7write32Ejj(std::uint32_t address, std::uint32_t value) {
    if (observe && plant_width) {
        writes.push_back({address, 2, value & 0xFFFFu});
        __real__ZN3ps23iop7write16Ejt(address, static_cast<std::uint16_t>(value));
    } else {
        if (observe) writes.push_back({address, 4, value});
        __real__ZN3ps23iop7write32Ejj(address, value);
    }
}

int main(int argc, char **argv) {
    if (argc < 3 || argc > 4) return 2;
    static_assert(sizeof(Cpu) == 156, "Use the actual frozen R192 reference ABI");
    static_assert(sizeof(Cpu::r[0]) == 4);
    const bool planted_width = argc == 4 && std::string(argv[3]) == "--planted-store-width";
    const auto bytes = read(argv[1]);
    check(!bytes.empty(), "saved_state_read");
    check(ps2::g_options.memcard_path.empty(), "no_card_mounted_before");
    ps2::iop::kernel::install_sif(); // Original host RPC factories, no boot/run.
    check(ps2::load_state(bytes.data(), bytes.size()), "actual_runtime_full_state_parser");
    check(ps2::g_options.memcard_path.empty(), "no_card_mounted_after");
    if (failures) return 3;
    const auto &module = aot::selected_module();
    bool module_found = false;
    for (const auto &[id, loaded] : ps2::iop::kernel::modules())
        if (loaded.name == module.name && loaded.base == module.base && loaded.size == module.total_size) module_found = true;
    check(module_found, "actual_saved_module_identity_and_base");
    const Cpu captured = ps2::iop::g_cpu;
    std::vector<std::uint8_t> seed(ps2::iop::g_ram, ps2::iop::g_ram+ps2::iop::kRamSize);
    Patches patches;
    for (std::uint32_t i = 0; i < module.import_count; ++i) {
        const auto address = module.base+module.imports[i].offset;
        patches.pairs.push_back({address, ps2::iop::ram32(address), ps2::iop::ram32(address+4)});
    }
    const aot::Services services{&patches, validate, nullptr};
    check(aot::matches(module, services), "original_text_with_exact_captured_import_patches");
    check(!aot::matches(module), "patched_imports_without_identity_provider_refused");
    check(module.text_size == 4912 && module.original_words[0xDC8/4] == 0x3C02FFFFu &&
          module.original_words[0xDDC/4] == 0x308300F0u &&
          module.original_words[0xDF8/4] == 0xAC2510F0u, "byte_joined_export5_body");
    if (failures) return 4;
    const std::array ids{0u, 0x8FFFu, 0x9000u, 0x9001u, 0x900Fu, 0x9010u, 0x90EFu,
                         0x90F0u, 0x90F1u, 0x90FFu, 0x9100u, 0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFFu};
    const std::array values{0u, 0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFFu};
    std::uint32_t comparisons = 0, valid_cases = 0, invalid_cases = 0, ordered_writes = 0;
    std::uint32_t detected_width_cases = 0;
    for (const auto id : ids) for (const auto value : values) {
        Cpu initial = captured;
        initial.pc = module.base+0xDC8; initial.npc = initial.pc+4;
        initial.r[0] = 0; initial.r[4] = id; initial.r[5] = value;
        initial.r[3] = 0xAABBCCDDu; initial.r[31] = 0x0001F000u;
        reset_ram(seed);
        Cpu native = initial;
        aot::Pipeline pipeline{}; // Frozen v8 producer has no serialized load queue.
        observe = true; plant_width = planted_width;
        const auto ran = aot::dispatch(module, native, pipeline, 64, services);
        observe = false; plant_width = false;
        const auto native_writes = writes;
        std::vector<std::uint8_t> native_ram(ps2::iop::g_ram, ps2::iop::g_ram+ps2::iop::kRamSize);
        check(ran.status == aot::Status::external_destination && ran.instructions > 0 && native.pc == initial.r[31],
              "complete_export5_return_boundary");
        check(!pipeline.pending_load, "whole_export_return_queue_drained");
        reset_ram(seed);
        Cpu expected = initial;
        observe = true;
        reference(expected, ran.instructions);
        observe = false;
        const bool equal = std::memcmp(&expected, &native, sizeof(Cpu)) == 0 &&
                           std::memcmp(ps2::iop::g_ram, native_ram.data(), native_ram.size()) == 0 && writes == native_writes;
        if (planted_width && !equal) ++detected_width_cases;
        else check(equal, "full_CPU_RAM_and_ordered_write_equivalence");
        const bool valid = static_cast<std::uint32_t>(id-0x9000u) < 0xF1u;
        if (valid) { ++valid_cases; check(native_writes.size() == 1, "valid_export_has_one_store"); }
        else { ++invalid_cases; check(native_writes.empty() && native.r[2] == 0xFFFFFFFFu, "invalid_export_refuses_without_store"); }
        ordered_writes += static_cast<std::uint32_t>(native_writes.size());
        ++comparisons;
    }
    // Real module entry with the loader's argc/argv register convention. The
    // unimplemented import is a named boundary, never a fake module success.
    reset_ram(seed);
    Cpu entry = captured;
    entry.pc = module.base+0x3A0; entry.npc = entry.pc+4;
    entry.r[0] = 0; entry.r[4] = 1; entry.r[5] = 0x00500000u;
    ps2::iop::set_ram32(entry.r[5], 0x00500100u);
    std::memcpy(ps2::iop::ram_ptr(0x00500100u), "SDRDRV.IRX", 11);
    const std::vector<std::uint8_t> entry_seed(ps2::iop::g_ram, ps2::iop::g_ram+ps2::iop::kRamSize);
    const Cpu entry_initial = entry;
    aot::Pipeline entry_pipeline{};
    observe = true;
    const auto entry_result = aot::dispatch(module, entry, entry_pipeline, 4096, services);
    observe = false;
    const auto entry_writes = writes;
    const std::vector<std::uint8_t> entry_ram(ps2::iop::g_ram, ps2::iop::g_ram+ps2::iop::kRamSize);
    check(entry_result.status == aot::Status::import_unbound && entry_result.import && entry_result.instructions > 0,
          "module_entry_stops_at_unbound_import");
    reset_ram(entry_seed);
    Cpu entry_reference = entry_initial;
    observe = true; reference(entry_reference, entry_result.instructions); observe = false;
    check(std::memcmp(&entry_reference, &entry, sizeof(Cpu)) == 0 &&
          std::memcmp(ps2::iop::g_ram, entry_ram.data(), entry_ram.size()) == 0 && writes == entry_writes,
          "actual_module_entry_prefix_differential");
    // Actual original LW at +DEC followed by LUI at +DF0: expose the frozen
    // oracle's immediate-load gap, then verify equality after the real delay.
    reset_ram(seed);
    ps2::iop::set_ram32(module.base+0x14F0u, 0x13579BDFu);
    Cpu delayed = captured; delayed.r[0] = 0; delayed.pc = module.base+0xDEC;
    delayed.npc = delayed.pc+4; delayed.r[2] = (module.base+0x14F0u) & 0xFFFF0000u;
    delayed.r[31] = 0x1F000u;
    Cpu immediate = delayed;
    aot::Pipeline delayed_pipeline{};
    const auto first = aot::dispatch(module, delayed, delayed_pipeline, 1, services);
    reference(immediate, 1);
    const bool load_gap = first.instructions == 1 && delayed_pipeline.pending_load && delayed.r[2] != immediate.r[2];
    check(load_gap, "R3000_queued_load_vs_frozen_reference_gap_measured");
    const Cpu after_load = delayed;
    const auto after_load_pipeline = delayed_pipeline;
    aot::dispatch(module, delayed, delayed_pipeline, 1, services);
    reference(immediate, 1);
    check(std::memcmp(&immediate, &delayed, sizeof(Cpu)) == 0 && !delayed_pipeline.pending_load,
          "equality_after_actual_load_delay_instruction");
    Cpu lost = after_load;
    auto lost_pipeline = after_load_pipeline; lost_pipeline.pending_load = false;
    aot::dispatch(module, lost, lost_pipeline, 1, services);
    check(lost.r[2] != immediate.r[2], "planted_lost_pending_load_rejected");
    // Actual taken BEQ at +DD8, whose ANDI delay at +DDC must survive a
    // one-instruction budget boundary. JR/branch targets are captured before
    // their slots; restarting at the target would silently drop real work.
    reset_ram(seed);
    Cpu branch = captured; branch.r[0] = 0; branch.r[2] = 0;
    branch.r[3] = 0xAABBCCDDu; branch.r[4] = 0x900Fu;
    branch.pc = module.base+0xDD8; branch.npc = branch.pc+4;
    Cpu branch_expected = branch;
    aot::Pipeline branch_pipeline{};
    aot::dispatch(module, branch, branch_pipeline, 1, services);
    reference(branch_expected, 1);
    check(std::memcmp(&branch, &branch_expected, sizeof(Cpu)) == 0 &&
          branch.pc == module.base+0xDDC && branch.npc == module.base+0xE04,
          "taken_branch_budget_preserves_slot_and_target");
    const Cpu after_branch = branch;
    aot::dispatch(module, branch, branch_pipeline, 1, services);
    reference(branch_expected, 1);
    check(std::memcmp(&branch, &branch_expected, sizeof(Cpu)) == 0 && branch.r[3] == 0,
          "actual_delay_effect_before_saved_target");
    Cpu lost_branch = after_branch; lost_branch.npc = lost_branch.pc+4;
    aot::Pipeline lost_branch_pipeline{};
    aot::dispatch(module, lost_branch, lost_branch_pipeline, 1, services);
    const bool lost_branch_refused = lost_branch.pc != branch_expected.pc;
    check(lost_branch_refused, "planted_lost_branch_target_rejected");
    Cpu dropped_delay = after_branch; dropped_delay.pc = after_branch.npc; dropped_delay.npc = dropped_delay.pc+4;
    aot::Pipeline dropped_delay_pipeline{};
    aot::dispatch(module, dropped_delay, dropped_delay_pipeline, 1, services);
    const bool dropped_delay_refused = dropped_delay.r[3] != branch_expected.r[3];
    check(dropped_delay_refused, "planted_dropped_delay_effect_rejected");
    reset_ram(seed);
    Cpu guard = captured; guard.pc = module.base+0xDC8; guard.npc = guard.pc+4; guard.r[0] = 0;
    aot::Pipeline guard_pipeline{};
    ps2::iop::set_ram32(module.base+8, ps2::iop::ram32(module.base+8)^1u);
    const auto bad_code = aot::dispatch(module, guard, guard_pipeline, 8, services);
    check(bad_code.status == aot::Status::code_identity_mismatch && bad_code.instructions == 0, "planted_original_word_refused");
    reset_ram(seed);
    const auto &patch = patches.pairs[0]; ps2::iop::set_ram32(patch[0], patch[1]^1u);
    check(!aot::matches(module, services), "planted_import_target_refused");
    reset_ram(seed);
    guard.pc = 0x1F000; guard.npc = guard.pc+4;
    const auto outside = aot::dispatch(module, guard, guard_pipeline, 8, services);
    check(outside.status == aot::Status::external_destination && outside.instructions == 0, "uncompiled_PC_refused_without_fallback");
    if (planted_width) check(detected_width_cases > 0, "planted_width_must_be_observed");
    std::ofstream result(argv[2]);
    result << "{\n  \"status\":\"" << (failures ? "FAIL" : (planted_width ? "PLANTED_WIDTH_REJECTED" : "PASS")) << "\",\n"
           << "  \"saved_frame\":" << ps2::frame_count() << ",\n  \"captured_IOP_PC\":" << captured.pc
           << ",\n  \"whole_export5_comparisons\":" << comparisons << ",\n  \"valid_input_cases\":" << valid_cases
           << ",\n  \"invalid_input_cases\":" << invalid_cases << ",\n  \"ordered_stores_compared\":" << ordered_writes
           << ",\n  \"module_entry_prefix_instructions\":" << entry_result.instructions
           << ",\n  \"module_entry_boundary\":\"" << aot::status_name(entry_result.status) << "\",\n"
           << "  \"remaining_imports\":" << module.import_count
           << ",\n  \"reference_immediate_load_gap_measured\":" << (load_gap ? "true" : "false")
           << ",\n  \"actual_branch_budget_slice_comparisons\":2,\n  \"planted_lost_branch_target_rejected\":" << (lost_branch_refused ? "true" : "false")
           << ",\n  \"planted_dropped_delay_effect_rejected\":" << (dropped_delay_refused ? "true" : "false")
           << ",\n  \"planted_width_cases_detected\":" << detected_width_cases << ",\n  \"failures\":" << failures
           << ",\n  \"native_credit\":0,\n  \"scope\":\"Whole source export5 boundary plus actual IRX-entry prefix; snapshot-seeded constructed calls, not historical function-entry captures. R3000 queue initialized explicitly; frozen v8 producer has no queue. No whole-module or whole-IOP completion claim.\"\n}\n";
    if (!result) return 5;
    std::printf("comparisons=%u prefix_instructions=%u imports_remaining=%u expected_reference_load_gap=%d failures=%d\n",
                comparisons, entry_result.instructions, module.import_count, load_gap, failures);
    return failures ? 1 : (planted_width ? 1 : 0);
}
