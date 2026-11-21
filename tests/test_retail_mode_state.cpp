#include "mka/retail_mode_state.hpp"
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
using namespace mka::retail;
namespace {
bool fail_allocation = false;
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
}
void* operator new(std::size_t n) {
    if (fail_allocation) throw std::bad_alloc();
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
int main() { try {
    NativeRetailModeState state;
    RetailModeFailure f;
    check(state.top() == 0 && state.top_past_21() == 0 && !state.contains(0), "empty is distinct from a stored zero");
    state.pop();
    fail_allocation = true;
    bool pushed = state.push(0, f);
    fail_allocation = false;
    check(!pushed && f.kind == RetailModeError::allocation && !state.contains(0), "failed first allocation leaves source-empty state");
    check(state.push(0, f) && state.contains(0), "actual zero entry stored");
    fail_allocation = true;
    pushed = state.push(0, f);
    fail_allocation = false;
    check(pushed, "same top is suppressed before any allocation");
    for (int value = 1; value <= 8; ++value) {
        fail_allocation = true;
        pushed = state.push(value, f);
        fail_allocation = false;
        if (!pushed) {
            check(f.kind == RetailModeError::allocation && state.top() == value - 1 && !state.contains(value),
                "actual growth failure retains complete old history");
            check(state.push(value, f), "retry admits same source entry");
        }
    }
    check(state.top() == 8, "old top index7 advances to source slot8");
    fail_allocation = true;
    pushed = state.push(9, f);
    fail_allocation = false;
    check(pushed && state.top() == 9 && !state.contains(8) && state.contains(7), "source replaces last slot without losing first eight");
    state.pop(); check(state.top() == 7, "pop restores previous source slot");
    for (int i = 0; i != 20; ++i) state.pop();
    check(state.top() == 0 && !state.contains(0), "pop floors at empty");
    check(state.push(21, f) && state.top_past_21() == 21, "all21 returns last loaded21, not empty zero");
    state.clear();
    check(state.push(6, f) && state.push(21, f) && state.top() == 21 && state.top_past_21() == 6,
        "normal top and skip21 readers remain distinct");
    check(state.push(INT_MIN, f) && state.top() == INT_MIN && state.contains(INT_MIN), "negative signed source mode accepted");
    check(state.push(INT_MAX, f) && state.top() == INT_MAX, "nonnegative source values are not limited by misleading branch shape");
    state.clear(); check(state.top() == 0 && !state.contains(INT_MAX), "actual reset discards active history");
    std::puts("PASS native retail mode history source edge/failure checks; no game or endpoint");
    return 0;
} catch (const std::exception& e) {
    fail_allocation = false; std::fprintf(stderr, "FAIL mode history: %s\n", e.what()); return 1;
}}
