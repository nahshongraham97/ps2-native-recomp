#include "mka/retail_saved_profile_records.hpp"
#include "mka/retail_initial_reset_required.hpp"
#include "mka/retail_profile_scene_object.hpp"
#include "mka/retail_profile_entry_decision.hpp"
#include "mka/retail_profile_transition_child.hpp"
#include "native_retail_startup.hpp"
#include "../runtime/src/native/retail_main_control.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <new>
#include <stdexcept>
#include <thread>

using namespace mka::retail;
extern "C" int SDL_InitSubSystem(unsigned) { std::abort(); }
namespace {
thread_local bool fail_next_allocation = false;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
using Hook = void (*)(NativeRetailMainState&);
Hook menu_hook{};
bool menu_failure{};
unsigned menu_calls{};
std::weak_ptr<NativeRetailGameSession> revoked_session;
RetailSavedProfileRecordsFailure rf;
RetailGameSessionFailure gf;
NativeRetailSavedProfileRecords& records(NativeRetailMainState& main) {
    auto* result = main.startup.game_session->saved_profile_records(gf);
    check(result != nullptr, "actual persistent Session records");
    return *result;
}
struct Fixture {
    std::unique_ptr<NativeRetailStartupState> startup;
    mka::input::ControllerInput input;
    std::unique_ptr<NativeRetailMainState> main;
    std::shared_ptr<NativeRetailProfileSceneObject> scene;
    explicit Fixture(const std::filesystem::path& root) {
        check(!std::filesystem::exists(root), "exclusive fixture root");
        std::filesystem::create_directories(root / "content");
        mka::content::ContentVolumeFailure cf;
        auto volume = mka::content::NativeContentVolume::open(root / "content", cf);
        check(bool(volume), "actual native empty content volume");
        startup = std::make_unique<NativeRetailStartupState>(std::move(*volume), std::vector<std::string>{});
        StartupFailure f;
        check(initialize_native_retail_game_session(*startup, f), "actual Session birth");
        main = std::make_unique<NativeRetailMainState>(*startup, input);
        main->saved_profile_storage = mka::save::NativeSaveStore::open_or_create(root / "fixture-saves", f.saved_profile_storage);
        check(bool(main->saved_profile_storage), "actual exclusively owned native save storage");
        main->mode_history.emplace();
        RetailModeFailure mf;
        check(main->mode_history->push(0, mf), "actual source mode history");
        RetailParticipantFailure pf;
        RetailInputRoleFailure role;
        RetailInputAssignmentFailure assignment;
        main->participants = NativeRetailParticipants::create_first_two(pf);
        main->input_roles = NativeRetailInputRoles::create(role);
        main->input_assignments = NativeRetailInputAssignments::create(main->input_roles, main->participants, assignment);
        check(main->participants && main->input_roles && main->input_assignments, "actual input domain");
    }
    void enable_menu() {
        RetailModeFailure mf;
        check(main->mode_history->push(4, mf), "actual profile menu history4");
        RetailProfileSceneObjectFailure sf;
        scene = NativeRetailProfileSceneObject::create(startup->game_session, 7, {}, sf);
        RetailLiveSceneFailure lf;
        check(scene && startup->game_session->live_scene_reference(gf)->publish_from_2f1ad0(scene, lf) ==
              RetailLiveScenePublishOutcome::published, "actual live scene cookie reference");
        RetailParticipantFailure pf;
        check(main->participants->first_two(0, pf)->set_game_state(1, pf), "actual live participant source state");
    }
};
void check_prefix(NativeRetailMainState& main) {
    auto& owner = records(main);
    auto* first = owner.card_slot(0, rf);
    auto* second = owner.card_slot(1, rf);
    check(first && second && first->status_word == -1, "published first parent status before menu");
    check(first->children[7].value.phase() == RetailSavedChildPhase::source_defaults,
          "all eight actual first-parent children already reset");
    check(*owner.working_control_word(rf) == 0, "same actual Working control cleared by child leaf");
    if (menu_calls == 1) {
        check(second->status_word == 0 && second->children[0].value.phase() == RetailSavedChildPhase::zero_born,
              "first aggregate observes untouched second-parent BSS values");
        check(first->preferences.levels == std::array<float,5>{}, "first preference write follows real menu");
    } else {
        check(menu_calls == 2 && second->status_word == -1 &&
              second->children[7].value.phase() == RetailSavedChildPhase::source_defaults,
              "second aggregate precedes final parent-ready stores");
        check(first->preferences.levels == retail_profile_preference_defaults_from_2955d0().levels &&
              second->preferences.levels == std::array<float,5>{}, "each preference write follows its own aggregate/menu");
    }
}
void transfer(NativeRetailMainState& main) { throw detail::NativeMainReentry{&main, 91}; }
void recursive(NativeRetailMainState& main) {
    StartupFailure f;
    check(main_binding::initialize_retail_saved_profile_records(main, f) == RetailStartupGate::native_failure &&
          f.saved_profile_records.kind == RetailSavedProfileRecordsError::reentrant,
          "recursive source entry refused before mutation without poisoning outer call");
}
void revoke(NativeRetailMainState& main) {
    auto* owner = &records(main);
    revoked_session = main.startup.game_session;
    main.startup.game_record_reset.reset();
    main.startup.game_session.reset();
    check(!revoked_session.expired() && owner->card_slot(0, rf)->status_word == -1,
          "actual Session/records remain retained across last external release in menu");
}
void clear_participants_then_revoke(NativeRetailMainState& main) {
    RetailParticipantFailure f;
    for (unsigned i = 0; i != 2; ++i)
        check(main.participants->first_two(i, f)->set_game_state(0, f), "actual source participant status-clear fixture");
    revoke(main);
}
} // namespace
void* operator new(std::size_t size) {
    if (fail_next_allocation) { fail_next_allocation = false; throw std::bad_alloc(); }
    if (void* result = std::malloc(size ? size : 1)) return result;
    throw std::bad_alloc();
}
void operator delete(void* value) noexcept { std::free(value); }
void operator delete(void* value, std::size_t) noexcept { std::free(value); }

// Required menu descendants are fixture callouts only. The production
// aggregate/menu/transition path and both saved-profile bindings are real.
namespace mka::retail {
bool invoke_retail_profile_select_from_2f1630(NativeRetailMainState& main, std::string_view path,
    RetailProfileMenuFailure&) {
    check(path == "common/p_select/p_select", "actual source menu action literal");
    ++menu_calls;
    if (menu_hook) menu_hook(main);
    return !menu_failure;
}
std::optional<std::uint32_t> observe_retail_profile_permission_from_4eee60(
    NativeRetailMainState&, std::uint32_t, RetailProfileMenuFailure&) { return 0u; }
bool publish_retail_profile_participant_from_189690(NativeRetailMainState&,
    const std::shared_ptr<NativeRetailInputParticipant>&, RetailProfileMenuFailure&) {
    throw std::runtime_error("unexpected event producer for source index0 fixture");
}
bool apply_retail_profile_participant_from_188720(NativeRetailMainState&, std::uint32_t role,
    RetailProfileMenuFailure&) { check(role == 0, "actual profile transition role0"); return true; }
} // namespace mka::retail

int main(int argc, char** argv) try {
    check(argc == 2, "unique disposable native fixture root");
    const auto root = std::filesystem::absolute(std::filesystem::u8path(argv[1]));
    check(!std::filesystem::exists(root), "all fixture directories must be new");
    std::filesystem::create_directories(root);
    StartupFailure f;
    {
        Fixture x(root / "normal");
        auto& owner = records(*x.main);
        auto* first = owner.card_slot(0, rf);
        auto* active = owner.active_player(0, rf);
        check(first && first->status_word == 0 && active && active->card_slot == 0 && active->profile_index == 0 &&
              active->status_word == 0 && active->player_status_word == 8 &&
              active->value.phase() == RetailSavedChildPhase::zero_born, "actual BSS vs file-backed births");
        // Explicit typed SOURCE-STATE FIXTURES, not completed load/selection producers.
        active->card_slot = 1; active->profile_index = 7; active->status_word = 0x81234567;
        active->player_status_word = 99;
        first->version_word = 0x87654321; first->flag_byte_12 = 0xa5;
        first->children[4].in_use_word = 0xfeedbeef;
        *owner.working_control_word(rf) = 0xabcdef01;
        x.enable_menu(); menu_calls = 0; menu_hook = check_prefix;
        check(main_binding::initialize_retail_saved_profile_records(*x.main, f) == RetailStartupGate::ready &&
              menu_calls == 2, "actual production initializer reaches real aggregate/menu twice");
        menu_hook = nullptr;
        check(first == owner.card_slot(0, rf) && first->status_word == 1 && owner.card_slot(1, rf)->status_word == 1 &&
              first->version_word == 0x87654321 && first->flag_byte_12 == 0 && first->children[4].in_use_word == 0,
              "stable nodes and exactly reached parent stores");
        check(active->card_slot == 1 && active->profile_index == 7 && active->status_word == 0x81234567 &&
              active->player_status_word == 99 && active->value.phase() == RetailSavedChildPhase::zero_born,
              "171B10 does not invent an active-player reset");
        auto* second_active = owner.active_player(1, rf);
        second_active->card_slot = 0; second_active->profile_index = 3; second_active->status_word = 0xffffffff;
        owner.card_slot(1, rf)->children[7].in_use_word = 0x12345678;
        first->children[3].in_use_word = 0xffffffff;
        first->children[2].in_use_word = 0x13579bdf;
        RetailParticipantFailure pf; RetailInputAssignmentFailure af; RetailInputRoleFailure role;
        auto p0 = x.main->participants->first_two(0, pf), p1 = x.main->participants->first_two(1, pf);
        check(x.main->input_assignments->bind_direct(p0, RetailInputRole::role2, af) &&
              x.main->input_assignments->bind_direct(p1, RetailInputRole::role3, af) &&
              x.main->input_assignments->increment_count_before_280c50_bind(af), "actual associations/ordinals/count");
        for (unsigned i = 0; i != 4; ++i)
            check(x.main->input_roles->store_source_control_word_from_source(static_cast<RetailInputRole>(i), 0x12340000 + i, role), "full role-word fixture");
        check(x.main->input_roles->store_profile_mapping_active_from_source(0, true, role) &&
              x.main->input_roles->store_profile_mapping_active_from_source(1, true, role), "same existing mapping fixtures");
        check(initial_reset_required::reset_saved_profile_children_246070(*x.main, f), "actual production246070 reset");
        check(active->card_slot == -1 && active->profile_index == -1 && active->status_word == 0 && active->player_status_word == 8 &&
              second_active->card_slot == -1 && second_active->profile_index == -1 && second_active->status_word == 0 &&
              active->value.phase() == RetailSavedChildPhase::source_defaults && second_active->value.phase() == RetailSavedChildPhase::source_defaults,
              "both actual active records reset with source metadata order");
        check(owner.card_slot(1, rf)->children[7].in_use_word == 0 && first->children[3].in_use_word == 0 &&
              first->children[2].in_use_word == 0x13579bdf, "only selected parent in-use full words released");
        check(x.main->input_roles->observe(RetailInputRole::role2, role)->source_control_word == 0 &&
              x.main->input_roles->observe(RetailInputRole::role3, role)->source_control_word == 0 &&
              x.main->input_roles->observe(RetailInputRole::role0, role)->source_control_word == 0x12340000 &&
              x.main->input_roles->profile_mapping_word(0, role) == 0 && x.main->input_roles->profile_mapping_word(1, role) == 0 &&
              x.main->input_assignments->association_count(af) == 1 && p0->observe(pf)->assigned_role == 2 && p1->observe(pf)->assigned_role == 3,
              "actual role/mapping clears preserve associations, other roles and shared count");
        check(initial_reset_required::reset_saved_profile_children_246070(*x.main, f) &&
              first->children[2].in_use_word == 0x13579bdf,
              "repeated actual reset skips minus-one assignments and preserves unrelated in-use values");
        // Explicit invalid serialized-selection fixtures exercise the original
        // signed bounds. These are not evidence of an implemented load producer.
        const std::array<std::array<std::int32_t,2>,6> invalid{{
            {-1,0},{0,-1},{-2,5},{2,0},{0,8},{INT32_MIN,INT32_MAX}}};
        for (const auto& pair : invalid) {
            active->card_slot = pair[0]; active->profile_index = pair[1];
            first->children[0].in_use_word = 0x87654321;
            check(initial_reset_required::reset_saved_profile_children_246070(*x.main, f) &&
                  first->children[0].in_use_word == 0x87654321 && first->children[2].in_use_word == 0x13579bdf &&
                  active->card_slot == -1 && active->profile_index == -1,
                  "invalid signed parent/index skips just in-use release and still performs actual active reset");
        }
    }
    {
        Fixture x(root / "storage"); auto& owner = records(*x.main);
        auto moved = std::move(*x.main->saved_profile_storage);
        check(main_binding::initialize_retail_saved_profile_records(*x.main, f) == RetailStartupGate::native_failure &&
              f.saved_profile_records.kind == RetailSavedProfileRecordsError::storage && owner.card_slot(0, rf)->status_word == 0 &&
              owner.card_slot(0, rf)->children[0].value.phase() == RetailSavedChildPhase::zero_born,
              "moved native storage fails before any profile effect, with no reopen fallback");
        x.main->saved_profile_storage = std::move(moved);
        check(main_binding::initialize_retail_saved_profile_records(*x.main, f) == RetailStartupGate::ready,
              "pre-effect native failure does not poison valid later source call");
    }
    {
        Fixture x(root / "transfer"); x.enable_menu(); menu_hook = transfer; menu_calls = 0;
        bool caught = false;
        try { (void)main_binding::initialize_retail_saved_profile_records(*x.main, f); }
        catch (const detail::NativeMainReentry& v) { caught = v.owner == x.main.get() && v.value == 91; }
        auto& owner = records(*x.main);
        check(caught && owner.card_slot(0, rf)->status_word == -1 && owner.card_slot(1, rf)->status_word == 0 &&
              owner.card_slot(0, rf)->preferences.levels == std::array<float,5>{}, "Main transfer preserves actual published source prefix");
        menu_hook = recursive; menu_calls = 0;
        check(main_binding::initialize_retail_saved_profile_records(*x.main, f) == RetailStartupGate::ready && menu_calls == 2,
              "guard released and legitimate source replay works after Main transfer"); menu_hook = nullptr;
    }
    {
        Fixture x(root / "failure"); x.enable_menu(); menu_failure = true;
        check(main_binding::initialize_retail_saved_profile_records(*x.main, f) == RetailStartupGate::native_failure &&
              f.saved_profile_records.kind == RetailSavedProfileRecordsError::aggregate, "ordinary menu failure preserves named failure");
        menu_failure = false;
        check(main_binding::initialize_retail_saved_profile_records(*x.main, f) == RetailStartupGate::native_failure &&
              f.saved_profile_records.kind == RetailSavedProfileRecordsError::prior_partial_effect, "partial native effect cannot silently replay");
    }
    {
        Fixture x(root / "allocation"); auto& owner = records(*x.main); auto* active = owner.active_player(0, rf);
        active->status_word = 73; active->player_status_word = 99; owner.card_slot(0, rf)->children[0].in_use_word = 42;
        *owner.working_control_word(rf) = 17; fail_next_allocation = true;
        const bool ok = initial_reset_required::reset_saved_profile_children_246070(*x.main, f); fail_next_allocation = false;
        check(!ok && f.saved_profile_records.kind == RetailSavedProfileRecordsError::child &&
              active->value.phase() == RetailSavedChildPhase::failed_reset && owner.card_slot(0, rf)->children[0].in_use_word == 0 &&
              active->status_word == 73 && active->player_status_word == 99 && *owner.working_control_word(rf) == 0,
              "actual child allocation failure retains exact preceding in-use/shared effects and later metadata");
    }
    {
        Fixture x(root / "thread"); auto& owner = records(*x.main); bool refused = false;
        std::thread t([&] { RetailSavedProfileRecordsFailure q; StartupFailure sf;
            refused = !owner.card_slot(0, q) && q.kind == RetailSavedProfileRecordsError::wrong_thread &&
                main_binding::initialize_retail_saved_profile_records(*x.main, sf) == RetailStartupGate::native_failure; }); t.join();
        check(refused && owner.card_slot(0, rf)->status_word == 0, "foreign thread refuses before record mutation");
    }
    {
        Fixture a(root / "identity-a"), b(root / "identity-b"); auto& owner = records(*a.main);
        check(!owner.initialize_from_171b10(*b.main, rf) && rf.kind == RetailSavedProfileRecordsError::foreign_session &&
              owner.card_slot(0, rf)->status_word == 0 && records(*b.main).card_slot(0, rf)->status_word == 0,
              "another Main Session cannot mutate this domain or its own records through the wrong owner");
    }
    {
        Fixture x(root / "revocation"); x.enable_menu(); menu_hook = revoke;
        check(main_binding::initialize_retail_saved_profile_records(*x.main, f) == RetailStartupGate::native_failure &&
              f.saved_profile_records.kind == RetailSavedProfileRecordsError::aggregate &&
              f.saved_profile_records.aggregate.kind == RetailProfileAggregateMainError::menu &&
              f.saved_profile_records.aggregate.menu.kind == RetailProfileMenuError::session && revoked_session.expired(),
              "actual reached18B0D0 refuses missing Session; retained owner survives Busy unwind then releases"); menu_hook = nullptr;
    }
    {
        Fixture x(root / "post-menu-revocation"); x.enable_menu(); menu_hook = clear_participants_then_revoke;
        check(main_binding::initialize_retail_saved_profile_records(*x.main, f) == RetailStartupGate::native_failure &&
              f.saved_profile_records.kind == RetailSavedProfileRecordsError::foreign_session && revoked_session.expired(),
              "source menu status rereads skip transitions;171B10 refuses revoked Session before preferences"); menu_hook = nullptr;
    }
    std::puts("PASS actual171B10/246070 persistent records,storage/menu ordering,input identity,prefix,transfer,thread,allocation and lifetime;loaded save import/gameplay remain required");
    return 0;
} catch (const std::exception& e) {
    fail_next_allocation = false;
    std::fprintf(stderr, "saved profile records: %s\n", e.what()); return 1;
}
