#include "mka/retail_input_roles.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>
#include <stdexcept>
#include <thread>
using namespace mka::retail;
namespace {
bool fail_allocation = false;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
}
void* operator new(std::size_t n) {
    if (fail_allocation) throw std::bad_alloc();
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
int main() { try {
    RetailInputRoleFailure f;
    fail_allocation = true;
    auto failed = NativeRetailInputRoles::create(f);
    fail_allocation = false;
    check(!failed && f.kind == RetailInputRoleError::allocation, "role allocation failure publishes no owner");
    auto roles = NativeRetailInputRoles::create(f);
    check(bool(roles), "native four-role owner constructed");
    const std::array<std::uint32_t, 16> bodies{0x1607c0,0x1604c0,0x160a10,0x160640,
        0x161110,0x160b90,0x160ef0,0x160d40,0x160100,0x15ff60,0x15fe10,0x160250,
        0x1616b0,0x1612c0,0x161560,0x161410};
    const std::array<std::uint32_t, 16> arguments{0x62a008,0x62a010,0x62a018,0x62a020,
        0x62a028,0x62a030,0x62a038,0x62a040,0x62a048,0x62a058,0x62a068,0x62a078,
        0x62a088,0x62a090,0x62a098,0x62a0a0};
    for (unsigned index = 0; index != 4; ++index) {
        const auto role = static_cast<RetailInputRole>(index);
        const auto state = roles->observe(role, f);
        check(state && !state->initialized && !state->dispatch_blocked && state->analog_mapping &&
            !state->participant && !state->axes && state->previous == 0 && state->current == 0 && state->rising == 0,
            "retail role starts neutral and unassigned, including unpolled fourth role");
        for (unsigned row = 0; row != 16; ++row) {
            const auto binding = roles->observe_action_row(role, row, f);
            check(binding && binding->rising == 0 && binding->action.rising_mask == (1u << row) &&
                binding->action.source_body_id == bodies[row] && binding->action.source_argument_id == arguments[row],
                "all original serialized mapping triples retained as identities, never guest pointers");
            check(retail_canonical_action_index(1u << row, 99) == row, "actual canonical lookup");
        }
        check(!roles->observe_action_row(role, 16, f) && f.kind == RetailInputRoleError::invalid_action_row,
            "unknown action row cannot select a made-up binding");
    }
    check(retail_canonical_action_index(0x10000, 123) == 123, "source fallback retains caller row");
    check(!roles->observe(static_cast<RetailInputRole>(4), f) && f.kind == RetailInputRoleError::invalid_role,
        "unknown retail identity is distinct from host device capacity");
    bool rejected = false;
    std::thread worker([&] {
        RetailInputRoleFailure other;
        rejected = !roles->observe(RetailInputRole::role0, other) && other.kind == RetailInputRoleError::wrong_thread;
    }); worker.join(); check(rejected, "retail role state stays on its owning thread");
    std::puts("PASS native four-role initialization and original action table; no assignment, polling, game or endpoint");
    return 0;
} catch (const std::exception& e) {
    fail_allocation = false; std::fprintf(stderr, "FAIL input roles: %s\n", e.what()); return 1;
}}
