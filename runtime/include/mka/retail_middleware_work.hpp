#pragma once
#include <cstddef>
#include <memory>
#include <optional>
namespace mka::retail {
enum class RetailMiddlewareWorkError {none,allocation,host_capacity,native_exception,wrong_thread,closed,active,invalid_child,invalid_status,unbound_parent,foreign_parent,child_failed,release_pending,completion_uncertain,invalid_submission,foreign_submission,stale_submission};
struct RetailMiddlewareWorkFailure {RetailMiddlewareWorkError kind=RetailMiddlewareWorkError::none;};
enum class RetailMiddlewarePollStatus {source_zero=0,source_one=1,transfer_to_parent=2};
// Actual child bodies implement all staged ports; no successful defaults.
// Back-references to groups/dispatcher must be weak, avoiding ownership cycles.
enum class RetailMiddlewareReleaseResult {retained,released};
enum class RetailMiddlewareCompletionPhase {queued,primary_pending,primary_done,release_pending,failed_uncertain};
class NativeRetailMiddlewareChild {
public:
    virtual ~NativeRetailMiddlewareChild()=default;
    virtual std::optional<RetailMiddlewarePollStatus> poll(RetailMiddlewareWorkFailure&)=0;
    // Source327650's virtual completion/deferred-release effects, not automatic
    // physical IO or unconditional destruction. Primary failure is latched as
    // uncertain; it is not implicitly invoked again.
    virtual bool primary_complete(RetailMiddlewareWorkFailure&)=0;
    virtual std::optional<bool> release_required(RetailMiddlewareWorkFailure&) const=0;
    // retained explicitly means no release committed, so a later call is safe.
    // null/exception is uncertain and latched; it is never blindly replayed.
    virtual std::optional<RetailMiddlewareReleaseResult> release(RetailMiddlewareWorkFailure&)=0;
};
class NativeRetailMiddlewareDispatcher;
// Per-append occurrence identity. Holds issuer tags only, never child payload.
// Retired exact tickets report stale; another dispatcher reports foreign.
class NativeRetailMiddlewareSubmission final {
public:
    NativeRetailMiddlewareSubmission()=default;
private:
    struct State;std::shared_ptr<State> state_;
    explicit NativeRetailMiddlewareSubmission(std::shared_ptr<State>);
    friend class NativeRetailMiddlewareWorkGroup;
    friend class NativeRetailMiddlewareDispatcher;
};
struct RetailMiddlewareGroupObservation {bool enabled,polling,closed,attached;std::size_t children;};
// Pristine rollback is safe. Once configured/attached/used, explicitly drain,
// detach and shutdown; destruction with live ownership diagnoses and terminates.
class NativeRetailMiddlewareWorkGroup final {
public:
    static std::shared_ptr<NativeRetailMiddlewareWorkGroup> create(RetailMiddlewareWorkFailure&) noexcept;
    ~NativeRetailMiddlewareWorkGroup();
    bool configure(std::size_t reserve_hint,RetailMiddlewareWorkFailure&) noexcept;
    std::optional<NativeRetailMiddlewareSubmission> append(std::shared_ptr<NativeRetailMiddlewareChild>,RetailMiddlewareWorkFailure&) noexcept;
    std::optional<std::size_t> poll(RetailMiddlewareWorkFailure&) noexcept;
    RetailMiddlewareGroupObservation observe()const noexcept;
    bool shutdown(RetailMiddlewareWorkFailure&) noexcept;
private:
    struct State;std::unique_ptr<State> state_;
    explicit NativeRetailMiddlewareWorkGroup(std::unique_ptr<State>);
    friend class NativeRetailMiddlewareDispatcher;

};
struct RetailMiddlewareDispatcherObservation {bool pumping,closed;std::size_t groups,pending_completions;};
class NativeRetailMiddlewareDispatcher final {
public:
    static std::shared_ptr<NativeRetailMiddlewareDispatcher> create(RetailMiddlewareWorkFailure&) noexcept;
    ~NativeRetailMiddlewareDispatcher();
    bool attach(std::shared_ptr<NativeRetailMiddlewareWorkGroup>,RetailMiddlewareWorkFailure&) noexcept;
    bool detach(const std::shared_ptr<NativeRetailMiddlewareWorkGroup>&,RetailMiddlewareWorkFailure&) noexcept;
    std::optional<std::size_t> pump(RetailMiddlewareWorkFailure&) noexcept;
    RetailMiddlewareDispatcherObservation observe()const noexcept;
    std::optional<RetailMiddlewareCompletionPhase> completion_phase(const NativeRetailMiddlewareSubmission&,RetailMiddlewareWorkFailure&)const noexcept;
    bool shutdown(RetailMiddlewareWorkFailure&) noexcept;
private:
    struct State;std::shared_ptr<State> state_;
    explicit NativeRetailMiddlewareDispatcher(std::shared_ptr<State>);
    friend class NativeRetailMiddlewareWorkGroup;
};
} // namespace mka::retail
