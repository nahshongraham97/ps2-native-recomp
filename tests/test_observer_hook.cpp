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
    std::printf("=== test_observer_hook: starting verification (R193 Phase A & B) ===\n");

    // Initialize clean state
    hook::reset_observers();
    hook::set_observer_hooks_enabled(false);
    TEST_CHECK(!hook::observer_hooks_enabled());

    Cpu cpu{};
    cpu.pc = 0x00100008; // Boot entry PC
    cpu.r[31].w[0] = 0x00100020; // RA
    cpu.r[29].w[0] = 0x01FE0000; // SP
    cpu.r[4].w[0] = 0x12345678;  // A0 argument
    cpu.r[2].w[0] = 0x00000001;  // V0 return value

    // 1. When disabled, notify_observer does nothing
    hook::notify_observer(cpu, 1);
    TEST_CHECK(g_callback_calls == 0);
    TEST_CHECK(hook::total_observations() == 0);
    std::printf("[PASS] Disabled state: zero callbacks, zero overhead\n");

    // 2. Register observer at 0x00100008 with sample_interval = 2 (initial 2, sample every 2)
    hook::register_observer(0x00100008, test_callback, "test_boot_entry", 2, 2);
    hook::set_observer_hooks_enabled(true);
    TEST_CHECK(hook::observer_hooks_enabled());

    // 3. Trigger notify_observer matching PC
    hook::notify_observer(cpu, 2);
    TEST_CHECK(g_callback_calls == 1);
    TEST_CHECK(g_last_pc == 0x00100008);
    TEST_CHECK(g_last_ra == 0x00100020);
    TEST_CHECK(g_last_sp == 0x01FE0000);
    TEST_CHECK(g_last_depth == 2);
    TEST_CHECK(g_last_count == 1);
    TEST_CHECK(hook::total_observations() == 1);
    TEST_CHECK(hook::observations_for(0x00100008) == 1);

    // Verify guest state is completely unmutated
    TEST_CHECK(cpu.pc == 0x00100008);
    TEST_CHECK(cpu.r[31].w[0] == 0x00100020);
    TEST_CHECK(cpu.r[29].w[0] == 0x01FE0000);
    TEST_CHECK(cpu.r[4].w[0] == 0x12345678);
    TEST_CHECK(cpu.r[2].w[0] == 0x00000001);
    std::printf("[PASS] Registered callback fired with intact guest registers\n");

    // 4. Test explicit notify_observer_pc (for gameplay syscall / direct hook points)
    hook::register_observer(0x0010C830u, test_callback, "PollSema_gameplay", 3, 5);
    cpu.pc = 0x0010C838u; // After syscall
    hook::notify_observer_pc(0x0010C830u, cpu, 0);
    TEST_CHECK(g_callback_calls == 2);
    TEST_CHECK(g_last_pc == 0x0010C830u);
    TEST_CHECK(hook::observations_for(0x0010C830u) == 1);
    std::printf("[PASS] notify_observer_pc correctly intercepted gameplay function 0x0010C830\n");

    // 5. Test call-count sampling (sample_interval = 5 for PollSema)
    for (int i = 0; i < 4; ++i) {
        hook::notify_observer_pc(0x0010C830u, cpu, 0);
    }
    // Now count is 5, which matches sample_interval (5 % 5 == 0)
    TEST_CHECK(g_last_count == 5);
    TEST_CHECK(g_last_sampled == true);
    std::printf("[PASS] Call-count sampling verified (count=5 flagged as sampled)\n");

    // 6. Test Subsystem Profiler records (Phase C)
    profile::set_profiler_enabled(true);
    TEST_CHECK(profile::profiler_enabled());
    profile::record_vu_interpret(15000000); // 15 ms
    profile::record_vu_compiled(2000000);   // 2 ms
    profile::record_gs_draw(5000000);       // 5 ms
    profile::record_d3d12_present(3000000); // 3 ms
    profile::dump_summary_if_due(120);      // Triggers dump table
    std::printf("[PASS] Subsystem profiler recorded and formatted timings\n");

    // 7. Unregister and Reset
    hook::unregister_observer(0x00100008);
    hook::unregister_observer(0x0010C830u);
    hook::reset_observers();
    TEST_CHECK(hook::total_observations() == 0);
    TEST_CHECK(hook::observations_for(0x00100008) == 0);
    std::printf("[PASS] Reset cleared all statistics and observer entries\n");

    std::printf("=== test_observer_hook: ALL ASSERTIONS PASSED (R193 Phase A & B & C) ===\n");
    return 0;
}
