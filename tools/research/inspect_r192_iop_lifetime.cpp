// Read-only, test-only introspection of the pinned original R192 runtime.
// Compile each component as a separate TU; never cast private runtime layouts.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

static std::string text_hex(const std::string &text) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string result;
    for (unsigned char byte : text) { result += digits[byte >> 4]; result += digits[byte & 15]; }
    return result.empty() ? "-" : result;
}

#if R192_LIFETIME_COMPONENT == 1
#include R192_ORIGINAL_KERNEL
void r192_dump_kernel(FILE *out) {
    using namespace ps2::iop;
    using namespace ps2::iop::kernel;
    std::fprintf(out, "CONTEXT %d %d %d %u %u %u %llu\n", g_current, g_nesting, g_suspended, g_cpu.pc, g_cpu.npc, g_cpu.r[31],
                 static_cast<unsigned long long>(g_now));
    for (const auto &[id, m] : g_modules)
        std::fprintf(out, "MODULE %d %u %u %u %u %s\n", id, m.base, m.size, m.entry, m.gp, text_hex(m.name).c_str());
    for (const auto &[base, size] : g_used) std::fprintf(out, "ALLOCATION %u %u\n", base, size);
    for (const auto &[name, address] : g_libraries) {
        // No masked/wrapping ram_ptr read for a malformed registry pointer.
        const auto at = phys(address);
        if (at > kRamSize - 20 || (at & 3)) throw std::runtime_error("provider_header_out_of_bounds");
        std::fprintf(out, "PROVIDER %s %u %u %u %u %u %u\n", text_hex(name).c_str(), address,
                     ram32(at), ram32(at + 4), ram32(at + 8), ram32(at + 12), ram32(at + 16));
    }
    for (const auto &[id, t] : g_threads) {
        std::fprintf(out, "THREAD %d %d %u %u %u %u %u %u %u %d\n", id, t.status, t.entry, t.context.pc,
                     t.context.npc, t.context.r[31], t.stack, t.stack_size, t.rpc_server, static_cast<bool>(t.on_exit));
    }
    for (const auto &[line, i] : g_interrupts)
        std::fprintf(out, "INTERRUPT %u %u %u %u\n", line, i.handler, i.arg, i.mode);
    auto events = g_events;
    while (!events.empty()) {
        const auto &e = events.top();
        std::fprintf(out, "JOB %llu %llu %u %u %u %u %u %d\n", static_cast<unsigned long long>(e.at),
                     static_cast<unsigned long long>(e.seq), e.job.kind, e.job.a, e.job.b, e.job.c, e.job.d,
                     static_cast<bool>(e.action));
        events.pop();
    }
}
#elif R192_LIFETIME_COMPONENT == 2
#include R192_ORIGINAL_LIBS
void r192_dump_libraries(FILE *out) {
    using namespace ps2::iop::kernel;
    for (const auto &h : g_vblank_handlers) std::fprintf(out, "VBLANK %d %u %u\n", h.edge, h.handler, h.arg);
    for (const auto &[id, h] : g_alarms)
        std::fprintf(out, "ALARM %llu %u %u %d\n", static_cast<unsigned long long>(id), h.handler, h.arg, h.active);
    for (const auto &[id, h] : g_hard_timers)
        std::fprintf(out, "TIMER %u %u %u %d %llu\n", id, h.handler, h.common, h.running,
                     static_cast<unsigned long long>(h.generation));
}
#elif R192_LIFETIME_COMPONENT == 3
#include R192_ORIGINAL_SIF
void r192_dump_sif(FILE *out) {
    using namespace ps2::sif;
    for (const auto &[id, s] : g_servers)
        std::fprintf(out, "SERVER %u %d %u %u %u %u %u\n", id, s.hle, s.sd, s.sid, s.func, s.cfunc, s.queue);
    for (const auto &[sd, s] : g_guest_servers)
        std::fprintf(out, "GUEST_SERVER %u %d %u %u %u %u %u\n", sd, s.hle, s.sd, s.sid, s.func, s.cfunc, s.queue);
    for (const auto &[id, h] : g_cmd_handlers) std::fprintf(out, "COMMAND %u %u %u\n", id, h.handler, h.data);
    for (const auto &[queue, tid] : g_queue_threads) std::fprintf(out, "RPC_THREAD %u %d\n", queue, tid);
}
#else
#include "ps2/system.hpp"
#include "iop/iop.hpp"
#include "iop/kernel.hpp"
#include <cstddef>
#include <stdexcept>
void r192_dump_kernel(FILE *);
void r192_dump_libraries(FILE *);
void r192_dump_sif(FILE *);
static_assert(sizeof(ps2::iop::Cpu) == 156);
static_assert(offsetof(ps2::iop::Cpu, pc) == 136);
static_assert(offsetof(ps2::iop::Cpu, status) == 144);
int main(int argc, char **argv) {
    try {
        if (argc != 2) throw std::runtime_error("one_owned_state_required");
        std::ifstream input(argv[1], std::ios::binary);
        if (!input) throw std::runtime_error("owned_state_unreadable");
        const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input), {}};
        if (bytes.size() < 12 || std::memcmp(bytes.data(), "PS2STATE", 8)) throw std::runtime_error("state_magic_or_header");
        std::uint32_t version{}; std::memcpy(&version, bytes.data() + 8, 4);
        if (version != 8) throw std::runtime_error("only_pinned_v8_admitted");
        if (!ps2::g_options.memcard_path.empty()) throw std::runtime_error("unexpected_card_path");
        // Register existing HLE RPC factories needed by the actual serializer.
        // No reset, boot, mount, execute, run, callback or path-load invocation.
        ps2::iop::kernel::install_sif();
        if (!ps2::load_state(bytes.data(), bytes.size())) throw std::runtime_error("actual_serializer_refused");
        if (!ps2::g_options.memcard_path.empty()) throw std::runtime_error("card_path_changed");
        std::printf("STATE %llu 8 156\n", static_cast<unsigned long long>(ps2::frame_count()));
        r192_dump_kernel(stdout); r192_dump_libraries(stdout); r192_dump_sif(stdout);
        if (std::fflush(stdout)) throw std::runtime_error("output_write_failed");
        return 0;
    } catch (const std::exception &e) { std::fprintf(stderr, "REFUSED:%s\n", e.what()); return 1; }
}
#endif
