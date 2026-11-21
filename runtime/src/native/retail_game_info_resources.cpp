#include "mka/retail_game_info_resources.hpp"

#include <new>
#include <utility>

namespace mka::retail {
namespace {
bool fail(RetailGameInfoResourceFailure& f,RetailGameInfoResourceError kind,
          const char* operation) noexcept {
    f.kind=kind;
    f.operation=operation;
    return false;
}
}

NativeRetailGameInfoResource::NativeRetailGameInfoResource(
    std::shared_ptr<NativeRetailGameSession> session,
    std::shared_ptr<NativeRetailGameControls> controls) noexcept
    : session_(std::move(session)),controls_(std::move(controls)) {}

bool NativeRetailGameInfoResource::access(
    RetailGameInfoResourceFailure& f) const noexcept {
    f={};
    if(!session_ || !controls_)
        return fail(f,RetailGameInfoResourceError::missing_owner,
                    "g_game_info retained session/controls");
    if(!session_->validate_current_thread(f.session))
        return fail(f,RetailGameInfoResourceError::wrong_thread,
                    "g_game_info persistent session thread");
    return true;
}
bool NativeRetailGameInfoResource::clear_source_byte0_bit7_from_185ee0(
    RetailGameInfoResourceFailure& f) noexcept {
    if(!access(f)) return false;
    if(!controls_->clear_primary_byte0_bit7_from_185ee0(f.controls))
        return fail(f,RetailGameInfoResourceError::missing_owner,
                    "g_game_info source bit clear in actual controls");
    return true;
}
std::optional<bool> NativeRetailGameInfoResource::source_byte0_bit7(
    RetailGameInfoResourceFailure& f) const noexcept {
    if(!access(f)) return {};
    const auto value=controls_->primary_byte0_bit7(f.controls);
    if(!value)
        fail(f,RetailGameInfoResourceError::missing_owner,
             "g_game_info actual primary word read");
    return value;
}
std::shared_ptr<NativeRetailGameSession>
NativeRetailGameInfoResource::retained_session(
    RetailGameInfoResourceFailure& f) const noexcept {
    if(!access(f)) return {};
    return session_;
}
std::shared_ptr<NativeRetailGameControls>
NativeRetailGameInfoResource::retained_controls(
    RetailGameInfoResourceFailure& f) const noexcept {
    if(!access(f)) return {};
    return controls_;
}
NativeRetailInitialTaskState* NativeRetailGameInfoResource::initial_task_state(
    RetailGameInfoResourceFailure& f) const noexcept {
    if(!access(f)) return nullptr;
    auto* state=session_->initial_task_state(f.session);
    if(!state)
        fail(f,RetailGameInfoResourceError::missing_owner,
             "g_game_info actual initial-task state");
    return state;
}

NativeRetailHumanBoneTable::NativeRetailHumanBoneTable() noexcept
    : words_{{16,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0}} {}
NativeRetailFlippedHumanBoneTable::NativeRetailFlippedHumanBoneTable() noexcept
    : first_word_(16),referred_words_{{0,2,1,3,5,4,6,8,7,11,10,9,13,12,15,14}} {}
NativeRetailNpcGroundCollisions::NativeRetailNpcGroundCollisions() noexcept
    : words_{{7,0,0,0,0x3dcccccdu,8,0,0,0,0x3dcccccdu,0xffffffffu,0,0,0,0,0}} {}

std::unique_ptr<NativeRetailGameInfoResources>
NativeRetailGameInfoResources::create(
    std::shared_ptr<NativeRetailGameSession> session,
    RetailGameInfoResourceFailure& f) noexcept {
    f={};
    if(!session) {
        fail(f,RetailGameInfoResourceError::missing_session,
             "g_game_info actual persistent session");
        return {};
    }
    if(!session->validate_current_thread(f.session)) {
        fail(f,RetailGameInfoResourceError::wrong_thread,
             "g_game_info session construction thread");
        return {};
    }
    auto controls=session->controls(f.session);
    auto* initial=session->initial_task_state(f.session);
    if(!controls || !initial) {
        fail(f,RetailGameInfoResourceError::missing_owner,
             "g_game_info controls or initial-task state");
        return {};
    }
    try {
        auto result=std::unique_ptr<NativeRetailGameInfoResources>(
            new NativeRetailGameInfoResources);
        result->game_info_=std::shared_ptr<NativeRetailGameInfoResource>(
            new NativeRetailGameInfoResource(std::move(session),std::move(controls)));
        result->human_bones_=std::shared_ptr<NativeRetailHumanBoneTable>(
            new NativeRetailHumanBoneTable);
        result->flipped_human_bones_=std::shared_ptr<NativeRetailFlippedHumanBoneTable>(
            new NativeRetailFlippedHumanBoneTable);
        result->npc_ground_collisions_=std::shared_ptr<NativeRetailNpcGroundCollisions>(
            new NativeRetailNpcGroundCollisions);
        return result;
    } catch(const std::bad_alloc&) {
        fail(f,RetailGameInfoResourceError::allocation,
             "retail four typed global values");
    } catch(...) {
        fail(f,RetailGameInfoResourceError::native_exception,
             "retail global resource value construction");
    }
    return {};
}

bool NativeRetailGameInfoResources::access(
    RetailGameInfoResourceFailure& f) const noexcept {
    f={};
    if(!game_info_ || !human_bones_ || !flipped_human_bones_ ||
       !npc_ground_collisions_)
        return fail(f,RetailGameInfoResourceError::missing_owner,
                    "four typed source global values");
    const auto value=game_info_->source_byte0_bit7(f);
    return value.has_value();
}
std::shared_ptr<NativeRetailGameInfoResource>
NativeRetailGameInfoResources::game_info(
    RetailGameInfoResourceFailure& f) const noexcept {
    if(!access(f)) return {};
    return game_info_;
}
std::shared_ptr<NativeRetailHumanBoneTable>
NativeRetailGameInfoResources::human_bones(
    RetailGameInfoResourceFailure& f) const noexcept {
    if(!access(f)) return {};
    return human_bones_;
}
std::shared_ptr<NativeRetailFlippedHumanBoneTable>
NativeRetailGameInfoResources::flipped_human_bones(
    RetailGameInfoResourceFailure& f) const noexcept {
    if(!access(f)) return {};
    return flipped_human_bones_;
}
std::shared_ptr<NativeRetailNpcGroundCollisions>
NativeRetailGameInfoResources::npc_ground_collisions(
    RetailGameInfoResourceFailure& f) const noexcept {
    if(!access(f)) return {};
    return npc_ground_collisions_;
}

bool NativeRetailGameInfoResources::publish_four_after_163a50(
    NativeRetailGameResourceRegistry& registry,
    RetailGameInfoResourceFailure& f) noexcept {
    if(!access(f)) return false;
    unsigned inserted=0;
    const auto publish=[&](std::string_view name,
                           NativeRetailGameResourceValue value,
                           const char* operation) noexcept {
        if(registry.insert_global(name,std::move(value),f.registry)) {
            ++inserted;
            return true;
        }
        return fail(f,inserted ? RetailGameInfoResourceError::registry_partial :
                     RetailGameInfoResourceError::registry,operation);
    };
    if(!publish("g_game_info",NativeRetailGameResourceValue{game_info_},
                "185EE0 g_game_info publication")) return false;
    if(!publish("lores_human_bones",NativeRetailGameResourceValue{human_bones_},
                "185EE0 lores_human_bones publication")) return false;
    if(!publish("flipped_lores_human_bones",
                NativeRetailGameResourceValue{flipped_human_bones_},
                "185EE0 flipped_lores_human_bones publication")) return false;
    if(!publish("lores_npc_ground_colls",
                NativeRetailGameResourceValue{npc_ground_collisions_},
                "185EE0 lores_npc_ground_colls publication")) return false;
    f={};
    return true;
}
} // namespace mka::retail
