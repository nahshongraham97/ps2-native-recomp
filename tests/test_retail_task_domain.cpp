#include "mka/retail_task_domain.hpp"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <thread>

namespace {
void check(bool good, const char* label) {
    if (!good) {
        std::fprintf(stderr, "retail task domain: %s\n", label);
        std::abort();
    }
}
struct RealTestBody final : mka::retail::NativeRetailTaskBody {
    mka::retail::NativeTaskStep run() override {
        return mka::retail::NativeTaskStep::returned(1.0f);
    }
    void cleanup(bool) override { ++cleanup_calls; }
    int cleanup_calls{};
};
}

int main() {
    using namespace mka::retail;
    RetailGameControlsFailure controls_failure;
    auto controls = NativeRetailGameControls::create(controls_failure);
    check(bool(controls), "real shared game controls");
    RetailTaskDomainFailure failure;
    auto domain = NativeRetailTaskDomain::create(controls, failure);
    check(bool(domain), "persistent source data and BSS birth");
    auto birth = domain->observe(failure);
    check(birth && birth->identity_counter == 0 && !birth->override_pending &&
              birth->live_current_word == 5 && birth->live_partner_word == 5 &&
              !birth->task_roots_born && !domain->prior_registry_if_born(failure) &&
              failure.kind == RetailTaskDomainError::none,
          "counter BSS and actual ELF live pair; no synthetic registry");

    check(domain->seed_counter_from_185ee0(failure) &&
              domain->observe(failure)->identity_counter == 0xf0000000u &&
              domain->seed_counter_from_185ee0(failure),
          "185EE0 unsigned seed and high counter preservation");
    check(domain->reset_task_roots_from_12f0d0(failure),
          "12F0D0 creates the one real registry");
    auto* registry = domain->registry(failure);
    check(registry && registry == domain->prior_registry_if_born(failure),
          "current and prior lookup share same registry owner");
    auto cookie = domain->next_cookie_from_130a50(failure);
    check(cookie && *cookie == 0xefffffffu,
          "130A50 decrements counter and joins live C0 low bit");
    check(domain->override_cookie_from_130ab0(0x12345678u, failure),
          "130AB0 writes one-shot override");
    cookie = domain->next_cookie_from_130a50(failure);
    check(cookie && *cookie == 0x12345678u &&
              domain->observe(failure)->identity_counter == 0xefffffffu,
          "override consumes without counter decrement");
    cookie = domain->next_cookie_from_130a50(failure);
    check(cookie && *cookie == 0xeffffffeu,
          "next ordinary cookie resumes source decrement");

    auto update = domain->begin_live_pair_from_28e2b0(0, 0, failure);
    check(update && update->current_word == 1 && update->partner_word == 0 &&
              update->required_followup == RetailTaskLiveFollowup::none,
          "selection BSS default branch writes real 1/0 pair");
    check(domain->apply_live_pair_from_28e280(failure) &&
              domain->observe(failure)->live_current_word == 5,
          "28E280 restores 5/5 when guard zero");
    check(controls->store_selection_flags_from_source(0x02, controls_failure),
          "live selection flag source writer");
    update = domain->begin_live_pair_from_28e2b0(0, 0, failure);
    check(update && update->current_word == 0 && update->partner_word == 1 &&
              update->required_followup == RetailTaskLiveFollowup::none,
          "28E2B0 bit1 branch writes 0/1 without optional call");
    cookie = domain->next_cookie_from_130a50(failure);
    check(cookie && (*cookie & 0x80000000u) == 0,
          "130A50 sees changed live C0 parity");
    check(domain->apply_live_pair_from_28e280(failure),
          "28E280 clears selection-produced pair");

    std::thread foreign([&] {
        RetailTaskDomainFailure other;
        check(!domain->next_cookie_from_130a50(other) &&
                  other.kind == RetailTaskDomainError::wrong_thread,
              "foreign thread cannot consume identity counter");
    });
    foreign.join();

    auto body = std::make_shared<RealTestBody>();
    RetailTaskRegistryFailure registry_failure;
    auto task = registry->publish_task(0x2001, 0x20, 0x55, body,
                                       registry_failure);
    check(bool(task), "real body enters same registry");
    check(!domain->reset_task_roots_from_12f0d0(failure) &&
              failure.kind == RetailTaskDomainError::active_registry_reset &&
              domain->registry(failure) == registry && body->cleanup_calls == 0,
          "reentry refuses to silently drop live body");

    auto pending = NativeRetailTaskDomain::create(controls, failure);
    check(bool(pending), "independent identity epoch for optional source call");
    check(controls->store_selection_flags_from_source(0x02, controls_failure),
          "select bit1 again");
    update = pending->begin_live_pair_from_28e2b0(1, 0, failure);
    check(update &&
              update->required_followup == RetailTaskLiveFollowup::source_245070 &&
              !pending->next_cookie_from_130a50(failure) &&
              failure.kind == RetailTaskDomainError::pending_source_followup,
          "unbound nonempty followup cannot become successful cookie path");
    std::puts("native retail task domain: PASS");
}
