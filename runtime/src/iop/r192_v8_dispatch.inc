// Included ONLY after an independently pinned, disposable original v8 kernel
// translation unit. Private registration metadata is actual loader authority.
// No opcode decoder, compiler, interpreter, or unbound success fallback exists.
#include "iop/r192_pic.hpp"
#include <memory>
#include <sstream>
#include <tuple>
#include <cstddef>

static_assert(sizeof(ps2::iop::Cpu) == 156, "Only the source-pinned v8 IOP ABI is supported");
static_assert(offsetof(ps2::iop::Cpu, pc) == 136 && offsetof(ps2::iop::Cpu, status) == 144);

namespace ps2::iop::kernel {
#ifdef R192_AOT_TEST_SERVICE_OBSERVER
void r192_test_hle_observer(std::uint32_t, Cpu &, bool);
#endif
namespace {
namespace aot_v8 = ps2::iop::r192_aot;
using BindingKey = std::tuple<int, std::string, std::uint32_t, std::uint32_t>;
std::map<BindingKey, std::pair<std::uint64_t, std::unique_ptr<aot_v8::BoundPic>>> g_r192_pic_bindings;
std::uint64_t g_r192_binding_generation{};
std::uint64_t g_r192_aot_steps{}, g_r192_hle_steps{};

[[noreturn]] void aot_refuse(const std::string &why, const Cpu &c) {
    std::ostringstream text;
    text << "IOP_AOT_REFUSED:" << why << " pc=" << std::hex << c.pc << " ra=" << c.r[31];
    std::fprintf(stderr, "[iop-aot-v8] %s\n", text.str().c_str());
    std::fflush(stderr);
    throw std::runtime_error(text.str());
}
bool known_hle_id(std::uint32_t id) {
    return id < g_hle.size() && !g_r192_synthesized_imports.count(id);
}
bool identity(void *, std::uint32_t address, const aot_v8::Import &entry) {
    const auto first = ram32(address), second = ram32(address + 4);
    if ((first >> 26) == kHleOpcode && second == (0x24000000u | entry.ordinal)) {
        const auto id = first & 0x03FFFFFFu;
        return id < g_hle.size() && g_hle[id].library == entry.library && g_hle[id].index == entry.ordinal;
    }
    const auto library = g_libraries.find(entry.library);
    if (library == g_libraries.end() || second != 0 || (first >> 26) != 2u) return false;
    const auto slot = static_cast<std::uint64_t>(phys(library->second)) + 20u + 4ull * entry.ordinal;
    if (slot + 4 > kRamSize) return false;
    const auto target = ram32(static_cast<std::uint32_t>(slot));
    return first == (0x08000000u | ((target >> 2) & 0x03FFFFFFu));
}
const aot_v8::Import *at_import(const aot_v8::Module &m, std::uint32_t offset) {
    for (std::uint32_t i = 0; i < m.import_count; ++i)
        if (m.imports[i].offset == offset) return &m.imports[i];
    return nullptr;
}
bool registered_kernel_stub(std::uint32_t address, std::uint32_t &id) {
    for (const auto &stub : g_hle_stubs)
        if (phys(stub.first) == address) {
            id = stub.second;
            return address + 8 <= kRamSize && ram32(address) == ((kHleOpcode << 26) | id) && ram32(address + 4) == 0;
        }
    return false;
}
void native_hle(Cpu &c, std::uint32_t id) {
    if (!known_hle_id(id)) {
        const auto key = id < g_hle.size() ? g_hle[id].library + "#" + std::to_string(g_hle[id].index) : "invalid_id";
        aot_refuse("unknown_or_synthesized_HLE:" + key, c);
    }
    const auto target = c.r[31];
    c.pc = target; c.npc = target + 4; c.r[0] = 0;
#ifdef R192_AOT_TEST_SERVICE_OBSERVER
    r192_test_hle_observer(id, c, true);
#endif
    g_hle[id].function(c); // Already compiled, provenance-tagged original service.
#ifdef R192_AOT_TEST_SERVICE_OBSERVER
    r192_test_hle_observer(id, c, false);
#endif
    c.r[0] = 0; ++g_r192_hle_steps;
}
aot_v8::BoundPic &select(Cpu &c, std::map<BindingKey, std::uint64_t> &verified) {
    const auto pc = phys(c.pc);
    for (const auto &[id, loaded] : g_modules) {
        if (!loaded.size || pc < loaded.base || pc - loaded.base >= loaded.size) continue;
        const BindingKey key{id, loaded.name, loaded.base, loaded.size};
        auto found = g_r192_pic_bindings.find(key);
        if (found != g_r192_pic_bindings.end()) {
            const auto checked = verified.find(key);
            if ((checked != verified.end() && checked->second == found->second.first) ||
                aot_v8::matches(found->second.second->module, {nullptr, identity, nullptr})) {
                verified[key] = found->second.first; return *found->second.second;
            }
            g_r192_pic_bindings.erase(found);
        }
        for (const auto &entry : aot_v8::pic_registry()) {
            if (loaded.name != entry.name) continue;
            const auto &source = entry.get();
            if (source.total_size != loaded.size || pc - loaded.base >= source.text_size) continue;
            auto binding = std::make_unique<aot_v8::BoundPic>();
            if (!binding->bind(source, loaded.base) || !aot_v8::matches(binding->module, {nullptr, identity, nullptr})) continue;
            auto *selected = binding.get();
            const auto generation = ++g_r192_binding_generation;
            g_r192_pic_bindings.emplace(key, std::make_pair(generation, std::move(binding)));
            verified[key] = generation; return *selected;
        }
    }
    aot_refuse("no_original_relocated_module_identity", c);
}
void scalar_step(const aot_v8::Module &m, Cpu &c) {
    ps2x::iop::detail::IopCpuState state{};
    std::copy(c.r, c.r + 32, state.gpr.begin());
    state.hi = c.hi; state.lo = c.lo; state.pc = c.pc;
    state.cop0[12] = c.status; state.cop0[13] = c.cause; state.cop0[14] = c.epc; state.cop0[15] = 0x1Fu;
    state.branchPending = c.npc != c.pc + 4;
    state.branchTarget = c.npc;
    ps2x::iop::detail::IopMemory memory;
    const auto ran = m.pic_body(state, memory, 1, m.base);
    if (ran != 1 || state.pendingLoad || state.exception)
        aot_refuse("compiled_v8_instruction_or_service_boundary", c);
    std::copy(state.gpr.begin(), state.gpr.end(), c.r); c.r[0] = 0;
    c.hi = state.hi; c.lo = state.lo; c.pc = state.pc;
    c.npc = state.branchPending ? state.branchTarget : state.pc + 4;
    c.status = state.cop0[12]; c.cause = state.cop0[13]; c.epc = state.cop0[14];
}
}

// Same v8 Cpu and scheduler budget ABI, with instruction dispatch removed.
void execute_aot_v8(Cpu &c, std::int64_t &budget) {
    std::map<BindingKey, std::uint64_t> verified;
    while (budget-- > 0) {
        if (c.r[0] || (c.pc & 3u)) aot_refuse("invalid_machine_entry", c);
        const auto pc = phys(c.pc);
        std::uint32_t id{};
        if (registered_kernel_stub(pc, id)) {
            native_hle(c, id); verified.clear();
            if (switch_pending()) return;
            continue;
        }
        auto &binding = select(c, verified);
        const auto &m = binding.module;
        const auto offset = pc - m.base;
        if (offset >= m.text_size) aot_refuse("module_data_not_compiled_entry", c);
        if (const auto *entry = at_import(m, offset)) {
            if (!identity(nullptr, pc, *entry)) aot_refuse("loader_import_identity_changed", c);
            const auto word = ram32(pc);
            if ((word >> 26) == kHleOpcode) {
                native_hle(c, word & 0x03FFFFFFu); verified.clear();
            } else {
                // Exact loader-produced J/NOP, with real two-step budget and
                // delay semantics. This is not a one-instruction HLE service.
                const auto target = (c.pc & 0xF0000000u) | ((word & 0x03FFFFFFu) << 2);
                c.pc = c.npc; c.npc = target; c.r[0] = 0;
            }
        } else if (offset >= 4 && at_import(m, offset - 4) &&
                   identity(nullptr, pc - 4, *at_import(m, offset - 4)) && ram32(pc) == 0) {
            c.pc = c.npc; c.npc += 4; c.r[0] = 0; // Actual linked import NOP.
        } else {
            // Recheck the EXACT instruction on every step. Native services,
            // DMA, state restores or stores cannot run stale translated words.
            if (ram32(pc) != m.original_words[offset / 4]) {
                verified.clear(); g_r192_pic_bindings.clear();
                aot_refuse("original_instruction_changed_requires_precompiled_variant", c);
            }
            scalar_step(m, c); ++g_r192_aot_steps;
        }
        if (switch_pending()) return;
    }
}
} // namespace ps2::iop::kernel
