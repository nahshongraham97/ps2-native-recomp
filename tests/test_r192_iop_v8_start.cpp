// The original, unchanged v8 kernel is compiled here so this TEST can inspect
// actual registration identities, not guess HLE IDs or admit unknown callbacks.
// No kernel is copied into production and the protected candidate is untouched.
#ifndef R192_REFERENCE_KERNEL_SOURCE
#error Explicit pinned original kernel source required
#endif
#include R192_REFERENCE_KERNEL_SOURCE
#include "iop/r192_pic.hpp"
#include "ps2/system.hpp"
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace {
namespace aot = ps2::iop::r192_aot;
using ps2::iop::Cpu;
using namespace ps2::iop;
std::set<std::uint32_t> known_boot_ids;
bool native_service{};
bool observe{};
unsigned service_calls{}, forbidden_interpreter_calls{};
std::ofstream trace;
std::string refusal;
void write_bytes(const void *p, std::size_t size) { trace.write(static_cast<const char *>(p), size); }
std::uint64_t ram_hash() {
    std::uint64_t h = 14695981039346656037ull;
    for (std::uint32_t i = 0; i < kRamSize; ++i) { h ^= g_ram[i]; h *= 1099511628211ull; }
    return h;
}
void checkpoint(const Cpu &cpu) {
    write_bytes(&cpu, sizeof cpu); const auto hash = ram_hash(); write_bytes(&hash, sizeof hash);
}
void service(std::uint32_t id, Cpu &cpu) {
    if (id >= kernel::g_hle.size()) throw std::runtime_error("invalid_HLE_callback_id_refused");
    if (!known_boot_ids.count(id))
        throw std::runtime_error("unknown_or_synthesized_HLE_callback_refused:" + kernel::g_hle[id].library + "#" + std::to_string(kernel::g_hle[id].index));
    const auto &entry = kernel::g_hle[id];
    if (observe) {
        const std::uint32_t marker = 0x484C4521u, size = static_cast<std::uint32_t>(entry.library.size());
        write_bytes(&marker, 4); write_bytes(&size, 4); write_bytes(entry.library.data(), size);
        write_bytes(&entry.index, 4); checkpoint(cpu);
    }
    ++service_calls;
    kernel::g_hle[id].function(cpu); // Actual, pre-existing registered native body.
    if (observe) checkpoint(cpu);
}
#ifdef R192_USE_PRIVATE_AOT
void observe_actual_service(std::uint32_t id, Cpu &cpu, bool before) {
    if (id >= kernel::g_hle.size() || kernel::g_r192_synthesized_imports.count(id))
        throw std::runtime_error("unknown_actual_native_registration_refused");
    const auto &entry = kernel::g_hle[id];
    if (before) {
        if (observe) {
            const std::uint32_t marker = 0x484C4521u, size = static_cast<std::uint32_t>(entry.library.size());
            write_bytes(&marker, 4); write_bytes(&size, 4); write_bytes(entry.library.data(), size);
            write_bytes(&entry.index, 4); checkpoint(cpu);
        }
        ++service_calls;
    } else if (observe) checkpoint(cpu);
}
#endif
bool validate(void *, std::uint32_t address, const aot::Import &entry) {
    const auto word = ram32(address), delay = ram32(address + 4);
    if ((word >> 26) != kHleOpcode || delay != (0x24000000u | entry.ordinal)) return false;
    const auto id = word & 0x03FFFFFFu;
    // This validates loader-patched CODE IDENTITY only. It does not admit an
    // unknown callback for execution; invoke()/service() require the independent
    // pre-load whitelist and never run the loader's synthesized success body.
    return id < kernel::g_hle.size() &&
           kernel::g_hle[id].library == entry.library && kernel::g_hle[id].index == entry.ordinal;
}
bool invoke(void *, const aot::Import &entry, Cpu &cpu) {
    const auto found = kernel::g_hle_ids.find({entry.library, entry.ordinal});
    if (found == kernel::g_hle_ids.end()) return false;
    native_service = true;
    try { service(found->second, cpu); } catch (...) { native_service = false; throw; }
    native_service = false;
    return true;
}
void save(const std::string &path, const void *p, std::size_t n) {
    std::ofstream out(path, std::ios::binary); out.write(static_cast<const char *>(p), n);
}
}
#ifdef R192_USE_PRIVATE_AOT
namespace ps2::iop { void reference_execute(Cpu &, std::int64_t &); }
namespace ps2::iop::kernel {
void r192_test_hle_observer(std::uint32_t id, Cpu &cpu, bool before) { observe_actual_service(id, cpu, before); }
}
#endif
extern "C" void __real__ZN3ps23iop7executeERNS0_3CpuERx(Cpu &, std::int64_t &);
extern "C" void __wrap__ZN3ps23iop7executeERNS0_3CpuERx(Cpu &cpu, std::int64_t &budget) {
    if (native_service) { ++forbidden_interpreter_calls; throw std::runtime_error("native_HLE_requires_unbound_guest_execution"); }
    __real__ZN3ps23iop7executeERNS0_3CpuERx(cpu, budget);
}
extern "C" void __wrap__ZN3ps23iop8call_hleERNS0_3CpuEj(Cpu &cpu, std::uint32_t id) { service(id, cpu); }

extern "C" void __real__ZN3ps23iop6write8Ejh(std::uint32_t, std::uint8_t);
extern "C" void __real__ZN3ps23iop7write16Ejt(std::uint32_t, std::uint16_t);
extern "C" void __real__ZN3ps23iop7write32Ejj(std::uint32_t, std::uint32_t);
void record_write(std::uint32_t a, std::uint32_t width, std::uint32_t value) {
    if (!observe) return;
    const std::uint32_t marker = 0x57524954u; write_bytes(&marker, 4); write_bytes(&a, 4); write_bytes(&width, 4); write_bytes(&value, 4);
}
extern "C" void __wrap__ZN3ps23iop6write8Ejh(std::uint32_t a, std::uint8_t v) { record_write(a, 1, v); __real__ZN3ps23iop6write8Ejh(a, v); }
extern "C" void __wrap__ZN3ps23iop7write16Ejt(std::uint32_t a, std::uint16_t v) { record_write(a, 2, v); __real__ZN3ps23iop7write16Ejt(a, v); }
extern "C" void __wrap__ZN3ps23iop7write32Ejj(std::uint32_t a, std::uint32_t v) { record_write(a, 4, v); __real__ZN3ps23iop7write32Ejj(a, v); }

int main(int argc, char **argv) {
    if (argc < 5 || argc > 7) return 2;
    static_assert(sizeof(Cpu) == 156);
    const bool compiled = std::string(argv[3]) == "compiled";
    if (!compiled && std::string(argv[3]) != "reference") return 2;
    std::ifstream input(argv[1], std::ios::binary);
    const std::vector<std::uint8_t> elf{std::istreambuf_iterator<char>(input), {}};
    const auto base = static_cast<std::uint32_t>(std::stoul(argv[2], nullptr, 0));
    const std::string output = argv[4];
    const auto alias = argc >= 6 ? static_cast<std::uint32_t>(std::stoul(argv[5], nullptr, 0)) : 0u;
    const bool plant_unknown = argc == 7 && std::string(argv[6]) == "--planted-unknown-import";
    if (argc == 7 && !plant_unknown) return 2;
    if (alias != 0 && alias != 0x80000000u && alias != 0xA0000000u) return 2;
    unsigned instructions{};
    bool returned{};
    std::string serialized = "not_reached";
    Cpu cpu{};
    try {
        if (!ps2::g_options.memcard_path.empty()) throw std::runtime_error("card_configuration_refused");
        kernel::boot();
        for (const auto &[key, id] : kernel::g_hle_ids) known_boot_ids.insert(id);
        if (base > 0x20000 && kernel::alloc(base - 0x20000) != 0x20000) throw std::runtime_error("base_reservation_failed");
        const auto id = load_module_buffer(elf, "SDRDRV.IRX", "");
        const auto loaded = kernel::modules().at(id);
        if (loaded.base != base || !kernel::thread(1)) throw std::runtime_error("actual_loader_context_missing");
        cpu = kernel::thread(1)->context;
        cpu.pc |= alias; cpu.npc |= alias;
        const auto exit = cpu.r[31];
        aot::BoundPic bound;
        if (!bound.bind(aot::selected_pic_template(), base)) throw std::runtime_error("PIC_binding_failed");
        const aot::Services services{nullptr, validate, invoke};
        if (!aot::matches(bound.module, services)) throw std::runtime_error("original_code_or_known_import_identity_refused");
        if (plant_unknown) {
            bool selected = false;
            for (std::uint32_t i = 0; i < bound.module.import_count; ++i) {
                const auto at = base + bound.module.imports[i].offset;
                const auto word = ram32(at);
                if ((word >> 26) == kHleOpcode && !known_boot_ids.count(word & 0x03FFFFFFu)) {
                    cpu.pc = alias | at; cpu.npc = cpu.pc + 4; selected = true; break;
                }
            }
            if (!selected) throw std::runtime_error("actual_unknown_import_control_unavailable");
        }
        aot::Pipeline pipeline{};
        trace.open(output + ".trace", std::ios::binary); observe = true; checkpoint(cpu);
        while (instructions < 100000 && cpu.pc != exit) {
            if (compiled) {
#ifdef R192_USE_PRIVATE_AOT
                std::int64_t one = 1; ps2::iop::kernel::execute_aot_v8(cpu, one);
#else
                const auto r = aot::dispatch(bound.module, cpu, pipeline, 1, services);
                if (r.instructions != 1 || r.status != aot::Status::budget_exhausted)
                    throw std::runtime_error(std::string("named_AOT_frontier:") + r.reason);
                if (pipeline.pending_load) throw std::runtime_error("queued_load_leaked_into_v8_compatibility");
#endif
            } else {
                std::int64_t one = 1;
#ifdef R192_USE_PRIVATE_AOT
                ps2::iop::reference_execute(cpu, one);
#else
                ps2::iop::execute(cpu, one);
#endif
            }
            ++instructions; checkpoint(cpu);
        }
        observe = false; trace.close();
        returned = cpu.pc == exit;
        if (!returned) throw std::runtime_error("whole_start_instruction_limit");
        save(output + ".cpu", &cpu, sizeof cpu); save(output + ".ram", g_ram, kRamSize);
        // The manual function-call harness does not run scheduler on_exit.
        // Exclude only that known pending host closure so the ORIGINAL IOP
        // serializer can measure all its serializable RAM/kernel/device state.
        kernel::thread(1)->on_exit = nullptr; kernel::thread(1)->context = cpu; g_cpu = cpu;
        ps2::state::Writer writer; writer.set_version(8); ps2::iop::state(writer);
        save(output + ".iop-state", writer.out.data(), writer.out.size()); serialized = "original_IOP_serializer_complete_excluding_manual_on_exit";
    } catch (const std::exception &e) {
        refusal = e.what(); observe = false; if (trace.is_open()) trace.close();
    }
    std::ofstream result(output + ".json");
    result << "{\n  \"status\": \"" << (refusal.empty() ? "PASS" : "REFUSED") << "\",\n"
           << "  \"classification\": \"reference compatibility capability only\",\n"
           << "  \"execution_alias\": " << alias << ",\n"
           << "  \"whole_start_returned\": " << (returned ? "true" : "false") << ",\n"
           << "  \"instructions\": " << instructions << ",\n  \"native_service_calls\": " << service_calls << ",\n"
           << "  \"forbidden_interpreter_calls\": " << forbidden_interpreter_calls << ",\n"
           << "  \"synthesized_success_callbacks_executed\": " << kernel::g_missing_reported.size() << ",\n"
           << "  \"serializer\": \"" << serialized << "\",\n  \"refusal\": \"" << refusal << "\",\n"
           << "  \"native_credit\": 0\n}\n";
    return refusal.empty() ? 0 : 1;
}
