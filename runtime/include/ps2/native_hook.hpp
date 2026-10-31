#pragma once

#include "ps2/cpu.hpp"
#include <cstdint>
#include <functional>

namespace ps2::hook {

// Function signature for a native C++ replacement hook (Phase 2 Master Plan)
// Takes Cpu &c, allowing native implementation of guest functions, updating return values,
// and returning cleanly without executing recompiled MIPS.
using HookHandler = void (*)(Cpu &c);

// Register a native replacement hook for guest_pc
void register_hook(std::uint32_t guest_pc, HookHandler handler, const char *name = nullptr);

// Unregister a hook
void unregister_hook(std::uint32_t guest_pc);

// Find active replacement hook for guest_pc (nullptr if unhooked)
HookHandler find_hook(std::uint32_t guest_pc);

// Check if any native replacement hooks are active
bool has_active_hooks();

// Total count of executed native replacement hooks
std::uint64_t native_hook_executions();

} // namespace ps2::hook
