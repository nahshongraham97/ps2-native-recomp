#include "mka/retail_input_poll.hpp"

#include <new>
#include <utility>

namespace mka::retail {
namespace {
bool fail(RetailAssignedPollFailure& f, RetailAssignedPollError kind,
          const char* operation) noexcept {
    f.kind=kind;
    f.operation=operation;
    return false;
}
}

NativeRetailAssignedPoll::NativeRetailAssignedPoll(
    std::shared_ptr<NativeRetailInputRoles> roles,
    std::shared_ptr<NativeRetailGameControls> controls,
    input::ControllerInput& samples) noexcept
    : roles_(std::move(roles)), controls_(std::move(controls)), samples_(&samples) {}

std::unique_ptr<NativeRetailAssignedPoll> NativeRetailAssignedPoll::create(
    std::shared_ptr<NativeRetailInputRoles> roles,
    std::shared_ptr<NativeRetailGameControls> controls,
    input::ControllerInput& samples,
    RetailAssignedPollFailure& f) noexcept {
    f={};
    if(!roles||!controls) {
        fail(f,RetailAssignedPollError::invalid_owner,"retail assigned poll owners");
        return {};
    }
    RetailInputRoleFailure role_failure;
    if(!roles->observe(RetailInputRole::role0,role_failure)) {
        f.role=role_failure;
        fail(f,RetailAssignedPollError::role,"retail assigned poll role owner");
        return {};
    }
    RetailGameControlsFailure controls_failure;
    if(!controls->analog_input_inhibited(controls_failure)) {
        f.controls=controls_failure;
        fail(f,RetailAssignedPollError::controls,"retail assigned poll game controls");
        return {};
    }
    try {
        return std::unique_ptr<NativeRetailAssignedPoll>(
            new NativeRetailAssignedPoll(std::move(roles),std::move(controls),samples));
    } catch(const std::bad_alloc&) {
        fail(f,RetailAssignedPollError::allocation,"retail assigned poll allocation");
    } catch(...) {
        fail(f,RetailAssignedPollError::native_exception,"retail assigned poll creation");
    }
    return {};
}

bool NativeRetailAssignedPoll::access_gate(
    NativeRetailInputRoles& roles, NativeRetailGameControls& controls,
    RetailAssignedPollFailure& f) const noexcept {
    f={};
    if(std::this_thread::get_id()!=thread_)
        return fail(f,RetailAssignedPollError::wrong_thread,"retail assigned poll thread");
    if(&roles!=roles_.get()||&controls!=controls_.get())
        return fail(f,RetailAssignedPollError::foreign_owner,"retail assigned poll owner identity");
    RetailInputRoleFailure role_failure;
    if(!roles.role(RetailInputRole::role0,role_failure)) {
        f.role=role_failure;
        return fail(f,RetailAssignedPollError::role,"retail assigned poll role access");
    }
    RetailGameControlsFailure controls_failure;
    if(!controls.analog_input_inhibited(controls_failure)) {
        f.controls=controls_failure;
        return fail(f,RetailAssignedPollError::controls,"retail assigned poll control access");
    }
    return true;
}

bool NativeRetailAssignedPoll::access(
    NativeRetailInputRoles& roles, NativeRetailGameControls& controls,
    const input::ControllerInput& samples,
    RetailAssignedPollFailure& f) const noexcept {
    if(!access_gate(roles,controls,f)) return false;
    if(&samples!=samples_)
        return fail(f,RetailAssignedPollError::foreign_owner,"retail assigned poll sample identity");
    return true;
}

bool NativeRetailAssignedPoll::store_gate_from_163930(
    NativeRetailInputRoles& roles, NativeRetailGameControls& controls,
    std::uint32_t source_mask, RetailAssignedPollFailure& f) noexcept {
    if(!access_gate(roles,controls,f)) return false;
    // Source visits 2, 1, 0 and shifts the mask after each store.
    for(std::uint32_t role=3;role-- >0;) {
        roles.roles_[role].dispatch_blocked=(source_mask&1u)!=0;
        source_mask>>=1;
    }
    RetailGameControlsFailure controls_failure;
    if(!controls.store_analog_input_inhibited_from_source((source_mask&1u)!=0,
            controls_failure)) {
        f.controls=controls_failure;
        return fail(f,RetailAssignedPollError::controls,"retail 163930 analog inhibit store");
    }
    return true;
}

// Native full-resolution threshold. Source 1632F0 subtracts 127 from each
// byte, clears |difference| strictly below the live threshold, then divides
// negative values by 127 and nonnegative values by 128. Host sticks are
// signed 16-bit with center zero, so compare equivalent normalized magnitudes
// without first reducing them to a source byte.
bool NativeRetailAssignedPoll::direction(
    std::int16_t raw, std::uint8_t deadzone) noexcept {
    if(raw<0)
        return static_cast<std::int32_t>(-static_cast<std::int32_t>(raw))*127 >=
               static_cast<std::int32_t>(deadzone)*32768;
    if(raw>0)
        return static_cast<std::int32_t>(raw)*128 >=
               static_cast<std::int32_t>(deadzone)*32767;
    return false;
}

bool NativeRetailAssignedPoll::ready_assigned(
    NativeRetailInputRoles& roles, NativeRetailGameControls& controls,
    const input::ControllerInput& samples, RetailInputRole role_id,
    std::uint32_t host_player, RetailAnalogDeadzone deadzone,
    RetailAssignedPollFailure& f) noexcept {
    if(!access(roles,controls,samples,f)) return false;
    RetailInputRoleFailure role_failure;
    if(!roles.role(role_id,role_failure)) {
        f.role=role_failure;
        return fail(f,RetailAssignedPollError::invalid_role,"retail assigned poll role");
    }
    const auto index=static_cast<std::size_t>(role_id);
    if(index>1)
        return fail(f,RetailAssignedPollError::invalid_role,
                    "retail 2214d0 samples only source roles zero and one");
    auto& state=roles.roles_[index];
    if(!state.participant.lock())
        return fail(f,RetailAssignedPollError::unassigned_role,"retail assigned poll participant");
    std::optional<input::ControllerSample> sample;
    try {
        sample=samples.sample(host_player);
    } catch (const std::bad_alloc&) {
        return fail(f,RetailAssignedPollError::allocation,"retail assigned poll sample allocation");
    } catch (...) {
        return fail(f,RetailAssignedPollError::native_exception,"retail assigned poll sample access");
    }
    if(!sample)
        return fail(f,RetailAssignedPollError::missing_host_route,"retail assigned poll host route");
    if(!sample->state.connected)
        return fail(f,RetailAssignedPollError::disconnected,"retail assigned poll disconnected route");
    if(!sample->state.native_axes)
        return fail(f,RetailAssignedPollError::missing_native_axes,"retail assigned poll full axes");
    RetailGameControlsFailure controls_failure;
    const auto inhibited=controls.analog_input_inhibited(controls_failure);
    if(!inhibited) {
        f.controls=controls_failure;
        return fail(f,RetailAssignedPollError::controls,"retail assigned poll analog inhibit");
    }

    // 2216E8..221710: current -> prior; active-low source bytes become this
    // host producer's already active-high button bits.
    const auto previous=state.current;
    std::uint32_t current=sample->state.buttons;
    // 221728 calls 280A60 before 221788 publishes the new axis word.
    if(state.analog_mapping&&!(*inhibited)&&state.axes) {
        const auto x=(*state.axes)[2], y=(*state.axes)[3];
        if(x<0&&direction(x,deadzone.source_units))
            current|=state.action_rows[15].rising_mask; // source +0xb4
        if(x>0&&direction(x,deadzone.source_units))
            current|=state.action_rows[13].rising_mask; // source +0x9c
        if(y<0&&direction(y,deadzone.source_units))
            current|=state.action_rows[12].rising_mask; // source +0x90
        if(y>0&&direction(y,deadzone.source_units))
            current|=state.action_rows[14].rising_mask; // source +0xa8
    }
    state.initialized=true;
    state.previous=previous;
    state.current=current;
    state.rising=current&(current^previous);
    // 221778..221788 publish axes only after analog masks and rising.
    state.axes=sample->state.native_axes->sticks;
    return true;
}

} // namespace mka::retail
