#include "r192_aot.hpp"
#include "iop/kernel.hpp"

#include <algorithm>

namespace ps2::iop::r192_aot {
namespace {
const Import *find_import(const Module &module, std::uint32_t offset) {
    for (std::uint32_t i = 0; i < module.import_count; ++i)
        if (module.imports[i].offset == offset) return &module.imports[i];
    return nullptr;
}

bool valid(const Module &m) {
    if (!m.name || (!m.body && !m.pic_body) || (m.body && m.pic_body) || !m.original_words || !m.text_size || (m.text_size & 3u) ||
        (m.base & 3u) || m.base >= kRamSize || m.total_size > kRamSize - m.base ||
        m.text_size > m.total_size || (m.import_count && (!m.imports || m.text_size < 8u))) return false;
    for (std::uint32_t i = 0; i < m.import_count; ++i) {
        const auto &entry = m.imports[i];
        if (!entry.library || entry.ordinal > 0xFFFFu || (entry.offset & 3u) || entry.offset > m.text_size - 8u ||
            m.original_words[entry.offset / 4] != 0x03E00008u ||
            m.original_words[entry.offset / 4 + 1] != (0x24000000u | entry.ordinal)) return false;
        for (std::uint32_t j = 0; j < i; ++j)
            if (m.imports[j].offset == entry.offset) return false;
    }
    return true;
}

void capture(const Cpu &c, const Pipeline &pipeline, ps2x::iop::detail::IopCpuState &state) {
    std::copy(c.r, c.r + 32, state.gpr.begin());
    state.gpr[0] = 0;
    state.hi = c.hi; state.lo = c.lo; state.pc = c.pc;
    state.cop0[8] = pipeline.badvaddr; state.cop0[12] = c.status;
    state.cop0[13] = c.cause; state.cop0[14] = c.epc;
    state.cop0[15] = 0x1Fu; // Frozen R192 IOP PRId, independently in cpu.cpp MFC0.
    state.pendingLoad = pipeline.pending_load;
    state.pendingLoadReg = pipeline.pending_load_reg;
    state.pendingLoadValue = pipeline.pending_load_value;
    state.branchPending = c.npc != c.pc + 4u;
    state.branchTarget = c.npc;
}

void restore(Cpu &c, Pipeline &pipeline, const ps2x::iop::detail::IopCpuState &state) {
    std::copy(state.gpr.begin(), state.gpr.end(), c.r);
    c.r[0] = 0;
    c.hi = state.hi; c.lo = state.lo; c.pc = state.pc;
    c.npc = state.branchPending ? state.branchTarget : state.pc + 4u;
    pipeline.badvaddr = state.cop0[8];
    if (c.status != state.cop0[12]) kernel::set_status(c, state.cop0[12]);
    c.cause = state.cop0[13]; c.epc = state.cop0[14];
    pipeline.pending_load = state.pendingLoad;
    pipeline.pending_load_reg = state.pendingLoadReg;
    pipeline.pending_load_value = state.pendingLoadValue;
}
} // namespace

bool matches(const Module &module, const Services &services) {
    if (!valid(module)) return false;
    for (std::uint32_t offset = 0; offset < module.text_size; offset += 4) {
        if (ram32(module.base + offset) == module.original_words[offset / 4]) continue;
        const auto *entry = find_import(module, offset);
        if (!entry && offset >= 4) entry = find_import(module, offset - 4);
        if (!entry || !services.validate_import ||
            !services.validate_import(services.user, module.base + entry->offset, *entry)) return false;
    }
    return true;
}

Result dispatch(const Module &module, Cpu &cpu, Pipeline &pipeline, std::uint32_t budget, const Services &services) {
    if (!valid(module)) return {Status::invalid_module, 0, nullptr, "invalid_module_descriptor"};
    if (!matches(module, services)) return {Status::code_identity_mismatch, 0, nullptr, "original_text_or_import_identity_mismatch"};
    if (cpu.r[0] != 0) return {Status::invalid_entry, 0, nullptr, "invalid_R3000_zero_register"};
    if (pipeline.pending_load && (pipeline.pending_load_reg == 0 || pipeline.pending_load_reg >= 32))
        return {Status::invalid_entry, 0, nullptr, "invalid_pending_load_register"};
    std::uint32_t ran = 0;
    while (ran < budget) {
        const auto pc = phys(cpu.pc);
        if ((pc & 3u) || pc < module.base || pc - module.base >= module.text_size)
            return {Status::external_destination, ran, nullptr, "next_IOP_destination_requires_its_own_AOT_binding"};
        const auto *entry = find_import(module, pc - module.base);
        if (entry) {
            if (!services.invoke_import)
                return {Status::import_unbound, ran, entry, "native_import_service_not_bound"};
            const Cpu before = cpu;
            const Pipeline before_pipeline = pipeline;
            const auto target = cpu.r[31]; // JR reads its target before a pending load commits.
            if (pipeline.pending_load) cpu.r[pipeline.pending_load_reg] = pipeline.pending_load_value;
            pipeline.pending_load = false;
            // The preserved v8 host-call ABI observes the return PC before
            // entering its service. A service may deliberately change it.
            cpu.pc = target; cpu.npc = target + 4u; cpu.r[0] = 0;
            if (!services.invoke_import(services.user, *entry, cpu)) {
                cpu = before;
                pipeline = before_pipeline;
                return {Status::import_unbound, ran, entry, "native_import_service_refused"};
            }
            cpu.r[0] = 0;
            ++ran;
            continue;
        }
        ps2x::iop::detail::IopCpuState state{};
        capture(cpu, pipeline, state);
        ps2x::iop::detail::IopMemory memory;
        const auto used = module.pic_body ? module.pic_body(state, memory, budget - ran, module.base)
                                          : module.body(state, memory, budget - ran);
        if (!used || used > budget - ran)
            return {Status::invalid_body_result, ran, nullptr, "compiled_body_made_no_or_invalid_progress"};
        restore(cpu, pipeline, state);
        ran += used;
        if (state.exception)
            return {Status::architectural_exception, ran, nullptr, "R3000_exception_requires_compiled_exception_destination"};
    }
    return {Status::budget_exhausted, ran, nullptr, "requested_instruction_budget_consumed"};
}

const char *status_name(Status status) {
    switch (status) {
    case Status::budget_exhausted: return "budget_exhausted";
    case Status::import_unbound: return "import_unbound";
    case Status::external_destination: return "external_destination";
    case Status::architectural_exception: return "architectural_exception";
    case Status::code_identity_mismatch: return "code_identity_mismatch";
    case Status::invalid_module: return "invalid_module";
    case Status::invalid_entry: return "invalid_entry";
    case Status::invalid_body_result: return "invalid_body_result";
    }
    return "unknown_status";
}
} // namespace ps2::iop::r192_aot
