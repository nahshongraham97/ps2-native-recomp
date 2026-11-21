#include "mka/retail_initial_task.hpp"
#include "mka/retail_initial_reset_required.hpp"
#include "mka/retail_input_assignments.hpp"
#include "mka/retail_input_roles.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_pad_control.hpp"
#include "mka/retail_section_workspace.hpp"
#include "native_retail_startup.hpp"
#include "retail_main_control.hpp"

namespace mka::retail {
namespace {
[[noreturn]] void fail(const char* operation) {
    throw RetailInitialTaskException(RetailInitialTaskError::after_effect_unknown, operation);
}
void require(bool result, const char* operation) {
    if (!result) fail(operation);
}

// Native locals survive the original12ABD0 virtual+14 suspension. No source
// instruction, register image, device queue or fixed address map is executed.
class NativeRetailInitialReset final : public NativeRetailInitialChild {
public:
    explicit NativeRetailInitialReset(NativeRetailMainState& main)
        : main_(main), lifetime_(main.lifetime), session_(main.startup.game_session) {}

    RetailInitialChildStep advance() override {
        const auto lifetime = lifetime_.lock();
        const auto session = session_.lock();
        if (!lifetime || !lifetime->active_on_owner_thread())
            fail("18D970 Main lifetime/thread");
        StartupFailure failure;
        if (!session || !session->validate_current_thread(failure.game) ||
            main_.startup.game_session != session)
            fail("18D970 retained actual game Session");
        if (failed_after_effect_) fail("18D970 prior partial native reset");
        try {
            if (phase_ == Phase::start) {
                begin(*session, failure);
                phase_ = Phase::saved_profile;
            }
            if (phase_ == Phase::saved_profile) {
                require(initial_reset_required::reset_saved_profile_children_246070(main_, failure),
                        "246070 native saved profile children");
                phase_ = Phase::selected_resource;
            }
            if (phase_ == Phase::selected_resource) {
                require(initial_reset_required::reset_selected_resource_334700(main_, failure),
                        "334700 actual native selected resource");
                auto* records = session->game_records_values(failure.game);
                require(records != nullptr, "165780 retained native game values");
                records->word_660098 = 0xffffffffu; // Original165780(-1) full-word store.
                phase_ = Phase::request_banks;
            }
            if (phase_ == Phase::request_banks) {
                require(initial_reset_required::request_mode_one_banks_12a4f0(main_, failure),
                        "12A4F0(1) actual native sound bank requests");
                phase_ = Phase::wait_banks;
            }
            if (phase_ == Phase::wait_banks) {
                const auto pending = initial_reset_required::banks_pending_12abd0(main_, failure);
                if (!pending) fail("12ABD0 native shared catalog observation");
                //12ABE0 starts at1800, decrements before the zero check and
                // waits at most1799 times. This is retail timeout behavior,
                // independent of native job capacity or a hardware clock.
                if (*pending && --remaining_scans_ != 0)
                    return RetailInitialChildStep::wait(1.0f);
                phase_ = Phase::clear_references;
            }
            if (phase_ == Phase::clear_references) {
                require(initial_reset_required::clear_nonowning_references_18da30(main_, failure),
                        "18DA30 native nonowning reference/cookie clears");
                phase_ = Phase::configure_resource;
            }
            if (phase_ == Phase::configure_resource) {
                require(initial_reset_required::configure_selected_resource_zero_16fc70(main_, failure),
                        "16FC70 native selected resource zero");
                phase_ = Phase::resume_input;
            }
            if (phase_ == Phase::resume_input) {
                auto* domain = session->task_domain(failure.game);
                require(domain && main_.input_roles && resume_retail_input_from_163a50(
                            *session, *domain, *main_.input_roles, failure.game_record_reset),
                        "163A50 actual native input resume");
                phase_ = Phase::final_count;
            }
            if (phase_ == Phase::final_count) {
                RetailInputAssignmentFailure assignment_failure;
                require(main_.input_assignments &&
                        main_.input_assignments->reset_count_from_18d970(assignment_failure),
                        "18DA50 final native association count reset");
                phase_ = Phase::complete;
            }
            return RetailInitialChildStep::completed();
        } catch (const detail::NativeMainReentry&) {
            // This is a game control transfer. The task dispatcher owns its
            // continuation contract; it must never become an error latch.
            throw;
        } catch (...) {
            failed_after_effect_ = true;
            throw;
        }
    }

private:
    void begin(NativeRetailGameSession& session, StartupFailure& failure) {
        require(main_.startup.section_workspace && main_.startup.memory_owners &&
                main_.startup.section_workspace->select_from_19c550(
                    3, *main_.startup.memory_owners, failure.section_workspace),
                "19C550(3) native section storage selection");
        RetailParticipantFailure participant_failure;
        require(bool(main_.participants), "18D970 native participant owners");
        const auto first = main_.participants->first_two(0, participant_failure);
        const auto second = main_.participants->first_two(1, participant_failure);
        require(first && second, "18D970 retained first/second participants");
        require(first->store_source_word_2c(0x40, participant_failure), "18D988 first+2C");
        require(second->store_source_word_2c(0x40, participant_failure), "18D994 second+2C");
        require(first->store_initial_game_word_14(0, participant_failure), "18D9A0 first+14");
        require(second->store_initial_game_word_14(0, participant_failure), "18D9A8 second+14");

        const auto controls = session.controls(failure.game);
        const auto flags = controls ? controls->selection_flags(failure.game.controls) : std::nullopt;
        require(flags.has_value(), "18D9B0 actual selection flag byte");
        // ANDI from zero at18D9B4 contributes zero. The call delay slot
        // clears bit2 before185CC0 stores scalar mode12.
        require(controls->store_selection_flags_from_source(*flags & 0xfbu, failure.game.controls),
                "18D9CC clear selection byte bit2");
        require(session.store_scalar_mode_from_source(12, failure.game), "185CC0 scalar mode12");
        auto* state = session.initial_task_state(failure.game);
        RetailInitialTaskStateFailure state_failure;
        require(state && state->store_first_transition_latch(false, state_failure), "18D9D4 first latch clear");
        require(state->store_second_transition_latch(false, state_failure), "18D9DC second latch clear");
        RetailModeFailure mode_failure;
        require(main_.mode_history && main_.mode_history->push(3, mode_failure), "185D60 history3");
        require(first->set_game_state(0, participant_failure), "1319E0 first state0");
        require(second->set_game_state(0, participant_failure), "1319E0 second state0");
        RetailInputAssignmentFailure assignment_failure;
        require(main_.input_assignments &&
                main_.input_assignments->reset_count_from_18d970(assignment_failure),
                "18DA08 first native association count reset");
    }

    enum class Phase { start, saved_profile, selected_resource, request_banks,
                       wait_banks, clear_references, configure_resource,
                       resume_input, final_count, complete };
    NativeRetailMainState& main_;
    std::weak_ptr<NativeRetailMainLifetime> lifetime_;
    std::weak_ptr<NativeRetailGameSession> session_;
    Phase phase_{Phase::start};
    std::uint32_t remaining_scans_{1800};
    bool failed_after_effect_{};
};
} // namespace

namespace initial_task_required {
std::shared_ptr<NativeRetailInitialChild> begin_reset_18d970(
    NativeRetailMainState& main, NativeRetailGameSession& session) {
    if (!main.lifetime || !main.lifetime->active_on_owner_thread())
        fail("18D970 reset factory Main lifetime/thread");
    if (main.startup.game_session.get() != &session)
        fail("18D970 reset factory foreign Session");
    return std::make_shared<NativeRetailInitialReset>(main);
}
} // namespace initial_task_required
} // namespace mka::retail
