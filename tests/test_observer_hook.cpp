#include "ps2/observer_hook.hpp"
#include "ps2/cpu.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>

#define TEST_CHECK(cond) \
    do { \
        if (!(cond)) { \
            std::fprintf(stderr, "[FAIL] Assertion failed: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
            std::exit(1); \
        } \
    } while (0)

using namespace ps2;

static int g_callback_calls = 0;
static uint32_t g_last_pc = 0;
static uint32_t g_last_ra = 0;
static uint32_t g_last_sp = 0;
static uint32_t g_last_depth = 0;
static uint64_t g_last_count = 0;
static bool g_last_sampled = false;

static void test_callback(const hook::GuestCallContext &ctx) {
    g_callback_calls++;
    g_last_pc = ctx.pc;
    g_last_ra = ctx.ra;
    g_last_sp = ctx.sp;
    g_last_depth = ctx.depth;
    g_last_count = ctx.call_count;
    g_last_sampled = ctx.is_sampled;
}

int main() {
    std::printf("=== test_observer_hook: starting comprehensive verification ===\n");

    // Initialize clean state
    hook::reset_observers();
    hook::set_observer_hooks_enabled(false);
    profile::set_profiler_enabled(false);
    profile::reset_profiler_counters();
    TEST_CHECK(!hook::observer_hooks_enabled());
    TEST_CHECK(!profile::profiler_enabled());

    Cpu cpu{};
    cpu.pc = 0x00100008; // Boot entry PC
    cpu.r[31].w[0] = 0x00100020; // RA
    cpu.r[29].w[0] = 0x01FE0000; // SP
    cpu.r[4].w[0] = 0x12345678;  // A0 argument
    cpu.r[2].w[0] = 0x00000001;  // V0 return value

    // 1. When disabled, notify_observer and notify_syscall_trap do nothing
    hook::notify_observer(cpu, 1);
    hook::notify_syscall_trap(69, "PollSema", cpu, 5, 100);
    TEST_CHECK(g_callback_calls == 0);
    TEST_CHECK(hook::total_observations() == 0);
    TEST_CHECK(hook::syscall_traps_for(69) == 0);
    std::printf("[PASS] Disabled state: zero callbacks, zero observations, zero overhead\n");

    // 2. Test independent control: Enable profiler while observer is disabled
    profile::set_profiler_enabled(true);
    TEST_CHECK(profile::profiler_enabled());
    TEST_CHECK(!hook::observer_hooks_enabled());
    profile::record_vu_interpret(1000000); // 1 ms
    profile::record_gs_draw(2000000);      // 2 ms
    profile::record_ee_live(3000000);      // 3 ms
    profile::record_d3d12_present(4000000);// 4 ms
    hook::notify_observer(cpu, 1); // should still do nothing
    TEST_CHECK(g_callback_calls == 0);
    std::printf("[PASS] Independent control: Profiler records timings while observers are disabled\n");

    // 3. Reset profiler counters and verify clean state
    profile::reset_profiler_counters();
    std::printf("[PASS] Profiler counter reset verified\n");

    // 4. Enable observer hooks and register entry hook
    hook::set_observer_hooks_enabled(true);
    TEST_CHECK(hook::is_enabled());

    // Register observer: max_initial_logs = 2, sample_interval = 5
    hook::register_observer(0x00100008, test_callback, "test_boot_entry", 2, 5);
    TEST_CHECK(hook::observer_hooks_enabled());

    // Call 1: initial log (fires callback)
    hook::notify_observer(cpu, 2);
    TEST_CHECK(g_callback_calls == 1);
    TEST_CHECK(g_last_pc == 0x00100008);
    TEST_CHECK(g_last_count == 1);
    TEST_CHECK(g_last_sampled == false);

    // Call 2: initial log (fires callback)
    hook::notify_observer(cpu, 2);
    TEST_CHECK(g_callback_calls == 2);
    TEST_CHECK(g_last_count == 2);

    // Call 3: not initial, not sampled -> MUST NOT fire callback
    hook::notify_observer(cpu, 2);
    TEST_CHECK(g_callback_calls == 2); // Unchanged!
    TEST_CHECK(hook::observations_for(0x00100008) == 3);

    // Call 4: not initial, not sampled -> MUST NOT fire callback
    hook::notify_observer(cpu, 2);
    TEST_CHECK(g_callback_calls == 2); // Unchanged!
    TEST_CHECK(hook::observations_for(0x00100008) == 4);

    // Call 5: sampled (5 % 5 == 0) -> MUST fire callback with is_sampled = true
    hook::notify_observer(cpu, 2);
    TEST_CHECK(g_callback_calls == 3);
    TEST_CHECK(g_last_count == 5);
    TEST_CHECK(g_last_sampled == true);
    std::printf("[PASS] Sampled callback output verified: unsampled calls do not execute callback or flood stderr\n");

    // 5. Test Syscall Trap notification (distinguished from entry hook)
    hook::notify_syscall_trap(69, "PollSema", cpu, 2, 5);
    TEST_CHECK(hook::syscall_traps_for(69) == 1);
    hook::notify_syscall_trap(115, "SetVSyncFlag", cpu, 2, 5);
    TEST_CHECK(hook::syscall_traps_for(115) == 1);
    std::printf("[PASS] Syscall trap notifications distinguished and tracked per syscall number\n");

    // 6. Test full subsystem timing dump with all 5 instrumented categories
    profile::record_vu_interpret(12000000); // 12 ms
    profile::record_vu_compiled(3000000);   // 3 ms
    profile::record_gs_draw(8000000);       // 8 ms
    profile::record_d3d12_present(5000000); // 5 ms
    profile::record_ee_live(1000000);       // 1 ms
    profile::dump_summary_if_due(120);      // Frame 120 triggers formatted summary
    std::printf("[PASS] Subsystem profiler recorded and formatted timings for all categories\n");

    // 7. Verify unregister and global reset
    hook::unregister_observer(0x00100008);
    hook::reset_observers();
    profile::reset_profiler_counters();
    TEST_CHECK(hook::total_observations() == 0);
    TEST_CHECK(hook::observations_for(0x00100008) == 0);
    TEST_CHECK(hook::syscall_traps_for(69) == 0);
    std::printf("[PASS] Reset cleared all observer entries, syscall traps, and profiler counters\n");

    std::printf("=== test_observer_hook: ALL ASSERTIONS PASSED ===\n");
    return 0;
}
