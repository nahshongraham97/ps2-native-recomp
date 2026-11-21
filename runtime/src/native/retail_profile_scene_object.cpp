#include "mka/retail_profile_scene_object.hpp"
#include "mka/retail_game_session.hpp"
#include <new>
#include <stdexcept>
#include <utility>

namespace mka::retail {
namespace {
bool fail(RetailProfileSceneObjectFailure& f,RetailProfileSceneObjectError kind,
    const char* operation) noexcept { f={kind,operation}; return false; }
}
NativeRetailProfileSceneObject::NativeRetailProfileSceneObject(
    std::shared_ptr<NativeRetailGameSession> session,std::uint32_t cookie,
    const RetailProfileSceneValues& initial) noexcept
    : session_identity_(session.get()),session_(session),cookie_(cookie),values_(initial) {}

std::shared_ptr<NativeRetailProfileSceneObject> NativeRetailProfileSceneObject::create(
    std::shared_ptr<NativeRetailGameSession> session,std::uint32_t actual_cookie,
    const RetailProfileSceneValues& initial,RetailProfileSceneObjectFailure& f) noexcept {
    f={};
    RetailGameSessionFailure sf;
    if(!session) {
        fail(f,RetailProfileSceneObjectError::missing_session,"native profile scene Session missing");
        return {};
    }
    if(!session->validate_current_thread(sf)) {
        fail(f,RetailProfileSceneObjectError::wrong_thread,
            "native profile scene actual Session/lifetime/thread");
        return {};
    }
    try {
        return std::shared_ptr<NativeRetailProfileSceneObject>(
            new NativeRetailProfileSceneObject(std::move(session),actual_cookie,initial));
    } catch(const std::bad_alloc&) {
        fail(f,RetailProfileSceneObjectError::allocation,"native profile scene field owner allocation");
    } catch(...) {
        fail(f,RetailProfileSceneObjectError::native_exception,"native profile scene field owner construction");
    }
    return {};
}
std::shared_ptr<NativeRetailGameSession> NativeRetailProfileSceneObject::lock_owner(
    RetailProfileSceneObjectFailure& f) const noexcept {
    f={};
    if(std::this_thread::get_id()!=thread_) {
        fail(f,RetailProfileSceneObjectError::wrong_thread,"profile scene owner thread");
        return {};
    }
    auto session=session_.lock();
    if(!session) {
        fail(f,RetailProfileSceneObjectError::expired_session,"profile scene Session lifetime ended");
        return {};
    }
    RetailGameSessionFailure sf;
    if(!session->validate_current_thread(sf)) {
        fail(f,RetailProfileSceneObjectError::wrong_thread,"profile scene Session thread");
        return {};
    }
    return session;
}
const NativeRetailGameSession* NativeRetailProfileSceneObject::owner_session() const noexcept {
    return session_identity_;
}
bool NativeRetailProfileSceneObject::scene_identity_live() const noexcept {
    return !retired_ && !session_.expired();
}
std::uint32_t NativeRetailProfileSceneObject::source_cookie() const noexcept {
    return cookie_.load(std::memory_order_acquire);
}
bool NativeRetailProfileSceneObject::retire_from_12df60(RetailProfileSceneObjectFailure& f) noexcept {
    f={};
    if(std::this_thread::get_id()!=thread_)
        return fail(f,RetailProfileSceneObjectError::wrong_thread,"12DF60 menu object owner thread");
    if(retired_) return fail(f,RetailProfileSceneObjectError::retired,"12DF60 menu object already retired");
    // Original12DF60 zeros cookie in the release call's delay slot. Logical
    // retirement precedes native reference release; no guest pool is retained.
    cookie_.store(0,std::memory_order_release);
    retired_=true;
    return true;
}
void NativeRetailProfileSceneObject::cleanup_from_12fa00() {
    RetailProfileSceneObjectFailure f;
    if(!retire_from_12df60(f)) throw std::logic_error(f.operation);
}
bool NativeRetailProfileSceneObject::stage_from_18a260(std::uint32_t role,
    bool source_byte14_bit0,RetailProfileSceneObjectFailure& f) noexcept {
    const auto session=lock_owner(f);
    if(!session) return false;
    if(retired_) return fail(f,RetailProfileSceneObjectError::retired,"18A260 retired menu object fields");
    if(role>=values_.lanes.size())
        return fail(f,RetailProfileSceneObjectError::invalid_role,"18A260 source role outside0/1");
    values_.lanes[role].word_2c=source_byte14_bit0?2u:1u;
    values_.lanes[role].word_24=0;
    // LW/ADDIU(-1)/SW is a wrapped32-bit game counter, not host capacity.
    values_.word_60-=std::uint32_t{1};
    return true;
}
std::optional<RetailProfileSceneObjectObservation> NativeRetailProfileSceneObject::observe(
    RetailProfileSceneObjectFailure& f) const noexcept {
    const auto session=lock_owner(f);
    if(!session) return std::nullopt;
    return RetailProfileSceneObjectObservation{values_,source_cookie(),retired_};
}
} // namespace mka::retail
