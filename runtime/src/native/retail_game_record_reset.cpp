#include "mka/retail_game_record_reset.hpp"
#include "mka/retail_pad_control.hpp"
#include "retail_main_control.hpp"

#include <new>
#include <utility>

namespace mka::retail {
namespace {
bool fail(RetailGameRecordResetFailure& f,RetailGameRecordResetError kind,
          const char* operation) noexcept {
    f.kind=kind;
    f.operation=operation;
    return false;
}
}

std::unique_ptr<NativeRetailGameRecordReset>
NativeRetailGameRecordReset::create(
    std::shared_ptr<NativeRetailGameSession> session,
    RetailGameRecordResetFailure& f) noexcept {
    f={};
    if(!session) {
        fail(f,RetailGameRecordResetError::missing_owner,
             "185EE0 persistent native game session");
        return {};
    }
    if(!session->validate_current_thread(f.session)) {
        fail(f,RetailGameRecordResetError::wrong_thread,
             "185EE0 source owner thread");
        return {};
    }
    try {
        return std::unique_ptr<NativeRetailGameRecordReset>(
            new NativeRetailGameRecordReset(std::move(session)));
    } catch(const std::bad_alloc&) {
        fail(f,RetailGameRecordResetError::allocation,
             "185EE0 persistent reset owner allocation");
    } catch(...) {
        fail(f,RetailGameRecordResetError::native_exception,
             "185EE0 persistent reset owner construction");
    }
    return {};
}

bool NativeRetailGameRecordReset::access(
    RetailGameRecordResetFailure& f) const noexcept {
    f={};
    if(!session_)
        return fail(f,RetailGameRecordResetError::missing_owner,
                    "185EE0 retained GameSession missing");
    if(!session_->validate_current_thread(f.session))
        return fail(f,RetailGameRecordResetError::wrong_thread,
                    "185EE0 retained GameSession thread");
    return true;
}

void NativeRetailGameRecordReset::reset_152880(
    RetailGameRecordResetSnapshot& state) noexcept {
    state.word_152880_88=0;
    state.word_152880_8c=0;
    state.word_152880_90=0;
    state.word_152880_9c=0;
    state.word_152880_ac=0;
    state.word_152880_b0=0;
    state.word_152880_f8=0;
    state.word_152880_128=0;
    state.word_152880_124=0;
    state.word_152880_a0=0; // Original JR RA delay-slot store.
}

void NativeRetailGameRecordReset::reset_direct_185ee0(
    RetailGameRecordResetSnapshot& state) noexcept {
    state.word_185ee0_e308=0;
    state.word_185ee0_e30c=0;
    state.word_185ee0_e300=0;
    state.word_185ee0_e304=0;
    state.word_185ee0_e2f8=0;
    state.word_185ee0_e2fc=0;
    state.word_185ee0_e2f0=0;
    state.word_185ee0_e2f4=0;
    state.word_185ee0_ed80=0;
    state.word_185ee0_ed84=0;
    state.word_185ee0_ed78=0;
    state.word_185ee0_ed7c=0;
    state.word_185ee0_ecf4=0;
    state.word_185ee0_eaa4=0;
    state.word_185ee0_f2b4=0;
    state.word_185ee0_e120=0;
    state.word_185ee0_e11c=0;
    state.word_185ee0_e118=0;
    state.word_185ee0_c3c8=0; // File-backed birth was 1, not BSS zero.
    state.word_27a6a0_e9c8=0; // Source JR RA delay-slot store.
}

bool NativeRetailGameRecordReset::reset_163a50(
    NativeRetailTaskDomain& domain,NativeRetailInputRoles& roles,
    RetailGameRecordResetFailure& f) {
    return resume_retail_input_from_163a50(*session_,domain,roles,f);
}

bool NativeRetailGameRecordReset::run_185ee0(
    NativeRetailTaskDomain& domain,NativeRetailModeState& mode,
    NativeRetailInputRoles& roles,NativeRetailGameInfoResources& values,
    NativeRetailGameResourceRegistry& registry,
    RetailGameRecordResetFailure& f) {
    if(!access(f)) return false;
    auto* session_values=session_->game_record_values(f.session);
    if(!session_values)
        return fail(f,RetailGameRecordResetError::missing_owner,
                    "185EE0 persistent GameSession record values");
    auto& state=*session_values;
    if(state.partial_reset)
        return fail(f,RetailGameRecordResetError::partial_reset,
                    "previous 185EE0 reset stopped after source effects");
    auto actual_game=values.game_info(f.values);
    if(!actual_game)
        return fail(f,RetailGameRecordResetError::global_values,
                    "185EE0 four-value owner unavailable");
    const auto retained=actual_game->retained_session(f.values);
    if(retained!=session_)
        return fail(f,RetailGameRecordResetError::foreign_session,
                    "185EE0 same persistent GameSession identity");
    auto* session_domain=session_->task_domain(f.session);
    if(!session_domain)
        return fail(f,RetailGameRecordResetError::missing_owner,
                    "185EE0 persistent GameSession task domain");
    if(session_domain!=&domain)
        return fail(f,RetailGameRecordResetError::foreign_domain,
                    "185EE0 same persistent task domain identity");
    if(!domain.observe(f.task_domain))
        return fail(f,RetailGameRecordResetError::task_domain,
                    "185EE0 actual task domain thread/state");
    if(!registry.observe(f.resource_registry))
        return fail(f,RetailGameRecordResetError::registry,
                    "185EE0 actual global resource registry thread/state");
    auto controls=session_->controls(f.session);
    if(!controls)
        return fail(f,RetailGameRecordResetError::missing_owner,
                    "185EE0 actual primary game-info controls");

    // All preflight is before the first source store. Thereafter every
    // failure reports a retained partial state; no reset effect is undone.
    if(!domain.seed_counter_from_185ee0(f.task_domain))
        return fail(f,RetailGameRecordResetError::task_domain,
                    "185EE0 conditional cookie-counter seed");
    state.word_185ee0_7b7348=0;
    reset_152880(state);
    state.word_185ee0_e070=0;
    if(!controls->reset_primary_game_info_word_from_16a9d0(f.controls)) {
        state.partial_reset=true;
        return fail(f,RetailGameRecordResetError::controls,
                    "16A9D0 complete primary-word reset");
    }
    state.word_16a9d0_2d4=2;
    state.word_185ee0_dfc0=0;
    state.word_185ee0_dfbc=0;
    state.word_185ee0_dfb8=0;
    state.word_185ee0_dfb4=0;
    state.word_185ee0_dfb0=0;
    state.word_185ee0_df80=0;
    if(!actual_game->clear_source_byte0_bit7_from_185ee0(f.values)) {
        state.partial_reset=true;
        return fail(f,RetailGameRecordResetError::global_values,
                    "185EE0 primary byte0 bit7 clear");
    }
    reset_direct_185ee0(state);
    const auto retained_top=mode.top();
    mode.clear();
    if(!mode.push(retained_top,f.mode)) {
        state.partial_reset=true;
        return fail(f,RetailGameRecordResetError::mode,
                    "185EE0 retained top-mode reset/push");
    }
    try {
        if(!reset_163a50(domain,roles,f)) {
            state.partial_reset=true;
            return false;
        }
    } catch(const detail::NativeMainReentry&) {
        throw; // The original nonlocal Main transfer remains control flow.
    } catch(const std::bad_alloc&) {
        state.partial_reset=true;
        return fail(f,RetailGameRecordResetError::allocation,
                    "163A50 prior-task lookup after effect");
    } catch(...) {
        state.partial_reset=true;
        return fail(f,RetailGameRecordResetError::native_exception,
                    "163A50 prior-task lookup after effect");
    }
    if(!values.publish_four_after_163a50(registry,f.values)) {
        state.partial_reset=true;
        return fail(f,RetailGameRecordResetError::global_values,
                    "185EE0 four typed global registrations after effect");
    }
    f={};
    return true;
}

std::optional<RetailGameRecordResetSnapshot>
NativeRetailGameRecordReset::observe(
    RetailGameRecordResetFailure& f) const noexcept {
    if(!access(f)) return {};
    auto* session_values=session_->game_record_values(f.session);
    if(!session_values) {
        fail(f,RetailGameRecordResetError::missing_owner,
             "185EE0 persistent GameSession record values");
        return {};
    }
    return *session_values;
}
} // namespace mka::retail
