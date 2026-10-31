#pragma once

#include "ps2/cpu.hpp"
#include <cstdint>
#include <cstddef>

namespace ps2::hook {

// Context delivered to an observer hook.
// Receives const Cpu reference: strictly read-only inspection of guest state.
// Cannot mutate guest registers, guest memory, or program counter.
struct GuestCallContext {
    std::uint32_t pc;
    std::uint32_t ra;
    std::uint32_t sp;
    const Cpu &cpu;
    std::uint32_t depth;
    const char *name;
    std::uint64_t call_count;
    bool is_sampled;
};

using ObserverCallback = void (*)(const GuestCallContext &ctx);

// Register an observer hook for a specific guest PC.
// Registration does not alter guest code or execution path.
// When the guest PC is called, callback is invoked, then original execution resumes unconditionally.
// max_initial_logs: number of initial calls to log explicitly before throttling.
// sample_interval: after initial logs, log once every sample_interval calls (0 = disable sampling).
void register_observer(std::uint32_t pc, ObserverCallback callback, const char *name = nullptr,
                       std::uint32_t max_initial_logs = 5, std::uint32_t sample_interval = 1000);

// Unregister an observer hook
void unregister_observer(std::uint32_t pc);

// Enable or disable observer hooks globally (also controlled via PS2_OBSERVER_HOOKS=1 env var)
void set_observer_hooks_enabled(bool enabled);
bool is_enabled();
bool observer_hooks_enabled();

// Fast dispatcher notification: uses cpu.pc
void notify_observer(const Cpu &cpu, std::uint32_t depth);

// Explicit PC notification (used for syscalls or direct-call observation points)
void notify_observer_pc(std::uint32_t pc, const Cpu &cpu, std::uint32_t depth);

// Query execution statistics
std::uint64_t total_observations();
std::uint64_t observations_for(std::uint32_t pc);

// Reset all observers and statistics (used in tests)
void reset_observers();

} // namespace ps2::hook

namespace ps2::profile {

// Subsystem-level high-precision profiler (Phase C)
// Zero-overhead when disabled; enabled via PS2_PROFILE_SUBSYSTEMS=1
bool profiler_enabled();
void set_profiler_enabled(bool enabled);

void record_vu_interpret(std::uint64_t ns);
void record_vu_compiled(std::uint64_t ns);
void record_gs_draw(std::uint64_t ns);
void record_d3d12_present(std::uint64_t ns);
void record_ee_live(std::uint64_t ns);
void record_frame();

void dump_summary_if_due(std::uint64_t current_frame);

} // namespace ps2::profile
