#include "mka/retail_task_script_context.hpp"

namespace mka::retail {
bool NativeRetailTaskScriptContextState::access(
    RetailTaskScriptContextFailure& f) const noexcept {
    f={};
    if(std::this_thread::get_id()!=owner_thread_) {
        f={RetailTaskScriptContextError::wrong_thread,"native task script context thread"};
        return false;
    }
    return true;
}
bool NativeRetailTaskScriptContextState::publish_matched_context_from_2168f0(
    const std::shared_ptr<NativeRetailTaskScriptContext>& context,
    RetailTaskScriptContextFailure& f) noexcept {
    if(!access(f)) return false;
    context_=context;
    assigned_=static_cast<bool>(context);
    return true;
}
std::optional<std::shared_ptr<NativeRetailTaskScriptContext>>
NativeRetailTaskScriptContextState::active_context(
    RetailTaskScriptContextFailure& f) const noexcept {
    if(!access(f)) return {};
    auto context=context_.lock();
    if(assigned_ && !context) {
        f={RetailTaskScriptContextError::expired_context,"native task script context owner expired"};
        return {};
    }
    return context;
}
bool NativeRetailTaskScriptContextState::clear_from_2168e0(
    RetailTaskScriptContextFailure& f) noexcept {
    if(!access(f)) return false;
    context_.reset();
    assigned_=false;
    return true;
}
} // namespace mka::retail
