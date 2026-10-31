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
            }

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
            return;
        }
    }
}

void notify_observer(const Cpu &cpu, std::uint32_t depth) {
    notify_observer_pc(cpu.pc, cpu, depth);
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

void reset_observers() {
    g_observers.clear();
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

std::uint64_t g_last_dump_frame = 0;
std::chrono::steady_clock::time_point g_window_start;

void init_profiler() {
    if (g_profiler_init) return;
    g_profiler_init = true;
    const char *env = std::getenv("PS2_PROFILE_SUBSYSTEMS");
    if (env && (std::strcmp(env, "1") == 0 || std::strcmp(env, "true") == 0)) {
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
}

void record_ee_live(std::uint64_t ns) {
    if (!g_profiler_enabled) return;
    g_ns_ee_live.fetch_add(ns, std::memory_order_relaxed);
}

void dump_summary_if_due(std::uint64_t current_frame) {
    if (!g_profiler_enabled) return;
    if (current_frame == 0 || current_frame - g_last_dump_frame < 120) return;

    const auto now = std::chrono::steady_clock::now();
    const double elapsed_ms = std::chrono::duration<double, std::milli>(now - g_window_start).count();
    const std::uint64_t frames = current_frame - g_last_dump_frame;
    const double fps = frames / (elapsed_ms / 1000.0);

    const double vu_int_ms = g_ns_vu_interpret.exchange(0) / 1000000.0;
    const double vu_cmp_ms = g_ns_vu_compiled.exchange(0) / 1000000.0;
    const double gs_ms = g_ns_gs_draw.exchange(0) / 1000000.0;
    const double present_ms = g_ns_present.exchange(0) / 1000000.0;
    const double ee_live_ms = g_ns_ee_live.exchange(0) / 1000000.0;

    const std::uint64_t vu_int_calls = g_calls_vu_interpret.exchange(0);
    const std::uint64_t vu_cmp_calls = g_calls_vu_compiled.exchange(0);
    const std::uint64_t gs_calls = g_calls_gs_draw.exchange(0);

    const double measured_total = vu_int_ms + vu_cmp_ms + gs_ms + present_ms + ee_live_ms;
    const double ee_exec_ms = std::max(0.0, elapsed_ms - measured_total);

    std::fprintf(stderr,
        "\n================ [Subsystem Profile — Frame %llu (%u frames @ %.1f FPS)] ================\n"
        "  VU1-Interpreted : %7.1f ms (%5.1f%%) [Calls: %llu, Avg: %.2f us/call]\n"
        "  VU1-Compiled    : %7.1f ms (%5.1f%%) [Calls: %llu, Avg: %.2f us/call]\n"
        "  GS-Packet/Draw  : %7.1f ms (%5.1f%%) [Draws: %llu]\n"
        "  D3D12-Present   : %7.1f ms (%5.1f%%)\n"
        "  EE-Live-Compile : %7.1f ms (%5.1f%%)\n"
        "  EE-Exec / Host  : %7.1f ms (%5.1f%%)\n"
        "  Total Window    : %7.1f ms (Avg Frame: %.2f ms)\n"
        "========================================================================================\n\n",
        static_cast<unsigned long long>(current_frame),
        static_cast<unsigned>(frames),
        fps,
        vu_int_ms, 100.0 * vu_int_ms / elapsed_ms, static_cast<unsigned long long>(vu_int_calls),
        vu_int_calls ? (vu_int_ms * 1000.0 / vu_int_calls) : 0.0,
        vu_cmp_ms, 100.0 * vu_cmp_ms / elapsed_ms, static_cast<unsigned long long>(vu_cmp_calls),
        vu_cmp_calls ? (vu_cmp_ms * 1000.0 / vu_cmp_calls) : 0.0,
        gs_ms, 100.0 * gs_ms / elapsed_ms, static_cast<unsigned long long>(gs_calls),
        present_ms, 100.0 * present_ms / elapsed_ms,
        ee_live_ms, 100.0 * ee_live_ms / elapsed_ms,
        ee_exec_ms, 100.0 * ee_exec_ms / elapsed_ms,
        elapsed_ms, elapsed_ms / frames
    );

    g_last_dump_frame = current_frame;
    g_window_start = std::chrono::steady_clock::now();
}

} // namespace ps2::profile
