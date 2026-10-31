#include "ps2/native_hook.hpp"
#include <atomic>
#include <cstdio>
#include <unordered_map>
#include <mutex>

namespace ps2::hook {

namespace {

struct HookEntry {
    HookHandler handler = nullptr;
    const char *name = nullptr;
};

std::unordered_map<std::uint32_t, HookEntry> g_hooks;
std::mutex g_hook_mutex;
std::atomic<bool> g_has_hooks{false};
std::atomic<std::uint64_t> g_execution_count{0};

} // namespace

void register_hook(std::uint32_t guest_pc, HookHandler handler, const char *name) {
    std::lock_guard lock(g_hook_mutex);
    g_hooks[guest_pc] = {handler, name};
    g_has_hooks.store(!g_hooks.empty(), std::memory_order_release);
    std::fprintf(stderr, "[native-hook] Registered replacement hook at 0x%08X (%s)\n",
                 guest_pc, name ? name : "unnamed");
}

void unregister_hook(std::uint32_t guest_pc) {
    std::lock_guard lock(g_hook_mutex);
    g_hooks.erase(guest_pc);
    g_has_hooks.store(!g_hooks.empty(), std::memory_order_release);
}

HookHandler find_hook(std::uint32_t guest_pc) {
    if (!g_has_hooks.load(std::memory_order_relaxed)) return nullptr;
    std::lock_guard lock(g_hook_mutex);
    auto it = g_hooks.find(guest_pc);
    if (it != g_hooks.end()) {
        g_execution_count.fetch_add(1, std::memory_order_relaxed);
        return it->second.handler;
    }
    return nullptr;
}

bool has_active_hooks() {
    return g_has_hooks.load(std::memory_order_relaxed);
}

std::uint64_t native_hook_executions() {
    return g_execution_count.load(std::memory_order_relaxed);
}

} // namespace ps2::hook
