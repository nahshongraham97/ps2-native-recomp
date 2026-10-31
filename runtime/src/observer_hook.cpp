#include "ps2/observer_hook.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>
#include <atomic>
#include <chrono>

namespace ps2::hook {

namespace {

struct ObserverEntry {
    std::uint32_t pc;
    ObserverCallback callback;
    const char *name;
    std::uint32_t max_initial_logs;
    std::uint32_t sample_interval;
    std::uint64_t count;
};

struct SyscallTrapEntry {
    std::int32_t syscall_num;
    const char *name;
    std::uint64_t count;
};

std::vector<ObserverEntry> g_observers;
std::vector<SyscallTrapEntry> g_syscall_traps;
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

void register_observer(std::uint32_t pc, ObserverCallback callback, const char *name,
                       std::uint32_t max_initial_logs, std::uint32_t sample_interval) {
    init_from_env();
    auto it = std::find_if(g_observers.begin(), g_observers.end(), [pc](const ObserverEntry &e) {
        return e.pc == pc;
    });
    if (it != g_observers.end()) {
        it->callback = callback;
        it->name = name ? name : "unnamed";
        it->max_initial_logs = max_initial_logs;
        it->sample_interval = sample_interval;
        return;
    }
    g_observers.push_back({pc, callback, name ? name : "unnamed", max_initial_logs, sample_interval, 0});
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

bool is_enabled() {
    init_from_env();
    return g_enabled;
}

bool observer_hooks_enabled() {
    init_from_env();
    return g_enabled && !g_observers.empty();
}

void notify_observer_pc(std::uint32_t pc, const Cpu &cpu, std::uint32_t depth) {
    if (!g_enabled || g_observers.empty()) return;

    for (auto &entry : g_observers) {
        if (entry.pc == pc) {
            ++entry.count;
            ++g_total_observations;

            const bool is_initial = (entry.max_initial_logs > 0 && entry.count <= entry.max_initial_logs);
            const bool is_sampled = (entry.sample_interval > 0 && (entry.count % entry.sample_interval == 0));

            if (is_initial || is_sampled) {
                std::fprintf(stderr,
                    "[observer-hook] PC=0x%08X (%s) RA=0x%08X SP=0x%08X Depth=%u CallCount=%llu (A0=0x%08X, V0=0x%08X)\n",
                    pc, entry.name, cpu.r[31].w[0], cpu.r[29].w[0], depth,
                    static_cast<unsigned long long>(entry.count),
                    cpu.r[4].w[0], cpu.r[2].w[0]
                );

                if (entry.callback) {
                    GuestCallContext ctx{
                        pc,
                        cpu.r[31].w[0],
                        cpu.r[29].w[0],
                        cpu,
                        depth,
                        entry.name,
                        entry.count,
                        is_sampled
                    };
                    entry.callback(ctx);
                }
            }
            return;
        }
    }
}

void notify_observer(const Cpu &cpu, std::uint32_t depth) {
    notify_observer_pc(cpu.pc, cpu, depth);
}

void notify_syscall_trap(std::int32_t syscall_num, const char *name, const Cpu &cpu,
                         std::uint32_t max_initial_logs, std::uint32_t sample_interval) {
    if (!g_enabled) return;

    auto it = std::find_if(g_syscall_traps.begin(), g_syscall_traps.end(), [syscall_num](const SyscallTrapEntry &e) {
        return e.syscall_num == syscall_num;
    });

    if (it == g_syscall_traps.end()) {
        g_syscall_traps.push_back({syscall_num, name ? name : "unknown", 0});
        it = g_syscall_traps.end() - 1;
    }

    ++it->count;
    ++g_total_observations;

    const bool is_initial = (max_initial_logs > 0 && it->count <= max_initial_logs);
    const bool is_sampled = (sample_interval > 0 && (it->count % sample_interval == 0));

    if (is_initial || is_sampled) {
        std::fprintf(stderr,
            "[syscall-trap] Syscall %d (%s) invoked from RA=0x%08X (Caller PC=0x%08X, A0=0x%08X, SP=0x%08X, Count=%llu)\n",
            syscall_num, it->name,
            cpu.r[31].w[0], cpu.pc, cpu.r[4].w[0], cpu.r[29].w[0],
            static_cast<unsigned long long>(it->count)
        );
    }
}

std::uint64_t total_observations() {
    return g_total_observations.load();
}

std::uint64_t observations_for(std::uint32_t pc) {
    for (const auto &entry : g_observers) {
        if (entry.pc == pc) return entry.count;
    }
    return 0;
}

std::uint64_t syscall_traps_for(std::int32_t syscall_num) {
    for (const auto &entry : g_syscall_traps) {
        if (entry.syscall_num == syscall_num) return entry.count;
    }
    return 0;
}

void reset_observers() {
    g_observers.clear();
    g_syscall_traps.clear();
    g_total_observations.store(0);
}

} // namespace ps2::hook

namespace ps2::profile {

namespace {

bool g_profiler_init = false;
bool g_profiler_enabled = false;

std::atomic<std::uint64_t> g_ns_vu_interpret{0};
std::atomic<std::uint64_t> g_ns_vu_compiled{0};
std::atomic<std::uint64_t> g_ns_gs_draw{0};
std::atomic<std::uint64_t> g_ns_present{0};
std::atomic<std::uint64_t> g_ns_ee_live{0};

std::atomic<std::uint64_t> g_calls_vu_interpret{0};
std::atomic<std::uint64_t> g_calls_vu_compiled{0};
std::atomic<std::uint64_t> g_calls_gs_draw{0};
std::atomic<std::uint64_t> g_calls_present{0};
std::atomic<std::uint64_t> g_calls_ee_live{0};

std::uint64_t g_last_dump_frame = 0;
std::chrono::steady_clock::time_point g_window_start;

void init_profiler() {
    if (g_profiler_init) return;
    g_profiler_init = true;
    const char *env = std::getenv("PS2_PROFILE_SUBSYSTEMS");
    if (env && (std::strcmp(env, "1") == 0 || std::strcmp(env, "true") == 0 || std::strcmp(env, "TRUE") == 0)) {
        g_profiler_enabled = true;
        g_window_start = std::chrono::steady_clock::now();
        std::fprintf(stderr, "[profiler] Subsystem performance timing profiler ENABLED (Phase C)\n");
    }
}

} // namespace

bool profiler_enabled() {
    init_profiler();
    return g_profiler_enabled;
}

void set_profiler_enabled(bool enabled) {
    g_profiler_init = true;
    g_profiler_enabled = enabled;
    if (enabled) g_window_start = std::chrono::steady_clock::now();
}

void record_vu_interpret(std::uint64_t ns) {
    if (!g_profiler_enabled) return;
    g_ns_vu_interpret.fetch_add(ns, std::memory_order_relaxed);
    g_calls_vu_interpret.fetch_add(1, std::memory_order_relaxed);
}

void record_vu_compiled(std::uint64_t ns) {
    if (!g_profiler_enabled) return;
    g_ns_vu_compiled.fetch_add(ns, std::memory_order_relaxed);
    g_calls_vu_compiled.fetch_add(1, std::memory_order_relaxed);
}

void record_gs_draw(std::uint64_t ns) {
    if (!g_profiler_enabled) return;
    g_ns_gs_draw.fetch_add(ns, std::memory_order_relaxed);
    g_calls_gs_draw.fetch_add(1, std::memory_order_relaxed);
}

void record_d3d12_present(std::uint64_t ns) {
    if (!g_profiler_enabled) return;
    g_ns_present.fetch_add(ns, std::memory_order_relaxed);
    g_calls_present.fetch_add(1, std::memory_order_relaxed);
}

void record_ee_live(std::uint64_t ns) {
    if (!g_profiler_enabled) return;
    g_ns_ee_live.fetch_add(ns, std::memory_order_relaxed);
    g_calls_ee_live.fetch_add(1, std::memory_order_relaxed);
}

void reset_profiler_counters() {
    g_ns_vu_interpret.store(0);
    g_ns_vu_compiled.store(0);
    g_ns_gs_draw.store(0);
    g_ns_present.store(0);
    g_ns_ee_live.store(0);

    g_calls_vu_interpret.store(0);
    g_calls_vu_compiled.store(0);
    g_calls_gs_draw.store(0);
    g_calls_present.store(0);
    g_calls_ee_live.store(0);

    g_last_dump_frame = 0;
    g_window_start = std::chrono::steady_clock::now();
}

void dump_summary_if_due(std::uint64_t current_frame) {
    if (!g_profiler_enabled) return;
    if (current_frame == 0 || current_frame - g_last_dump_frame < 120) return;

    const auto now = std::chrono::steady_clock::now();
    const double elapsed_ms = std::chrono::duration<double, std::milli>(now - g_window_start).count();
    const std::uint64_t frames = current_frame - g_last_dump_frame;
    const double fps = (elapsed_ms > 0.0) ? (frames / (elapsed_ms / 1000.0)) : 0.0;

    const double vu_int_ms = g_ns_vu_interpret.exchange(0) / 1000000.0;
    const double vu_cmp_ms = g_ns_vu_compiled.exchange(0) / 1000000.0;
    const double gs_ms = g_ns_gs_draw.exchange(0) / 1000000.0;
    const double present_ms = g_ns_present.exchange(0) / 1000000.0;
    const double ee_live_ms = g_ns_ee_live.exchange(0) / 1000000.0;

    const std::uint64_t vu_int_calls = g_calls_vu_interpret.exchange(0);
    const std::uint64_t vu_cmp_calls = g_calls_vu_compiled.exchange(0);
    const std::uint64_t gs_calls = g_calls_gs_draw.exchange(0);
    const std::uint64_t present_calls = g_calls_present.exchange(0);
    const std::uint64_t ee_live_calls = g_calls_ee_live.exchange(0);

    char ee_buf[128];
    if (ee_live_calls > 0) {
        std::snprintf(ee_buf, sizeof(ee_buf), "%7.1f ms cumulative [%5.2f ms/frame] [Compiles: %llu, Avg: %.2f ms/call]",
                      ee_live_ms, ee_live_ms / frames, static_cast<unsigned long long>(ee_live_calls),
                      ee_live_ms / ee_live_calls);
    } else {
        std::snprintf(ee_buf, sizeof(ee_buf), "0.0 ms [0 calls (all EE code translated or precompiled)]");
    }

    std::fprintf(stderr,
        "\n================ [Subsystem Profile — Frame %llu (%u frames @ %.1f FPS)] ================\n"
        "  Wall-Clock Window : %7.1f ms [Avg Frame: %.2f ms]\n"
        "  ----------------------------------------------------------------------------------------\n"
        "  Subsystem Cumulative Execution (Thread concurrency active: vector, render, & host overlap):\n"
        "    VU1-Interpreted : %7.1f ms cumulative [%5.2f ms/frame] [Calls: %llu, Avg: %.2f us/call]\n"
        "    VU1-Compiled    : %7.1f ms cumulative [%5.2f ms/frame] [Calls: %llu, Avg: %.2f us/call]\n"
        "    GS-Packet/Render: %7.1f ms cumulative [%5.2f ms/frame] [Renders: %llu, Avg: %.2f us/call]\n"
        "    D3D12-Present   : %7.1f ms cumulative [%5.2f ms/frame] [Presents: %llu]\n"
        "    EE-Live-Compile : %s\n"
        "========================================================================================\n\n",
        static_cast<unsigned long long>(current_frame),
        static_cast<unsigned>(frames),
        fps,
        elapsed_ms, (frames > 0) ? (elapsed_ms / frames) : 0.0,
        vu_int_ms, vu_int_ms / frames, static_cast<unsigned long long>(vu_int_calls),
        vu_int_calls ? (vu_int_ms * 1000.0 / vu_int_calls) : 0.0,
        vu_cmp_ms, vu_cmp_ms / frames, static_cast<unsigned long long>(vu_cmp_calls),
        vu_cmp_calls ? (vu_cmp_ms * 1000.0 / vu_cmp_calls) : 0.0,
        gs_ms, gs_ms / frames, static_cast<unsigned long long>(gs_calls),
        gs_calls ? (gs_ms * 1000.0 / gs_calls) : 0.0,
        present_ms, present_ms / frames, static_cast<unsigned long long>(present_calls),
        ee_buf
    );

    g_last_dump_frame = current_frame;
    g_window_start = std::chrono::steady_clock::now();
}

} // namespace ps2::profile
