#pragma once

#include "mka/retail_game_session.hpp"
#include "mka/retail_initial_task_required.hpp"
#include "mka/retail_main_lifetime.hpp"
#include "mka/retail_task_registry.hpp"

#include <memory>
#include <stdexcept>

namespace mka::retail {
struct NativeRetailMainState;
class NativeRetailInputParticipant;

enum class RetailInitialTaskError {
    none, missing_session, foreign_session, wrong_thread, retired_main, missing_source_owner,
    invalid_menu_index, missing_child, invalid_child_step, after_effect_unknown,
    allocation, native_exception
};
struct RetailInitialTaskFailure {
    RetailInitialTaskError kind{RetailInitialTaskError::none};
    const char* operation{""};
};
class RetailInitialTaskException final : public std::runtime_error {
public:
    RetailInitialTaskException(RetailInitialTaskError kind,const char* operation);
    RetailInitialTaskError kind() const noexcept { return kind_; }
private:
    RetailInitialTaskError kind_;
};
class NativeRetailMenuContinuation;

// Original 18E100 body. The real Main must outlive this task and all retained
// child procedures. The retained session must be Main's actual startup
// session; no separate stand-in source-word owner is accepted.
class NativeRetailInitialTaskBody final : public NativeRetailTaskBody,
    public std::enable_shared_from_this<NativeRetailInitialTaskBody> {
public:
    static std::shared_ptr<NativeRetailInitialTaskBody> create(
        NativeRetailMainState&,std::shared_ptr<NativeRetailGameSession>,
        RetailInitialTaskFailure&) noexcept;
    NativeTaskStep run() override;
    void cleanup(bool release_allowed) override;
private:
    enum class Phase {
        begin_reset, advance_reset, start_words, choose_first_run, begin_intro, advance_intro,
        begin_prompt, advance_prompt, mark_first_run, transition_three,
        yield_menu, yielded
    };
    NativeRetailInitialTaskBody(NativeRetailMainState&,
        std::shared_ptr<NativeRetailGameSession>) noexcept;
    NativeRetailInitialTaskState& checked_state() const;
    NativeRetailMainState& main_;
    std::weak_ptr<NativeRetailMainLifetime> main_lifetime_;
    std::weak_ptr<NativeRetailGameSession> session_;
    std::shared_ptr<NativeRetailInitialChild> child_;
    std::shared_ptr<NativeRetailMenuContinuation> menu_;
    Phase phase_{Phase::begin_reset};
    bool after_effect_unknown_{};
};

// Original 18DB60 continuation. It retains an indexed child across any
// source +14 wait and publishes the next table index before invoking it.
class NativeRetailMenuContinuation final : public NativeRetailTaskContinuation,
    public std::enable_shared_from_this<NativeRetailMenuContinuation> {
public:
    NativeRetailMenuContinuation(NativeRetailMainState&,
        std::shared_ptr<NativeRetailGameSession>) noexcept;
    NativeTaskStep run() override;
    bool has_pending_child() const noexcept;
private:
    enum class Phase {
        detach_first, detach_second, clear_first_state, clear_second_state,
        push_history, select_body, begin_body, advance_body,
        transition_zero, returned
    };
    NativeRetailInitialTaskState& checked_state() const;
    NativeRetailMainState& main_;
    std::weak_ptr<NativeRetailMainLifetime> main_lifetime_;
    std::weak_ptr<NativeRetailGameSession> session_;
    std::shared_ptr<NativeRetailInputParticipant> first_, second_;
    std::shared_ptr<NativeRetailInitialChild> child_;
    unsigned selected_index_{};
    Phase phase_{Phase::detach_first};
    bool after_effect_unknown_{};
};

} // namespace mka::retail
