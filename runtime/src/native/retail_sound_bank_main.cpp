#include "mka/retail_main.hpp"
#include "mka/retail_current_sound_required.hpp"
#include "mka/retail_initial_reset_required.hpp"
#include "mka/retail_sound_bank_catalog.hpp"
#include "mka/retail_sound_runtime.hpp"
#include "native_retail_startup.hpp"
#include <new>
#include <algorithm>
#include <stdexcept>

namespace mka::retail {
namespace {
bool owner(NativeRetailMainState& main, StartupFailure& failure, bool require_ready) {
    failure = {};
    if (!main.lifetime || !main.lifetime->active_on_owner_thread()) {
        failure.kind = StartupError::sound_catalog;
        failure.sound_catalog.kind = RetailSoundCatalogError::wrong_owner;
        failure.operation = "native current-sound Main lifetime/thread";
        return false;
    }
    if (require_ready && (main.current_sound_phase != RetailCurrentSoundPhase::ready ||
            !main.sound_bank_catalog || !main.playback || !main.current_sound)) {
        failure.kind = StartupError::sound_catalog;
        failure.operation = "native current-sound bank/playback owner is not initialized";
        return false;
    }
    return true;
}
bool catalog_result(bool ok, StartupFailure& failure) {
    if (!ok) {
        failure.kind = StartupError::sound_catalog;
        failure.operation = failure.sound_catalog.operation;
    }
    return ok;
}
bool required_result(bool ok, StartupFailure& failure, const char* operation) {
    if (!ok && failure.kind == StartupError::none) {
        failure.kind = StartupError::sound_catalog;
        failure.operation = operation;
    }
    return ok;
}
}

namespace current_sound_required {
bool prepare_game_sound_state_12d7e0(NativeRetailMainState& main, StartupFailure& failure) {
    if (!owner(main, failure, false)) return false;
    // This prefix runs once before316320. Existing native references must
    // never be discarded by translating the original memset literally.
    if (main.current_sound_phase != RetailCurrentSoundPhase::empty ||
        main.game_sound_state_prepared || !main.game_sound_references.empty() ||
        main.sound_bank_catalog || main.playback || main.current_sound || main.sound_runtime || main.sound_status) {
        failure.kind = StartupError::sound_catalog;
        failure.operation = "12D7E0 game sound-state preparation requires a fresh owner";
        return false;
    }
    main.game_sound_references.clear();
    main.sound_auxiliary_word = 0;
    main.game_sound_state_prepared = true;
    return true;
}
}

namespace main_binding {
bool initialize_retail_current_sound(NativeRetailMainState& main, StartupFailure& failure) {
    if (!owner(main, failure, false)) return false;
    if (main.current_sound_phase == RetailCurrentSoundPhase::ready ||
        main.current_sound_phase == RetailCurrentSoundPhase::failed) {
        failure.kind = StartupError::already_initialized;
        failure.operation = "12D7E0 current-sound owner already initialized or failed";
        return false;
    }
    if (!main.file_paths) {
        failure.kind = StartupError::sound_catalog;
        failure.operation = "12D7E0 real native file service missing";
        return false;
    }
    if (main.current_sound_phase == RetailCurrentSoundPhase::empty) {
        // Actual native game-reference/auxiliary clear precedes the archive
        // and still-required playback/context factory, matching12D7E0.
        if (!required_result(current_sound_required::prepare_game_sound_state_12d7e0(main, failure),
                failure, "12D7E0 required game sound-state preparation")) {
            main.current_sound_phase = RetailCurrentSoundPhase::failed;
            return false;
        }
        main.current_sound_phase = RetailCurrentSoundPhase::game_state_prepared;
    }
    if (main.current_sound_phase == RetailCurrentSoundPhase::game_state_prepared) {
        auto catalog = NativeRetailSoundBankCatalog::open(main.startup.content, *main.file_paths,
            main.lifetime, failure.sound_catalog);
        if (!catalog_result(bool(catalog), failure)) {
            main.current_sound_phase = RetailCurrentSoundPhase::failed;
            return false;
        }
        main.sound_bank_catalog = std::move(catalog);
        main.current_sound_phase = RetailCurrentSoundPhase::bank_catalog_opened;
    }
    auto native_session = main.sound_bank_catalog->share_bank_session(failure.sound_catalog);
    if (!catalog_result(bool(native_session), failure)) {
        main.current_sound_phase = RetailCurrentSoundPhase::failed;
        return false;
    }
    if (!required_result(current_sound_required::create_playback_context_316320(
            main, std::move(*native_session), failure), failure, "316320 required native playback/context creation") ||
        !main.playback || !main.current_sound) {
        main.current_sound_phase = RetailCurrentSoundPhase::failed;
        if (failure.kind == StartupError::none) {
            failure.kind = StartupError::sound_catalog;
            failure.operation = "316320 required child did not publish actual playback/context owners";
        }
        return false;
    }
    // A required consumer may not close the shared backing and then publish a
    // nonnull context as ready. Validate the actual canonical owner again.
    if (!main.sound_bank_catalog->share_bank_session(failure.sound_catalog)) {
        main.current_sound_phase = RetailCurrentSoundPhase::failed;
        return catalog_result(false, failure);
    }
    main.current_sound_phase = RetailCurrentSoundPhase::ready;
    return true;
}

std::optional<sound::NativePlayHandle> submit_retail_queued_sound(NativeRetailMainState& main,
    const sound::AudioBankHandle& bank,std::int32_t selector,std::uint32_t semantic_sound_id,
    const sound::NativeActivationParameters& parameters,StartupFailure& failure){
    if(!owner(main,failure,true))return {};
    if(!main.sound_runtime||main.sound_runtime->playback_owner()!=main.playback||
       !main.sound_runtime->sound_context().same_identity(*main.current_sound)){
        failure.kind=StartupError::sound_catalog;failure.operation="queued game sound requires actual current-sound runtime";return {};
    }
    try {
        // Reserve the real game record before exposing any pending identity.
        if(main.game_sound_references.size()==main.game_sound_references.max_size()){
            failure.kind=StartupError::allocation;failure.operation="native game sound reference capacity";return {};
        }
        main.game_sound_references.reserve(main.game_sound_references.size()+1);
        auto play=main.sound_runtime->submit_queued_play(bank,selector,parameters,failure.sound_catalog.runtime);
        if(!play){failure.kind=StartupError::sound_catalog;failure.sound_catalog.kind=RetailSoundCatalogError::runtime;
            failure.operation=failure.sound_catalog.runtime.operation;return {};}
        main.game_sound_references.push_back({*play,bank,semantic_sound_id});return play;
    }catch(const std::bad_alloc&){failure.kind=StartupError::allocation;}
     catch(const std::length_error&){failure.kind=StartupError::allocation;}
    failure.operation="native queued game sound reference admission";return {};
}
bool retire_retail_game_sound_references(NativeRetailMainState& main, StartupFailure& failure) {
    if(!owner(main,failure,false))return false;
    if(main.game_sound_references.empty())return true;
    if(!owner(main,failure,true))return false;
    if(!main.sound_runtime||main.sound_runtime->playback_owner()!=main.playback||
       !main.sound_runtime->sound_context().same_identity(*main.current_sound)){
        failure.kind=StartupError::sound_catalog;
        failure.operation="12C1D0 references require the actual native current-sound owner";return false;
    }
    const auto fail_playback=[&](sound::PlaybackFailure value,const char* operation){
        failure.kind=value.kind==sound::PlaybackError::allocation_failed?StartupError::allocation:StartupError::sound_catalog;
        failure.sound_catalog.kind=RetailSoundCatalogError::runtime;
        failure.sound_catalog.runtime.kind=RetailSoundRuntimeError::playback;
        failure.sound_catalog.runtime.playback=std::move(value);
        failure.operation=operation;return false;
    };
    while(!main.game_sound_references.empty()){
        const auto& handle=main.game_sound_references.back().playback;
        if(!handle.empty()){
            sound::PlaybackFailure playback_failure;
            auto present=main.playback->observe(handle,playback_failure);
            if(!present&&playback_failure.kind==sound::PlaybackError::operation_active){
                RetailSoundRuntimeFailure cancellation_failure;
                auto cancellation=main.sound_runtime->cancel_pending_play(handle,cancellation_failure);
                if(cancellation&&*cancellation!=sound::ProgramWorkCancellation::not_cancellable){
                    // An accepted running request removes the game record only.
                    // The actual resource job keeps the association until its
                    // callback/suppression and native retirement really finish.
                    main.game_sound_references.pop_back();continue;
                }
                present=main.playback->observe(handle,playback_failure);
            }
            if(!present){
                if(playback_failure.kind!=sound::PlaybackError::retired_handle)
                    return fail_playback(std::move(playback_failure),"12C1D0 actual native play lookup");
            }else {
                auto retired=main.playback->force_retire(handle,playback_failure);
                if(!retired)return fail_playback(std::move(playback_failure),"12C1D0 actual native forced play retirement");
                if(!retired->native_resources_released){
                    playback_failure.kind=sound::PlaybackError::operation_active;
                    return fail_playback(std::move(playback_failure),"12C1D0 native private resources still retiring");
                }
            }
        }
        //12C1D0 removes the last record. Its stored bank/semantic identifier
        // are not unload requests. Real releasing PCM remains in Owner's pool.
        main.game_sound_references.pop_back();
    }
    return true;
}
bool resume_retail_game_sound_reference(NativeRetailMainState& main,std::uint32_t identifier,StartupFailure& failure){
    if(!owner(main,failure,false))return false;
    auto found=std::find_if(main.game_sound_references.begin(),main.game_sound_references.end(),
        [identifier](const auto& reference){return reference.sound_identifier==identifier;});
    if(found==main.game_sound_references.end())return true;
    if(!owner(main,failure,true))return false;
    if(!main.sound_runtime||main.sound_runtime->playback_owner()!=main.playback||
       !main.sound_runtime->sound_context().same_identity(*main.current_sound)){
        failure.kind=StartupError::sound_catalog;failure.operation="12C300 requires actual native current-sound runtime";return false;
    }
    if(!found->playback.empty()&&!main.sound_runtime->request_resume(found->playback,failure.sound_catalog.runtime)){
        failure.kind=StartupError::sound_catalog;failure.sound_catalog.kind=RetailSoundCatalogError::runtime;
        failure.operation=failure.sound_catalog.runtime.operation;return false;
    }
    //12C300 removes only the first matching semantic identifier, preserving
    // the suffix order. Native resources remain owned by playback/work.
    main.game_sound_references.erase(found);return true;
}
bool pump_retail_current_sound(NativeRetailMainState& main, StartupFailure& failure) {
    if (!owner(main, failure, true)) return false;
    // Original3152C0 increments64ECA8 even when transport admission or
    // reservation refuses. Native failures retain their diagnostic, but an
    // attempted frame still counts. Invalid Main admission above is separate.
    const auto finish_attempt = [&](bool result) {
        main.sound_frame_counter += std::uint32_t{1}; // ADDIU/SW word wrap.
        return result;
    };
    // An unwinding game control transfer never reaches the source increment.
    // Keep this explicit instead of incrementing from an unwinding destructor.
    const auto retained = main.sound_bank_catalog;
    // Returned bank callbacks precede the source command99 status request.
    // Program dispatch belongs to the separate MSL timed worker, not99.
    // Typed status publication retains the historical effect name; it neither
    // advances programs nor impersonates public-reference retirement events.
    if (!catalog_result(retained->pump_one(failure.sound_catalog), failure)) return finish_attempt(false);
    if (main.sound_runtime && !main.sound_runtime->observe(failure.sound_catalog.runtime)) {
        failure.kind = StartupError::sound_catalog;
        failure.sound_catalog.kind = RetailSoundCatalogError::runtime;
        failure.operation = failure.sound_catalog.runtime.operation;
        return finish_attempt(false);
    }
    if(main.sound_runtime){
        auto completed=main.sound_runtime->poll_play_completion(failure.sound_catalog.runtime);
        if(failure.sound_catalog.runtime.kind!=RetailSoundRuntimeError::none||
           (completed&&(completed->result.failure||completed->resume_failure))){
            if(completed&&(completed->result.failure||completed->resume_failure)){failure.sound_catalog.runtime.kind=RetailSoundRuntimeError::playback;
                failure.sound_catalog.runtime.playback=completed->resume_failure?*completed->resume_failure:*completed->result.failure;
                failure.sound_catalog.runtime.operation="native queued play ordinary consumer failed";}
            failure.kind=StartupError::sound_catalog;failure.sound_catalog.kind=RetailSoundCatalogError::runtime;
            failure.operation=failure.sound_catalog.runtime.operation;return finish_attempt(false);
        }
    }
    if (!required_result(current_sound_required::update_programs_after_bank_results_3152c0(main, failure),
            failure, "3152C0 required native sound status publication")) return finish_attempt(false);
    if (main.sound_bank_catalog != retained || !main.lifetime->active_on_owner_thread()) {
        failure.kind = StartupError::sound_catalog;
        failure.operation = "3152C0 current-sound owner changed during native completion";
        return finish_attempt(false);
    }
    return finish_attempt(true);
}
}

namespace initial_reset_required {
bool request_mode_one_banks_12a4f0(NativeRetailMainState& main, StartupFailure& failure) {
    if (!owner(main, failure, true)) return false;
    return catalog_result(main.sound_bank_catalog->select_mode_one(failure.sound_catalog), failure);
}
std::optional<bool> banks_pending_12abd0(NativeRetailMainState& main, StartupFailure& failure) {
    if (!owner(main, failure, true)) return {};
    const auto pending = main.sound_bank_catalog->pending(failure.sound_catalog);
    if (!pending) catalog_result(false, failure);
    return pending;
}
}
} // namespace mka::retail
