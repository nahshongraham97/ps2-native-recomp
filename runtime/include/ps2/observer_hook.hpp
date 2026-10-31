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
};

using ObserverCallback = void (*)(const GuestCallContext &ctx);

// Register an observer hook for a specific guest PC.
// Registration does not alter guest code or execution path.
// When the guest PC is called, callback is invoked, then original execution resumes unconditionally.
void register_observer(std::uint32_t pc, ObserverCallback callback, const char *name = nullptr, std::uint32_t max_logs = 10);

// Unregister an observer hook
void unregister_observer(std::uint32_t pc);

// Enable or disable observer hooks globally (also controlled via PS2_OBSERVER_HOOKS=1 env var)
void set_observer_hooks_enabled(bool enabled);
bool observer_hooks_enabled();

// Fast dispatcher notification: returns immediately if observer hooks are disabled
void notify_observer(const Cpu &cpu, std::uint32_t depth);

// Query execution statistics
std::uint64_t total_observations();
std::uint64_t observations_for(std::uint32_t pc);

// Reset all observers and statistics (used in tests)
void reset_observers();

} // namespace ps2::hook
