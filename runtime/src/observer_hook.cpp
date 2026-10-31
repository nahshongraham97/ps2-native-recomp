#include "ps2/observer_hook.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>
#include <atomic>

namespace ps2::hook {

namespace {

struct ObserverEntry {
    std::uint32_t pc;
    ObserverCallback callback;
    const char *name;
    std::uint32_t max_logs;
    std::uint64_t count;
};

std::vector<ObserverEntry> g_observers;
std::atomic<std::uint64_t> g_total_observations{0};
bool g_initialized = false;
bool g_enabled = false;

void init_from_env() {
    if (g_initialized) return;
    g_initialized = true;
    const char *env = std::getenv("PS2_OBSERVER_HOOKS");
    if (env && (std::strcmp(env, "1") == 0 || std::strcmp(env, "true") == 0 || std::strcmp(env, "TRUE") == 0)) {
        g_enabled = true;
        std::fprintf(stderr, "[observer-hook] Native observer hook framework ENABLED via PS2_OBSERVER_HOOKS\n");
    }
}

} // namespace

void register_observer(std::uint32_t pc, ObserverCallback callback, const char *name, std::uint32_t max_logs) {
    init_from_env();
    auto it = std::find_if(g_observers.begin(), g_observers.end(), [pc](const ObserverEntry &e) {
        return e.pc == pc;
    });
    if (it != g_observers.end()) {
        it->callback = callback;
        it->name = name ? name : "unnamed";
        it->max_logs = max_logs;
        return;
    }
    g_observers.push_back({pc, callback, name ? name : "unnamed", max_logs, 0});
}

void unregister_observer(std::uint32_t pc) {
    g_observers.erase(
        std::remove_if(g_observers.begin(), g_observers.end(), [pc](const ObserverEntry &e) {
            return e.pc == pc;
        }),
        g_observers.end()
    );
}

void set_observer_hooks_enabled(bool enabled) {
    g_initialized = true;
    g_enabled = enabled;
}

bool observer_hooks_enabled() {
    init_from_env();
    return g_enabled && !g_observers.empty();
}

void notify_observer(const Cpu &cpu, std::uint32_t depth) {
    if (!g_enabled || g_observers.empty()) return;

    const std::uint32_t pc = cpu.pc;
    for (auto &entry : g_observers) {
        if (entry.pc == pc) {
            ++entry.count;
            ++g_total_observations;

            if (entry.max_logs > 0 && entry.count <= entry.max_logs) {
                std::fprintf(stderr,
                    "[observer-hook] PC=0x%08X (%s) RA=0x%08X SP=0x%08X Depth=%u CallCount=%llu\n",
                    pc, entry.name, cpu.r[31].w[0], cpu.r[29].w[0], depth,
                    static_cast<unsigned long long>(entry.count)
                );
            }

            if (entry.callback) {
                GuestCallContext ctx{
                    pc,
                    cpu.r[31].w[0],
                    cpu.r[29].w[0],
                    cpu,
                    depth,
                    entry.name
                };
                entry.callback(ctx);
            }
            return;
        }
    }
}

std::uint64_t total_observations() {
    return g_total_observations.load();
}

std::uint64_t observations_for(std::uint32_t pc) {
    for (const auto &entry : g_observers) {
        if (entry.pc == pc) {
            return entry.count;
        }
    }
    return 0;
}

void reset_observers() {
    g_observers.clear();
    g_total_observations.store(0);
}

} // namespace ps2::hook
