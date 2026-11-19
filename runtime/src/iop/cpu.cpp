#include "iop.hpp"
#include "kernel.hpp"
#include "compliance_census.hpp"
#include "ps2/memory.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace ps2::iop {

alignas(64) std::uint8_t g_ram[kRamSize + 16];
Cpu g_cpu;

namespace {

std::vector<std::uint8_t> g_probes;

bool load_probes() {
    const char *spec = std::getenv("PS2_IOP_PROBE");
    if (!spec) return false;
    g_probes.assign(kRamSize / 32, 0);
    for (const char *p = spec; *p;) {
        char *end = nullptr;
        const std::uint32_t a = static_cast<std::uint32_t>(std::strtoul(p, &end, 16)) & (kRamSize - 1);
        g_probes[a >> 5] |= static_cast<std::uint8_t>(1u << ((a >> 2) & 7u));
        p = *end ? end + 1 : end;
    }
    return true;
}

void probe(const Cpu &c, std::uint32_t pc) {
    const kernel::Thread *t = kernel::current();
    std::fprintf(stderr, "[iop-probe] 0x%08X a0 %08X a1 %08X a2 %08X a3 %08X v0 %08X v1 %08X s0 %08X s1 %08X ra %08X thread %d time %llu\n", pc, c.r[4], c.r[5], c.r[6],
                 c.r[7], c.r[2], c.r[3], c.r[16], c.r[17], c.r[31], t ? t->id : 0, static_cast<unsigned long long>(kernel::now()));
    static const char *reg = std::getenv("PS2_IOP_PROBE_MEM");
    if (reg) {
        const std::uint32_t base = phys(c.r[std::atoi(reg) & 31]) & (kRamSize - 1);
        if (base + 64 <= kRamSize) {
            std::fprintf(stderr, "[iop-probe]   r%s:", reg);
            for (std::uint32_t i = 0; i < 16; ++i) std::fprintf(stderr, " %08X", ram32(base + i * 4));
            std::fprintf(stderr, "\n");
        }
    }
    // Optional fixed-address snapshot for a stopped IOP context. Interpret
    // mirrors in the same way as ram_ptr(); never wrap the 64-byte window.
    if (const char *spec = std::getenv("PS2_IOP_PROBE_ADDRESS")) {
        char *end = nullptr;
        const unsigned long raw = std::strtoul(spec, &end, 16);
        if (end != spec && *end == '\0' && raw <= 0xFFFFFFFFul) {
            const std::uint32_t base = phys(static_cast<std::uint32_t>(raw)) & (kRamSize - 1);
            if (base <= kRamSize - 64) {
                std::fprintf(stderr, "[iop-probe]   address %08lX:", raw);
                for (std::uint32_t i = 0; i < 16; ++i) std::fprintf(stderr, " %08X", ram32(base + i * 4));
                std::fprintf(stderr, "\n");
            }
        }
    }
    if (const char *spec = std::getenv("PS2_EE_PROBE_ADDRESS")) {
        char *end = nullptr;
        const unsigned long raw = std::strtoul(spec, &end, 16);
        if (end != spec && *end == '\0' && raw <= ps2::kRamSize - 128u) {
            const auto *memory = ps2::fast<std::uint8_t>(static_cast<std::uint32_t>(raw));
            if (memory) {
                std::fprintf(stderr, "[iop-probe]   EE address %08lX:", raw);
                for (std::uint32_t i = 0; i < 32; ++i)
                    std::fprintf(stderr, " %08X", ps2::load<std::uint32_t>(memory + i * 4));
                std::fprintf(stderr, "\n");
            }
        }
    }
}

}

void execute(Cpu &c, std::int64_t &budget) {
    static const bool probing = load_probes();
    // No instruction interpreter exists behind these dispatchers. Unmatched
    // code is a missing native destination and always stops, independent of
    // diagnostic flags or the former opt-in strict mode.
    auto stop_uncompiled_execution = [&] {
        ps2::census::report();
        const kernel::Thread *thread = kernel::current();
        std::fprintf(stderr, "[iop-native-stop] unresolved/uncompiled IOP PC=0x%08X thread=%d library=<unresolved:no matched import>\n",
                     c.pc, thread ? thread->id : 0);
        const std::uint32_t address = phys(c.pc);
        bool owner_found = false;
        for (const auto &[id, module] : kernel::modules()) {
            const std::uint32_t base = phys(module.base);
            if (module.size && address >= base && address - base < module.size) {
                owner_found = true;
                std::fprintf(stderr, "[iop-native-stop] owner module=%s id=%d base=0x%08X size=0x%X offset=0x%X entry=0x%08X\n",
                             module.name.c_str(), id, module.base, module.size, address - base, module.entry);
            }
        }
        if (!owner_found)
            std::fprintf(stderr, "[iop-native-stop] owner module=<unmapped> pc=0x%08X\n", c.pc);
        if (thread)
            std::fprintf(stderr, "[iop-native-stop] thread entry=0x%08X ra=0x%08X gp=0x%08X sp=0x%08X s0=0x%08X a0=0x%08X v0=0x%08X\n",
                         thread->entry, c.r[31], c.r[28], c.r[29], c.r[16], c.r[4], c.r[2]);
        std::fprintf(stderr, "[iop-native-stop] exception cause=0x%08X epc=0x%08X badvaddr=0x%08X status=0x%08X s1=0x%08X\n",
                     c.cause, c.epc, c.badvaddr, c.status, c.r[17]);
        if (std::getenv("PS2_IOP_DUMP_ON_STOP")) dump();
        std::fflush(stderr);
        ps2::census::report();  // before abort(): report() is idempotent, the handler call is a no-op
        std::abort();
    };
    static bool traced_mwfilefr_callback = false;
    static bool traced_mwfilefr_context = false;
    static bool traced_mwfilefr_request_context = false;
    while (budget > 0) {
        const std::uint32_t pc = c.pc;
        if (!traced_mwfilefr_request_context && pc == 0x00023B1Cu && kernel::g_trace_modules) {
            traced_mwfilefr_request_context = true;
            std::fprintf(stderr,
                         "[iop:mwfilefr-request-context] entry 0x%08X ra 0x%08X a0 %08X a1 %08X a2 %08X a3 %08X sp %08X stack+10 %08X +14 %08X +18 %08X +1C %08X\n",
                         pc, c.r[31], c.r[4], c.r[5], c.r[6], c.r[7], c.r[29],
                         read32(c.r[29] + 0x10u), read32(c.r[29] + 0x14u), read32(c.r[29] + 0x18u),
                         read32(c.r[29] + 0x1Cu));
        }
        if (!traced_mwfilefr_context && pc >= 0x00023B80u && pc < 0x00023BC0u && kernel::g_trace_modules) {
            traced_mwfilefr_context = true;
            std::fprintf(stderr,
                         "[iop:mwfilefr-context] pc 0x%08X ra 0x%08X a0 %08X a1 %08X a2 %08X a3 %08X sp %08X stack+10 %08X +14 %08X +18 %08X +1C %08X +20 %08X +24 %08X\n",
                         pc, c.r[31], c.r[4], c.r[5], c.r[6], c.r[7], c.r[29],
                         read32(c.r[29] + 0x10u), read32(c.r[29] + 0x14u), read32(c.r[29] + 0x18u),
                         read32(c.r[29] + 0x1Cu), read32(c.r[29] + 0x20u), read32(c.r[29] + 0x24u));
        }
        if (!traced_mwfilefr_callback && pc == 0x00021978u && kernel::g_trace_modules) {
            traced_mwfilefr_callback = true;
            std::fprintf(stderr,
                         "[iop:rpc-callback] entry 0x%08X ra 0x%08X a0 0x%08X a1 0x%08X a2 0x%08X a3 0x%08X sp 0x%08X\n",
                         pc, c.r[31], c.r[4], c.r[5], c.r[6], c.r[7], c.r[29]);
        }
        if (probing) {
            const std::uint32_t a = pc & (kRamSize - 1);
            if (g_probes[a >> 5] & (1u << ((a >> 2) & 7u))) probe(c, pc);
        }
        if (kernel::invoke_compiled_module(c, budget)) {
            if (kernel::switch_pending()) return;
            continue;
        }
        if (kernel::invoke_compiled_import(c)) {
            --budget; // one dispatcher slot replaces the IRX jr-ra import stub
            if (kernel::switch_pending()) return;
            continue;
        }
        if (kernel::invoke_runtime_hle_stub(c)) {
            --budget; // one host callback replaces the runtime-generated trampoline opcode
            if (kernel::switch_pending()) return;
            continue;
        }
        stop_uncompiled_execution();
    }
}

}
