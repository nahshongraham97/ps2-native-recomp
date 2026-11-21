#include "mka/retail_profile_resource_ids.hpp"
#include "mka/retail_game_session.hpp"
#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"
#include "retail_main_control.hpp"

#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>

namespace mka::retail {
namespace {
bool fail(RetailProfileResourceIdsFailure& f,
          RetailProfileResourceIdsError kind, const char* operation) noexcept {
    f={kind,operation};
    return false;
}
} // namespace

bool NativeRetailProfileResourceIds::access(
    RetailProfileResourceIdsFailure& f) const noexcept {
    if(std::this_thread::get_id()!=owner_thread_)
        return fail(f,RetailProfileResourceIdsError::wrong_thread,
                    "446F80 resource ID owner thread");
    RetailGameSessionFailure session_failure;
    if(!session_->validate_current_thread(session_failure))
        return fail(f,RetailProfileResourceIdsError::wrong_thread,
                    "446F80 retained game Session thread");
    if(after_effect_unknown_)
        return fail(f,RetailProfileResourceIdsError::after_effect_unknown,
                    "446F80 prior effectful native failure");
    f={};
    return true;
}

std::optional<RetailProfileResourceIdOutcome>
NativeRetailProfileResourceIds::record_from_446f80(
    NativeRetailMainState& main, std::uint32_t input_id,
    RetailProfileResourceIdsFailure& f) {
    if(!access(f)) return std::nullopt;
    if(!main.lifetime || !main.lifetime->active_on_owner_thread()) {
        fail(f,RetailProfileResourceIdsError::retired_main,
             "446F80 active Main lifetime and thread");
        return std::nullopt;
    }
    if(main.startup.game_session.get()!=session_) {
        fail(f,RetailProfileResourceIdsError::foreign_main,
             "446F80 Main and retained game Session identity");
        return std::nullopt;
    }
    // LBU is compared with the full word. An ID beyond 255 cannot match.
    if(input_id<0x100u &&
       std::find(active_.begin(),active_.end(),
                 static_cast<std::uint8_t>(input_id))!=active_.end()) {
        f={};
        return RetailProfileResourceIdOutcome::duplicate;
    }

    // This strong symbol must execute the real 446FF4..447118 actor search
    // and selected virtual cleanup. NativeMainReentry is the original typed
    // nonlocal control transfer and must not poison or flatten this owner.
    try {
        if(!retire_matching_retail_secret_camera_from_446f80(main,input_id,f)) {
            after_effect_unknown_=true;
            if(f.kind==RetailProfileResourceIdsError::none)
                fail(f,RetailProfileResourceIdsError::secret_camera_effect,
                     "446F80 required secret-camera cleanup");
            return std::nullopt;
        }
    } catch(const detail::NativeMainReentry&) {
        throw;
    } catch(const std::bad_alloc&) {
        after_effect_unknown_=true;
        fail(f,RetailProfileResourceIdsError::allocation,
             "446F80 secret-camera effect allocation");
        return std::nullopt;
    } catch(...) {
        after_effect_unknown_=true;
        fail(f,RetailProfileResourceIdsError::native_exception,
             "446F80 secret-camera effect exception");
        return std::nullopt;
    }
    if(after_effect_unknown_) {
        fail(f,RetailProfileResourceIdsError::after_effect_unknown,
             "446F80 nested secret-camera effect failed");
        return std::nullopt;
    }

    // The source reloads count after cleanup and does not repeat duplicate
    // detection. A nested cleanup may have appended the same ID already.
    const auto index=active_count_;
    if(input_id>=0x100u) {
        f={};
        return RetailProfileResourceIdOutcome::outside_byte_identity;
    }
    // 514240/514700 may eventually create divergent staging prefixes. No
    // native producer for their bytes is bound here, so never fabricate gaps.
    if(staging_count_!=index || active_.size()!=index || staging_.size()!=index) {
        after_effect_unknown_=true;
        fail(f,RetailProfileResourceIdsError::divergent_staging,
             "446F80 staging prefix after secret-camera effect");
        return std::nullopt;
    }
    if(index==std::numeric_limits<std::size_t>::max()) {
        after_effect_unknown_=true;
        fail(f,RetailProfileResourceIdsError::allocation,
             "446F80 native resource ID count overflow");
        return std::nullopt;
    }
    try {
        active_.reserve(index+1);
        staging_.reserve(index+1);
    } catch(const std::bad_alloc&) {
        after_effect_unknown_=true;
        fail(f,RetailProfileResourceIdsError::allocation,
             "446F80 native resource ID storage after effect");
        return std::nullopt;
    } catch(const std::length_error&) {
        after_effect_unknown_=true;
        fail(f,RetailProfileResourceIdsError::allocation,
             "446F80 native resource ID representability after effect");
        return std::nullopt;
    }
    // Reserved byte writes cannot allocate or throw. Keep the two byte stores
    // before the active and staging count publications, in original order.
    const auto id=static_cast<std::uint8_t>(input_id);
    active_.push_back(id);
    staging_.push_back(id);
    active_count_=index+1;
    staging_count_=index+1;
    f={};
    return RetailProfileResourceIdOutcome::appended;
}

std::optional<RetailProfileResourceIdsObservation>
NativeRetailProfileResourceIds::observe(
    RetailProfileResourceIdsFailure& f) const noexcept {
    if(std::this_thread::get_id()!=owner_thread_) {
        fail(f,RetailProfileResourceIdsError::wrong_thread,
             "446F80 resource ID observation thread");
        return std::nullopt;
    }
    RetailGameSessionFailure session_failure;
    if(!session_->validate_current_thread(session_failure)) {
        fail(f,RetailProfileResourceIdsError::wrong_thread,
             "446F80 retained game Session observation thread");
        return std::nullopt;
    }
    try {
        RetailProfileResourceIdsObservation result;
        result.active=active_;
        result.staging=staging_;
        result.active_count=active_count_;
        result.staging_count=staging_count_;
        result.after_effect_unknown=after_effect_unknown_;
        f={};
        return result;
    } catch(...) {
        fail(f,RetailProfileResourceIdsError::allocation,
             "446F80 native resource ID observation");
        return std::nullopt;
    }
}
} // namespace mka::retail
