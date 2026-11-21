#include "mka/retail_game_records.hpp"
#include "mka/retail_game_records_required.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_active_overlay.hpp"
#include "mka/retail_game_controls.hpp"
#include "mka/retail_game_info_resources.hpp"
#include "mka/retail_game_pool.hpp"
#include "mka/retail_game_task_lists.hpp"
#include "mka/retail_initial_task_state.hpp"
#include "mka/retail_task_domain.hpp"
#include "native_retail_startup.hpp"

#include <new>

namespace mka::retail::main_binding {
namespace {
bool refuse(StartupFailure& f,RetailGameRecordsMainError kind,
            const char* operation) noexcept {
    f.kind=StartupError::game_records;
    f.operation=operation;
    f.game_records.kind=kind;
    f.game_records.operation=operation;
    return false;
}

// The original 42A0B0 calls are on the two real allocation parents. Reset
// retires their payloads while retaining parent and child owner identities.
bool reset_two_payload_parents(NativeRetailMainState& m,StartupFailure& f) noexcept {
    if(!m.startup.memory_owners || !m.startup.active_overlay)
        return refuse(f,RetailGameRecordsMainError::missing_owner,
                      "130350 Wave and overlay payload owners");
    if(!m.startup.memory_owners->reset_wave_payloads(f.game_records.heap))
        return refuse(f,RetailGameRecordsMainError::heap,
                      "130350 Wave 42A0B0 payload reset");
    auto overlay=m.startup.active_overlay->retained_companion_parent(
        f.game_records.overlay);
    if(!overlay)
        return refuse(f,RetailGameRecordsMainError::overlay,
                      "130350 actual overlay 42A0B0 parent");
    if(!overlay->reset_actual_subtree(f.game_records.overlay.heap))
        return refuse(f,RetailGameRecordsMainError::overlay,
                      "130350 overlay 42A0B0 payload reset");
    return true;
}

bool source_values(bool ok,StartupFailure& f,const char* operation) noexcept {
    return ok || refuse(f,RetailGameRecordsMainError::source_values,operation);
}
}

bool reset_retail_game_records_130350(NativeRetailMainState& m,StartupFailure& f) {
    f={};
    auto session=m.startup.game_session;
    if(!session || !m.mode_history || !m.input_roles || !m.resource_registry ||
       !m.game_info_resources || !m.startup.game_record_reset)
        return refuse(f,RetailGameRecordsMainError::missing_owner,
                      "130350 source-ordered retained game owners");
    if(!session->validate_current_thread(f.game_records.session))
        return refuse(f,RetailGameRecordsMainError::wrong_thread,
                      "130350 retained GameSession thread");
    auto* domain=session->task_domain(f.game_records.session);
    if(!domain)
        return refuse(f,RetailGameRecordsMainError::domain,
                      "130350 same-session task domain");

    // 42A0B0 twice, then 182670. The numeric plan is the only profile
    // authority; the pool reserve below consumes its published count.
    if(!reset_two_payload_parents(m,f)) return false;
    auto selector=read_retail_profile_selector(m,f);
    if(!selector) return false;
    if(!reset_and_initialize_retail_profile(
           m,select_native_class_profile(*selector),f)) return false;
    if(!m.startup.profile_main)
        return refuse(f,RetailGameRecordsMainError::profile,
                      "130350 numeric profile owner after 182670");
    auto profile=m.startup.profile_main->observe(f.game_records.profile);
    if(!profile || !profile->numeric)
        return refuse(f,RetailGameRecordsMainError::profile,
                      "130350 published numeric profile count");

    // 185EE0 publishes the same Session values and four typed globals.
    if(!m.startup.game_record_reset->run_185ee0(
           *domain,*m.mode_history,*m.input_roles,*m.game_info_resources,
           *m.resource_registry,f.game_records.reset))
        return refuse(f,RetailGameRecordsMainError::reset,
                      "130350 185EE0 game-record reset");
    // 15EB10 is an original JR/NOP leaf. 12DA70 repeats both real parent
    // resets before it births or resets the one shared game-link pool.
    if(!reset_two_payload_parents(m,f)) return false;
    const auto source_count=profile->numeric->object_counts.game_pool_records;
    if(m.startup.game_pool) {
        if(!m.startup.game_pool->reset_from_12da70(
               source_count,f.game_records.pool))
            return refuse(f,RetailGameRecordsMainError::pool,
                          "12DA70 retained game pool reset");
    } else {
        auto born=NativeRetailGamePool::initialize(
            source_count,f.game_records.pool);
        if(!born)
            return refuse(f,RetailGameRecordsMainError::pool,
                          "12DA70 source-profile game pool birth");
        m.startup.game_pool=std::move(born);
    }
    if(!domain->reset_task_roots_from_12f0d0(
           m.startup.game_pool,f.game_records.domain))
        return refuse(f,RetailGameRecordsMainError::domain,
                      "12F0D0 source task-root reset with one game pool");
    if(!session->ensure_game_task_lists(
           m.startup.game_pool,f.game_records.session) ||
       !session->ensure_initial_game_tasks(
           m.startup.game_pool,f.game_records.session))
        return refuse(f,RetailGameRecordsMainError::task_lists,
                      "130350 retained same-pool callback and task owners");
    auto lists=session->game_task_lists(f.game_records.session);
    auto tasks=session->initial_game_tasks(f.game_records.session);
    if(!lists || !tasks)
        return refuse(f,RetailGameRecordsMainError::missing_owner,
                      "130350 retained task lists and executable bodies");

    if(!lists->clear_from_228b90(f.game_records.task_lists) ||
       !lists->clear_first_pair_from_13c180(f.game_records.task_lists))
        return refuse(f,RetailGameRecordsMainError::task_lists,
                      "228B90 and 13C180 first source heads");
    if(!source_values(clear_retail_game_records_13c180_early_words(
           *session,f.game_records.values),f,"13C180 ordered scalar stores"))
        return false;
    if(!lists->clear_later_pair_from_13c180(f.game_records.task_lists))
        return refuse(f,RetailGameRecordsMainError::task_lists,
                      "13C180 later source heads");
    if(!source_values(clear_retail_game_records_13c180_late_words(
           *session,f.game_records.values),f,"13C180 final scalar stores"))
        return false;

    // Original first birth has empty source heads. The real body checks the
    // retained list on each run; a later live callback without its concrete
    // source target is refused at that producer/visit, never treated as empty.
    if(!tasks->publish_source_task(RetailInitialGameTaskBodyKind::camera_13c220,
           *domain,f.game_records.task_body))
        return refuse(f,RetailGameRecordsMainError::task_body,
                      "13C180 real camera task publication");
    if(!source_values(reset_retail_game_records_word_173570(
           *session,f.game_records.values),f,"173570 source scalar reset"))
        return false;
    if(!lists->clear_from_139a30(f.game_records.task_lists))
        return refuse(f,RetailGameRecordsMainError::task_lists,
                      "139A30 actor and callback heads");
    if(!tasks->publish_source_task(RetailInitialGameTaskBodyKind::actors_13a7a0,
           *domain,f.game_records.task_body))
        return refuse(f,RetailGameRecordsMainError::task_body,
                      "139A30 real actor task publication");
    if(!source_values(clear_retail_game_records_16c1b0_word(
           *session,f.game_records.values),f,"16C1B0 source scalar reset"))
        return false;
    if(!lists->clear_from_16c1b0(f.game_records.task_lists))
        return refuse(f,RetailGameRecordsMainError::task_lists,
                      "16C1B0 scene callback head");
    if(!tasks->publish_source_task(RetailInitialGameTaskBodyKind::scene_16c1f0,
           *domain,f.game_records.task_body))
        return refuse(f,RetailGameRecordsMainError::task_body,
                      "16C1B0 real scene task publication");
    auto per_task=tasks->publish_source_task(
        RetailInitialGameTaskBodyKind::per_task_158080,
        *domain,f.game_records.task_body);
    if(!per_task)
        return refuse(f,RetailGameRecordsMainError::task_body,
                      "1588C0 real per-task publication");
    if(!session->retain_source_task_1588c0(
           std::move(per_task),f.game_records.session))
        return refuse(f,RetailGameRecordsMainError::session,
                      "1588C0 persistent source task handle");
    if(!source_values(store_retail_game_records_bytes_127370(
           *session,f.game_records.values),f,"127370 source bytes"))
        return false;
    if(!install_retail_text_allocator_pair_16fd80(m,f)) return false;
    return true;
}

bool initialize_retail_game_records(NativeRetailMainState& m,StartupFailure& f) {
    f={};
    auto session=m.startup.game_session;
    if(!session)
        return refuse(f,RetailGameRecordsMainError::missing_owner,
                      "1303F0 persistent GameSession");
    if(!session->validate_current_thread(f.game_records.session))
        return refuse(f,RetailGameRecordsMainError::wrong_thread,
                      "1303F0 persistent GameSession thread");
    if(!apply_retail_game_records_prefix_1303f0(*session,f.game_records.values))
        return refuse(f,RetailGameRecordsMainError::source_values,
                      "1303F0 source PRNG and scalar prefix");
    if(!m.input_roles)
        m.input_roles=NativeRetailInputRoles::create(f.game_records.roles);
    if(!m.input_roles ||
       !m.input_roles->initialize_roles_from_281e00(f.game_records.roles))
        return refuse(f,RetailGameRecordsMainError::roles,
                      "281E00 four real input-role defaults");
    if(!clear_retail_game_records_pair_1303f0(*session,f.game_records.values))
        return refuse(f,RetailGameRecordsMainError::source_values,
                      "1303F0 pair before 2173D0");
    if(m.resource_registry)
        return refuse(f,RetailGameRecordsMainError::registry,
                      "2173D0 repeated registry birth requires prior retirement");
    m.resource_registry=NativeRetailGameResourceRegistry::create(
        f.game_records.registry);
    if(!m.resource_registry)
        return refuse(f,RetailGameRecordsMainError::registry,
                      "2173D0 actual resource registry birth");
    auto controls=session->controls(f.game_records.session);
    if(!controls || !controls->clear_selection_state_from_1303f0(
           f.game_records.controls))
        return refuse(f,RetailGameRecordsMainError::controls,
                      "1303F0 source selection state word reset");
    auto* initial=session->initial_task_state(f.game_records.session);
    if(!initial || !initial->reset_menu_index_and_advance_cycle_from_18d430(
           f.game_records.initial_task))
        return refuse(f,RetailGameRecordsMainError::initial_task,
                      "18D430 same-session menu index and cycle");
    if(!clear_retail_mode_stack(m,f) || !enter_retail_mode_zero(m,f))
        return false;
    if(!m.game_info_resources) {
        m.game_info_resources=NativeRetailGameInfoResources::create(
            session,f.game_records.global_values);
        if(!m.game_info_resources)
            return refuse(f,RetailGameRecordsMainError::missing_owner,
                          "185EE0 four typed game-info values");
    }
    if(!reset_retail_game_records_130350(m,f)) return false;
    if(!m.input_roles->reset_first_two_mapping_tables_from_281870(
           f.game_records.roles))
        return refuse(f,RetailGameRecordsMainError::roles,
                      "281870 first two physical mapping pairs");
    if(m.participants)
        return refuse(f,RetailGameRecordsMainError::participants,
                      "137810 repeated participant birth requires detachment");
    m.participants=NativeRetailParticipants::create_first_two(
        f.game_records.participants);
    if(!m.participants)
        return refuse(f,RetailGameRecordsMainError::participants,
                      "1303F0 two real 137810 participant births");
    if(!reset_retail_game_records_tail_163fd0(*session,f.game_records.values))
        return refuse(f,RetailGameRecordsMainError::source_values,
                      "163FD0 final source scalar stores");
    return true;
}
} // namespace mka::retail::main_binding
