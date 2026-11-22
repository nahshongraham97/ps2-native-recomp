#include "mka/retail_current_sound_required.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_sound_bank_catalog.hpp"
#include "mka/retail_sound_runtime.hpp"
#include "native_retail_startup.hpp"
namespace mka::retail::current_sound_required {
bool create_playback_context_316320(NativeRetailMainState& main,sound::AudioBankSession&& session,StartupFailure& f){
    f={};
    if(!main.lifetime||!main.lifetime->active_on_owner_thread()){
        f.kind=StartupError::sound_catalog;f.sound_catalog.kind=RetailSoundCatalogError::wrong_owner;
        f.operation="316320 native context requires its Main owner thread";return false;
    }
    if(main.current_sound_phase!=RetailCurrentSoundPhase::bank_catalog_opened||!main.game_sound_state_prepared||
       !main.sound_bank_catalog||main.playback||main.current_sound||main.sound_runtime||main.sound_status){
        f.kind=StartupError::sound_catalog;f.operation="316320 native context requires the fresh opened catalog";return false;
    }
    auto runtime=NativeRetailSoundRuntime::create(std::move(session),f.sound_catalog.runtime);
    if(!runtime){
        f.kind=f.sound_catalog.runtime.kind==RetailSoundRuntimeError::allocation?StartupError::allocation:StartupError::sound_catalog;
        f.sound_catalog.kind=RetailSoundCatalogError::runtime;f.operation=f.sound_catalog.runtime.operation;return false;
    }
    main.playback=runtime->playback_owner();
    main.current_sound=runtime->sound_context();
    main.sound_runtime=std::move(runtime);
    return true;
}
}
