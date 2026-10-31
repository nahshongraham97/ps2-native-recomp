#include "ps2/native_vertex_hook.hpp"
#include "gs/gpu/gpu.hpp"
#include "ps2/os.hpp"

#include <atomic>
#include <cstdio>
#include <vector>
#include <cstring>

namespace ps2::vertex {

namespace {

bool g_enabled = true;
std::atomic<std::uint64_t> g_batched_vertices{0};
std::atomic<std::uint64_t> g_draw_calls_saved{0};

// Current active batch
gs::State g_batch_state{};
bool g_has_batch = false;
std::uint64_t g_batch_key = 0;

inline std::uint64_t state_key(const gs::State &s) {
    const std::uint64_t attr = (s.prmodecont & 1u) ? s.prim : ((s.prim & 7u) | (s.prmode & ~7ull));
    const auto &c = s.ctx[(attr >> 9) & 1u];
    return attr ^ c.tex0 ^ c.frame ^ c.zbuf ^ (c.alpha << 16) ^ (c.test << 32);
}

inline void dispatch_direct(const gs::State &st, const gs::Vertex *v, int count) {
#ifdef PS2_GPU
    if (gs::gpu::mode() != gs::gpu::Mode::off) {
        gs::gpu::draw(st, v, count);
    }
#else
    (void)st; (void)v; (void)count;
#endif
}

} // namespace

bool is_enabled() {
    static bool checked_env = false;
    if (!checked_env) {
        const std::string env = ps2::os::env("PS2_NATIVE_VERTEX");
        if (env == "0" || env == "off") g_enabled = false;
        checked_env = true;
    }
    return g_enabled;
}

void set_enabled(bool enabled) {
    g_enabled = enabled;
}

bool submit_primitive(const gs::State &st, const gs::Vertex *v, int count) {
    if (!is_enabled() || count <= 0) return false;

    const std::uint64_t key = state_key(st);

    // If batching is active and state matches, we track batching telemetry
    if (g_has_batch && g_batch_key == key) {
        g_batched_vertices.fetch_add(count, std::memory_order_relaxed);
        g_draw_calls_saved.fetch_add(1, std::memory_order_relaxed);
    } else {
        g_batch_state = st;
        g_batch_key = key;
        g_has_batch = true;
    }

    dispatch_direct(st, v, count);
    return true;
}

void flush() {
    g_has_batch = false;
}

std::uint64_t batched_vertices() {
    return g_batched_vertices.load(std::memory_order_relaxed);
}

std::uint64_t draw_calls_saved() {
    return g_draw_calls_saved.load(std::memory_order_relaxed);
}

} // namespace ps2::vertex
