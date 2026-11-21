#include "mka/retail_live_scene_reference.hpp"
#include "mka/retail_game_session.hpp"
#include <utility>

namespace mka::retail {
namespace {
bool fail(RetailLiveSceneFailure& f,RetailLiveSceneError kind,
          const char* operation) noexcept {
    f={kind,operation};
    return false;
}
} // namespace

bool NativeRetailSceneReference::access(RetailLiveSceneFailure& f) const noexcept {
    if(std::this_thread::get_id()!=owner_thread_)
        return fail(f,RetailLiveSceneError::wrong_thread,
                    "2F18F0 scene reference owner thread");
    RetailGameSessionFailure session_failure;
    if(!session_->validate_current_thread(session_failure))
        return fail(f,RetailLiveSceneError::wrong_thread,
                    "2F18F0 retained game Session thread");
    f={};
    return true;
}

std::optional<std::shared_ptr<NativeRetailSceneObject>>
NativeRetailSceneReference::lookup_from_2f18f0(
    RetailLiveSceneFailure& f) const noexcept {
    if(!access(f)) return std::nullopt;
    if(!reference_published_) return std::shared_ptr<NativeRetailSceneObject>{};
    auto object=object_.lock();
    if(!object || !object->scene_identity_live() ||
       object->source_cookie()!=cached_cookie_)
        return std::shared_ptr<NativeRetailSceneObject>{};
    return object;
}

std::optional<RetailLiveScenePublishOutcome>
NativeRetailSceneReference::publish(
    std::shared_ptr<NativeRetailSceneObject> candidate,const char* operation,
    RetailLiveSceneFailure& f) noexcept {
    if(!access(f)) return std::nullopt;
    // Raw 2F1920 and its 2F1AD0 inline copy skip the apparent null-clear
    // block. Only the explicit reset routines clear the pair.
    if(!candidate) return RetailLiveScenePublishOutcome::null_no_effect;
    if(!candidate->scene_identity_live()) {
        fail(f,RetailLiveSceneError::retired_object,
             "2F1920/2F1AD0 retired or expired native scene object");
        return std::nullopt;
    }
    // Only a live candidate has a usable owner identity. In particular, do
    // not consult an expired object's former Session address.
    if(candidate->owner_session()!=session_) {
        fail(f,RetailLiveSceneError::foreign_object,operation);
        return std::nullopt;
    }
    auto old=object_.lock();
    if(reference_published_ && old && old->scene_identity_live() &&
       old->source_cookie()==cached_cookie_ && old.get()!=candidate.get())
        return RetailLiveScenePublishOutcome::refused_valid_different;

    // The source stores its object first, then captures object+4 into the
    // cached word. The weak reference is nonowning; candidate stays retained
    // by the real producer throughout this call.
    object_=candidate;
    reference_published_=true;
    cached_cookie_=candidate->source_cookie();
    f={};
    return RetailLiveScenePublishOutcome::published;
}

std::optional<RetailLiveScenePublishOutcome>
NativeRetailSceneReference::publish_from_2f1920(
    std::shared_ptr<NativeRetailSceneObject> candidate,
    RetailLiveSceneFailure& f) noexcept {
    return publish(std::move(candidate),"2F1920 foreign scene object",f);
}
std::optional<RetailLiveScenePublishOutcome>
NativeRetailSceneReference::publish_from_2f1ad0(
    std::shared_ptr<NativeRetailSceneObject> candidate,
    RetailLiveSceneFailure& f) noexcept {
    return publish(std::move(candidate),"2F1AD0 foreign scene object",f);
}

bool NativeRetailSceneReference::reset(RetailLiveSceneFailure& f) noexcept {
    if(!access(f)) return false;
    object_.reset();
    reference_published_=false;
    cached_cookie_=0;
    f={};
    return true;
}
bool NativeRetailSceneReference::reset_from_2f1750(
    RetailLiveSceneFailure& f) noexcept {
    return reset(f);
}
bool NativeRetailSceneReference::reset_from_2f1ad0(
    RetailLiveSceneFailure& f) noexcept {
    return reset(f);
}

std::optional<RetailLiveSceneObservation>
NativeRetailSceneReference::observe(RetailLiveSceneFailure& f) const noexcept {
    if(!access(f)) return std::nullopt;
    RetailLiveSceneObservation result;
    result.reference_published=reference_published_;
    result.cached_cookie=cached_cookie_;
    const auto object=object_.lock();
    result.object_retained=bool(object);
    if(object) {
        result.object_live=object->scene_identity_live();
        result.current_cookie=object->source_cookie();
        result.valid=reference_published_ && result.object_live &&
                     *result.current_cookie==cached_cookie_;
    }
    return result;
}
} // namespace mka::retail
