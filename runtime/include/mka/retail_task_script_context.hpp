#pragma once

#include "mka/retail_observer_list.hpp"
#include <memory>
#include <optional>
#include <thread>

namespace mka::retail {

// Native capability for the actual59FE20 game script object. Its full216990
// producer and script data consumers remain required production work.
class NativeRetailTaskScriptContext : public NativeRetailObservedObject {};

enum class RetailTaskScriptContextError { none, wrong_thread, expired_context };
struct RetailTaskScriptContextFailure {
    RetailTaskScriptContextError kind{RetailTaskScriptContextError::none};
    const char* operation{""};
};

// The Session's TaskDomain owns this nonowning game-context reference across
// registry resets. It is not a guest pointer, register or script interpreter.
class NativeRetailTaskScriptContextState final {
public:
    NativeRetailTaskScriptContextState() noexcept = default;
    NativeRetailTaskScriptContextState(const NativeRetailTaskScriptContextState&)=delete;
    NativeRetailTaskScriptContextState& operator=(const NativeRetailTaskScriptContextState&)=delete;
    bool validate_current_thread(RetailTaskScriptContextFailure& f) const noexcept {
        return access(f);
    }
    // Only the final publication effect of2168F0: caller must already have
    // completed its real typed-object match/stale-link traversal. This method
    // does not implement that traversal or the216990 allocation/attachment.
    bool publish_matched_context_from_2168f0(
        const std::shared_ptr<NativeRetailTaskScriptContext>&,
        RetailTaskScriptContextFailure&) noexcept;
    std::optional<std::shared_ptr<NativeRetailTaskScriptContext>> active_context(
        RetailTaskScriptContextFailure&) const noexcept;
    bool clear_from_2168e0(RetailTaskScriptContextFailure&) noexcept;
private:
    bool access(RetailTaskScriptContextFailure&) const noexcept;
    std::weak_ptr<NativeRetailTaskScriptContext> context_;
    bool assigned_{};
    const std::thread::id owner_thread_{std::this_thread::get_id()};
};
} // namespace mka::retail
