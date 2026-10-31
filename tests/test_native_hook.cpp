#include "ps2/native_hook.hpp"
#include "ps2/native_vertex_hook.hpp"
#include "ps2/cpu.hpp"
#include "gs/gs.hpp"

#include <cassert>
#include <cstdio>

static bool g_hook_ran = false;
static void dummy_hook_fn(ps2::Cpu &c) {
    g_hook_ran = true;
    c.r[2].w[0] = 0xCAFEBABE; // return value in v0
}

int main() {
    std::printf("=== test_native_hook: starting verification ===\n");

    // 1. Verify initial replacement hook state
    assert(!ps2::hook::has_active_hooks());
    assert(ps2::hook::find_hook(0x00100000) == nullptr);

    // 2. Register replacement hook
    ps2::hook::register_hook(0x00100000, dummy_hook_fn, "dummy_function");
    assert(ps2::hook::has_active_hooks());

    // 3. Find and execute hook
    ps2::Cpu cpu{};
    cpu.pc = 0x00100000;
    auto handler = ps2::hook::find_hook(0x00100000);
    assert(handler != nullptr);
    handler(cpu);
    assert(g_hook_ran);
    assert(cpu.r[2].w[0] == 0xCAFEBABE);
    assert(ps2::hook::native_hook_executions() == 1);
    std::printf("[PASS] Native replacement hook intercepted and executed successfully\n");

    // 4. Unregister hook
    ps2::hook::unregister_hook(0x00100000);
    assert(!ps2::hook::has_active_hooks());
    assert(ps2::hook::find_hook(0x00100000) == nullptr);
    std::printf("[PASS] Native hook unregistration verified\n");

    // 5. Verify native vertex buffer hook default is DISABLED
    assert(!ps2::vertex::is_enabled());
    ps2::gs::State state{};
    ps2::gs::Vertex v[3]{};
    bool batched = ps2::vertex::submit_primitive(state, v, 3);
    assert(!batched); // Must return false when disabled so standard rendering pipeline executes
    std::printf("[PASS] Native vertex hook is disabled by default and passes through to authentic pipeline\n");

    // 6. Test explicit opt-in behavior
    ps2::vertex::set_enabled(true);
    assert(ps2::vertex::is_enabled());
    batched = ps2::vertex::submit_primitive(state, v, 3);
    assert(batched);
    ps2::vertex::flush();
    ps2::vertex::set_enabled(false);
    assert(!ps2::vertex::is_enabled());
    std::printf("[PASS] Native vertex hook explicit enable/disable verified\n");

    std::printf("=== test_native_hook: ALL ASSERTIONS PASSED ===\n");
    return 0;
}
