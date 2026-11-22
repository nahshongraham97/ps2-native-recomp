#include "mka/retail_input_assignments.hpp"
#include <array>
#include <cstdio>
#include <stdexcept>
#include <thread>
#include <type_traits>

using namespace mka::retail;
namespace {
void check(bool value, const char* text) { if (!value) throw std::runtime_error(text); }
std::uint32_t control(const std::shared_ptr<NativeRetailInputRoles>& roles, unsigned id) {
    RetailInputRoleFailure failure;
    auto value = roles->observe(static_cast<RetailInputRole>(id), failure);
    check(bool(value), "actual role observation");
    return value->source_control_word;
}
std::uint32_t mapping(const std::shared_ptr<NativeRetailInputRoles>& roles, unsigned id) {
    RetailInputRoleFailure failure;
    auto value = roles->profile_mapping_word(id, failure);
    check(bool(value), "actual retained mapping observation");
    return *value;
}
}
int main() try {
    static_assert(!std::is_default_constructible_v<NativeRetailRoleControlReference>);
    RetailInputRoleFailure rf;
    RetailParticipantFailure pf;
    RetailInputAssignmentFailure af;
    auto roles = NativeRetailInputRoles::create(rf);
    auto people = NativeRetailParticipants::create_first_two(pf);
    auto assignments = NativeRetailInputAssignments::create(roles, people, af);
    auto second_adapter = NativeRetailInputAssignments::create(roles, people, af);
    check(roles && people && assignments && second_adapter, "actual one-domain owners");
    auto first = people->first_two(0, pf), second = people->first_two(1, pf);
    check(first && second, "actual participants");
    check(mapping(roles, 0) == 0 && mapping(roles, 1) == 0, "original BSS zero births");

    // Source153660 and281550 storage-leaf fixtures. Their complete task/menu
    // producers are unimplemented and are not claimed reached by this client.
    const std::array<std::uint32_t, 4> initial{0x81234567u, 0x2468ace0u, 0xffffffffu, 0x13579bdfu};
    for (unsigned i = 0; i < 4; ++i)
        check(roles->store_source_control_word_from_source(static_cast<RetailInputRole>(i), initial[i], rf), "nonzero full-word source leaf fixture");
    check(roles->store_profile_mapping_active_from_source(0, true, rf) &&
          roles->store_profile_mapping_active_from_source(1, true, rf), "same existing mapping words seeded");
    check(!assignments->role_control_from_281830(0, af) && af.kind == RetailInputAssignmentError::none,
          "source negative role is null, distinct from failure");
    check(!assignments->role_control_from_281830(2, af) && af.kind == RetailInputAssignmentError::foreign_participant,
          "unsupported participant fails by name");
    check(assignments->reset_profile_controls_from_2817b0({}, af) &&
          assignments->reset_profile_controls_from_2817b0(first, af), "null and live initial ordinal5 have no stores");
    for (unsigned i = 0; i < 4; ++i) check(control(roles, i) == initial[i], "no-effect branches preserve every role");
    check(mapping(roles, 0) == 1 && mapping(roles, 1) == 1, "no-effect branches preserve both mapping words");

    check(assignments->bind_direct(first, RetailInputRole::role2, af) &&
          assignments->bind_direct(second, RetailInputRole::role0, af) &&
          assignments->increment_count_before_280c50_bind(af), "actual bound ordinals0/1 and nonzero shared count");
    auto captured = assignments->role_control_from_281830(0, af);
    check(captured && captured->role() == RetailInputRole::role2 && captured->observe(rf) == initial[2],
          "281830 captures the actual selected role control");
    check(second_adapter->reset_profile_controls_from_2817b0(first, af), "2817B0 ordinal0 through another adapter");
    check(control(roles, 2) == 0 && control(roles, 0) == initial[0] && control(roles, 1) == initial[1] && control(roles, 3) == initial[3],
          "source ordinal0 clears only assigned role2");
    check(mapping(roles, 0) == 0 && mapping(roles, 1) == 1 && assignments->association_count(af) == 1,
          "ordinal0 clears shared64EA94 only, preserving count");
    check(assignments->reset_profile_controls_from_2817b0(second, af) &&
          control(roles, 0) == 0 && mapping(roles, 1) == 0, "ordinal1 clears role0 and shared64EA90");
    check(first->observe(pf)->assigned_role == 2 && second->observe(pf)->assigned_role == 0 &&
          roles->observe(RetailInputRole::role2, rf)->participant == first &&
          roles->observe(RetailInputRole::role0, rf)->participant == second && assignments->association_count(af) == 1,
          "reset is not detach and preserves inverse associations and count");

    check(assignments->detach(first, af) && roles->store_profile_mapping_active_from_source(0, true, rf) &&
          assignments->reset_profile_controls_from_2817b0(first, af) && mapping(roles, 0) == 0,
          "retained ordinal0 still clears mapping when its role is negative");
    check(assignments->bind_direct(first, RetailInputRole::role3, af) &&
          roles->store_source_control_word_from_source(RetailInputRole::role2, 0xfeedbeefu, rf), "rebind after capture");
    check(captured->clear_from_245dd0(rf) && control(roles, 2) == 0 && control(roles, 3) == initial[3],
          "245F14 clear retains captured old role identity after rebind");
    std::optional<NativeRetailRoleControlReference> copy(std::move(*captured));
    check(copy->observe(rf) == 0 && captured->observe(rf) == 0, "reference rvalue copies retain valid actual owner");

    auto other_people = NativeRetailParticipants::create_first_two(pf);
    auto foreign = other_people->first_two(0, pf);
    check(!assignments->reset_profile_controls_from_2817b0(foreign, af) && af.kind == RetailInputAssignmentError::foreign_participant,
          "foreign participant cannot clear this domain");
    check(!roles->store_source_control_word_from_source(static_cast<RetailInputRole>(4), 0, rf) && rf.kind == RetailInputRoleError::invalid_role &&
          !roles->store_profile_mapping_active_from_source(2, true, rf) && rf.kind == RetailInputRoleError::invalid_mapping_table,
          "invalid native identities are named failures");
    bool wrong_thread = false;
    std::thread worker([&] {
        RetailInputAssignmentFailure f; RetailInputRoleFailure r;
        wrong_thread = !assignments->role_control_from_281830(0, f) && f.kind == RetailInputAssignmentError::wrong_thread &&
            !assignments->reset_profile_controls_from_2817b0(first, f) && f.kind == RetailInputAssignmentError::wrong_thread &&
            !copy->clear_from_245dd0(r) && r.kind == RetailInputRoleError::wrong_thread;
    });
    worker.join();
    check(wrong_thread && control(roles, 3) == initial[3] && control(roles, 1) == initial[1], "foreign thread cannot publish partial control reset");
    check(roles->store_profile_mapping_active_from_source(0, true, rf) &&
          roles->store_profile_mapping_active_from_source(1, true, rf) && roles->reset_first_two_mapping_tables_from_281870(rf) &&
          mapping(roles, 0) == 0 && mapping(roles, 1) == 0,
          "existing281870 resets the same observed words, with no shadow profile state");
    std::weak_ptr<NativeRetailInputRoles> retained = roles;
    assignments.reset(); second_adapter.reset(); people.reset(); first.reset(); second.reset(); roles.reset();
    check(!retained.expired() && copy->clear_from_245dd0(rf) && copy->observe(rf) == 0 && captured->observe(rf) == 0,
          "captured references retain the actual owner after external/domain handles retire");
    copy.reset(); captured.reset();
    check(retained.expired(), "last captured reference releases the actual role owner");
    std::puts("PASS_NATIVE_PROFILE_ROLE_LOOKUP_LIVE_ORDINAL_RESET_AND_CAPTURED_OWNER;full profile bindings remain required");
    return 0;
} catch (const std::exception& e) {
    std::fprintf(stderr, "profile input reset: %s\n", e.what()); return 1;
}
