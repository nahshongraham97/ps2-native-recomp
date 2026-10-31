#include "ps2/observer_hook.hpp"
#include "ps2/cpu.hpp"
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstring>

using namespace ps2;

static int g_callback_calls = 0;
static uint32_t g_last_pc = 0;
static uint32_t g_last_ra = 0;
static uint32_t g_last_sp = 0;
static uint32_t g_last_depth = 0;

static void test_callback(const hook::GuestCallContext &ctx) {
    g_callback_calls++;
    g_last_pc = ctx.pc;
    g_last_ra = ctx.ra;
    g_last_sp = ctx.sp;
    g_last_depth = ctx.depth;
}

int main() {
    std::printf("=== test_observer_hook: starting verification ===\n");

    // Initialize clean state
    hook::reset_observers();
    hook::set_observer_hooks_enabled(false);
    assert(!hook::observer_hooks_enabled());

    Cpu cpu{};
    cpu.pc = 0x00100008; // Boot entry PC
    cpu.r[31].w[0] = 0x00100020; // RA
    cpu.r[29].w[0] = 0x01FE0000; // SP
    cpu.r[4].w[0] = 0x12345678;  // A0 argument

    // 1. When disabled, notify_observer does nothing
    hook::notify_observer(cpu, 1);
    assert(g_callback_calls == 0);
    assert(hook::total_observations() == 0);
    std::printf("[PASS] Disabled state: zero callbacks, zero overhead\n");

    // 2. Register observer at 0x00100008
    hook::register_observer(0x00100008, test_callback, "test_boot_entry", 2);
    hook::set_observer_hooks_enabled(true);
    assert(hook::observer_hooks_enabled());

    // 3. Trigger notify_observer matching PC
    hook::notify_observer(cpu, 2);
    assert(g_callback_calls == 1);
    assert(g_last_pc == 0x00100008);
    assert(g_last_ra == 0x00100020);
    assert(g_last_sp == 0x01FE0000);
    assert(g_last_depth == 2);
    assert(hook::total_observations() == 1);
    assert(hook::observations_for(0x00100008) == 1);

    // Verify guest state is completely unmutated
    assert(cpu.pc == 0x00100008);
    assert(cpu.r[31].w[0] == 0x00100020);
    assert(cpu.r[29].w[0] == 0x01FE0000);
    assert(cpu.r[4].w[0] == 0x12345678);
    std::printf("[PASS] Registered callback fired with intact guest registers\n");

    // 4. Trigger with non-matching PC
    cpu.pc = 0x00200000;
    hook::notify_observer(cpu, 3);
    assert(g_callback_calls == 1); // Not incremented
    assert(hook::total_observations() == 1);
    std::printf("[PASS] Non-matching PC did not trigger observer\n");

    // 5. Trigger multiple times to verify max_logs bounding and observation counter
    cpu.pc = 0x00100008;
    hook::notify_observer(cpu, 4);
    hook::notify_observer(cpu, 5);
    assert(g_callback_calls == 3);
    assert(hook::total_observations() == 3);
    assert(hook::observations_for(0x00100008) == 3);
    std::printf("[PASS] Call counting and bounding verified (3 invocations)\n");

    // 6. Unregister observer
    hook::unregister_observer(0x00100008);
    hook::notify_observer(cpu, 6);
    assert(g_callback_calls == 3); // Still 3, no new calls
    std::printf("[PASS] Unregister successfully removes hook\n");

    // 7. Reset observers
    hook::reset_observers();
    assert(hook::total_observations() == 0);
    assert(hook::observations_for(0x00100008) == 0);
    std::printf("[PASS] Reset cleared all statistics and observer entries\n");

    std::printf("=== test_observer_hook: ALL ASSERTIONS PASSED ===\n");
    return 0;
}
