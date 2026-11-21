#include "mka/retail_input_poll.hpp"
#include "mka/retail_input_assignments.hpp"
#include "mka/retail_participants.hpp"

#include <array>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <thread>

using namespace mka::retail;
namespace input = mka::input;
namespace {
void check(bool value,const char* message) {
    if(!value)throw std::runtime_error(message);
}
input::ControllerState full(std::uint16_t buttons, std::int16_t left_x,
                             std::int16_t left_y, bool connected=true,
                             std::int16_t right_x=0, std::int16_t right_y=0) {
    input::NativeControllerAxes axes;
    axes.sticks={{right_x,right_y,left_x,left_y}};
    // Keep the legacy byte payload deliberately neutral. Direction outcomes
    // must come from the full signed host values, never this byte field.
    return input::digital_state(buttons,connected,{{128,128,128,128}},255,255,axes);
}
NativeRetailInputRoleObservation observe(NativeRetailInputRoles& roles,
                                         RetailInputRole role) {
    RetailInputRoleFailure failure;
    auto value=roles.observe(role,failure);
    check(bool(value),"actual role observation");
    return *value;
}
}

int main() {
    try {
        RetailInputRoleFailure role_failure;
        auto roles=NativeRetailInputRoles::create(role_failure);
        RetailGameControlsFailure controls_failure;
        auto controls=NativeRetailGameControls::create(controls_failure);
        RetailParticipantFailure participant_failure;
        auto participants=NativeRetailParticipants::create_first_two(participant_failure);
        RetailInputAssignmentFailure assignment_failure;
        auto assignments=NativeRetailInputAssignments::create(roles,participants,assignment_failure);
        check(roles&&controls&&participants&&assignments,"real game input owners born");
        auto participant=participants->first_two(0,participant_failure);
        check(participant&&assignments->bind_direct(participant,RetailInputRole::role0,
              assignment_failure),"actual participant association established");
        input::ControllerInput samples;
        RetailAssignedPollFailure f;
        auto poll=NativeRetailAssignedPoll::create(roles,controls,samples,f);
        check(bool(poll),"assigned-ready owner bound to actual game and host sample owners");
        auto initial=observe(*roles,RetailInputRole::role0);
        check(!initial.axes&&!initial.initialized&&initial.previous==0&&
              initial.current==0&&initial.rising==0,
              "source-zero axis storage is native absence before first sample");

        check(!poll->ready_assigned(*roles,*controls,samples,RetailInputRole::role0,7,
              RetailAnalogDeadzone{48},f)&&
              f.kind==RetailAssignedPollError::missing_host_route,
              "unpublished host player is not automatic assignment");
        check(samples.publish(7,full(1,32767,0)),"full-resolution host sample published");
        check(poll->ready_assigned(*roles,*controls,samples,RetailInputRole::role0,7,
              RetailAnalogDeadzone{48},f),"first assigned ready sample");
        auto first=observe(*roles,RetailInputRole::role0);
        check(first.initialized&&first.previous==0&&first.current==1&&
              first.rising==1&&first.axes&&(*first.axes)[2]==32767,
              "first frame sets bit1 and publishes full host axes after digital edge");

        check(samples.publish(7,full(0,0,0))&&
              poll->ready_assigned(*roles,*controls,samples,RetailInputRole::role0,7,
              RetailAnalogDeadzone{48},f),"second assigned ready sample");
        auto second=observe(*roles,RetailInputRole::role0);
        check(second.previous==1&&second.current==0x2000&&second.rising==0x2000&&
              second.axes&&(*second.axes)[2]==0,
              "assigned analog uses previous left X and row13 mask before new axes");
        check(poll->ready_assigned(*roles,*controls,samples,RetailInputRole::role0,7,
              RetailAnalogDeadzone{48},f),"third ready sample");
        auto third=observe(*roles,RetailInputRole::role0);
        check(third.previous==0x2000&&third.current==0&&third.rising==0,
              "new neutral axis only affects following poll");

        check(samples.publish(7,full(0,0,0,true,32767,-32768))&&
              poll->ready_assigned(*roles,*controls,samples,RetailInputRole::role0,7,
              RetailAnalogDeadzone{48},f)&&
              samples.publish(7,full(0,0,0))&&
              poll->ready_assigned(*roles,*controls,samples,RetailInputRole::role0,7,
              RetailAnalogDeadzone{48},f)&&
              observe(*roles,RetailInputRole::role0).current==0,
              "source param2 zero uses sample bytes six/seven, not right-stick bytes four/five");
        check(samples.publish(7,full(0,32767,-32768))&&
              poll->ready_assigned(*roles,*controls,samples,RetailInputRole::role0,7,
              RetailAnalogDeadzone{48},f)&&
              samples.publish(7,full(0,0,0))&&
              poll->ready_assigned(*roles,*controls,samples,RetailInputRole::role0,7,
              RetailAnalogDeadzone{48},f)&&
              observe(*roles,RetailInputRole::role0).current==(0x2000|0x1000),
              "different left X/Y signs route to independent source row13/row12 masks");

        check(samples.publish(7,full(0,-32768,0))&&
              poll->ready_assigned(*roles,*controls,samples,RetailInputRole::role0,7,
              RetailAnalogDeadzone{48},f)&&
              samples.publish(7,full(0,0,0))&&
              poll->ready_assigned(*roles,*controls,samples,RetailInputRole::role0,7,
              RetailAnalogDeadzone{48},f),"negative X prior-axis sequence");
        check(observe(*roles,RetailInputRole::role0).current==0x8000,
              "left X negative selects source row15 mask");
        check(samples.publish(7,full(0,0,-32768))&&
              poll->ready_assigned(*roles,*controls,samples,RetailInputRole::role0,7,
              RetailAnalogDeadzone{48},f)&&
              samples.publish(7,full(0,0,0))&&
              poll->ready_assigned(*roles,*controls,samples,RetailInputRole::role0,7,
              RetailAnalogDeadzone{48},f),"negative Y prior-axis sequence");
        check(observe(*roles,RetailInputRole::role0).current==0x1000,
              "left Y negative selects source row12 mask");
        check(samples.publish(7,full(0,0,32767))&&
              poll->ready_assigned(*roles,*controls,samples,RetailInputRole::role0,7,
              RetailAnalogDeadzone{48},f)&&
              samples.publish(7,full(0,0,0))&&
              poll->ready_assigned(*roles,*controls,samples,RetailInputRole::role0,7,
              RetailAnalogDeadzone{48},f),"positive Y prior-axis sequence");
        check(observe(*roles,RetailInputRole::role0).current==0x4000,
              "left Y positive selects source row14 mask");

        // The next two full-precision negative values have the SAME old
        // compatibility byte (79), yet straddle the logical 48/127 threshold.
        check(((32768-12384)>>8)==((32768-12385)>>8),
              "threshold neighbors share old 8-bit representation");
        check(samples.publish(7,full(0,-12384,0))&&
              poll->ready_assigned(*roles,*controls,samples,RetailInputRole::role0,7,
              RetailAnalogDeadzone{48},f)&&
              samples.publish(7,full(0,0,0))&&
              poll->ready_assigned(*roles,*controls,samples,RetailInputRole::role0,7,
              RetailAnalogDeadzone{48},f),"negative below-threshold pair");
        check(observe(*roles,RetailInputRole::role0).current==0,
              "full native negative value just below threshold stays quiet");
        check(samples.publish(7,full(0,-12385,0))&&
              poll->ready_assigned(*roles,*controls,samples,RetailInputRole::role0,7,
              RetailAnalogDeadzone{48},f)&&
              samples.publish(7,full(0,0,0))&&
              poll->ready_assigned(*roles,*controls,samples,RetailInputRole::role0,7,
              RetailAnalogDeadzone{48},f),"negative at-threshold pair");
        check(observe(*roles,RetailInputRole::role0).current==0x8000,
              "one signed16 host unit changes result without byte quantization");
        check(samples.publish(7,full(0,12287,0))&&
              poll->ready_assigned(*roles,*controls,samples,RetailInputRole::role0,7,
              RetailAnalogDeadzone{48},f)&&
              samples.publish(7,full(0,0,0))&&
              poll->ready_assigned(*roles,*controls,samples,RetailInputRole::role0,7,
              RetailAnalogDeadzone{48},f)&&
              observe(*roles,RetailInputRole::role0).current==0,
              "positive full16 magnitude below 48/128 threshold stays quiet");
        check(samples.publish(7,full(0,12288,0))&&
              poll->ready_assigned(*roles,*controls,samples,RetailInputRole::role0,7,
              RetailAnalogDeadzone{48},f)&&
              samples.publish(7,full(0,0,0))&&
              poll->ready_assigned(*roles,*controls,samples,RetailInputRole::role0,7,
              RetailAnalogDeadzone{48},f)&&
              observe(*roles,RetailInputRole::role0).current==0x2000,
              "positive full16 magnitude at 48/128 threshold selects row13");

        // 163930 consumes bits in role order 2,1,0, then bit3 for global
        // analog inhibit. It does not alter role 3.
        check(poll->store_gate_from_163930(*roles,*controls,0b1010,f),
              "real source gate stores three role bits and global inhibit");
        RetailGameControlsFailure gate_failure;
        check(!observe(*roles,RetailInputRole::role2).dispatch_blocked&&
              observe(*roles,RetailInputRole::role1).dispatch_blocked&&
              !observe(*roles,RetailInputRole::role0).dispatch_blocked&&
              !observe(*roles,RetailInputRole::role3).dispatch_blocked&&
              controls->analog_input_inhibited(gate_failure)==std::optional<bool>{true},
              "163930 reverse role order and final inhibit bit exact");
        check(samples.publish(7,full(0,-32768,0))&&
              poll->ready_assigned(*roles,*controls,samples,RetailInputRole::role0,7,
              RetailAnalogDeadzone{48},f)&&
              samples.publish(7,full(0,0,0))&&
              poll->ready_assigned(*roles,*controls,samples,RetailInputRole::role0,7,
              RetailAnalogDeadzone{48},f)&&
              observe(*roles,RetailInputRole::role0).current==0,
              "live inhibit prevents prior-axis direction OR");
        check(poll->store_gate_from_163930(*roles,*controls,0,f)&&
              controls->analog_input_inhibited(gate_failure)==std::optional<bool>{false},
              "source gate can restore native analog input");

        const auto before=observe(*roles,RetailInputRole::role0);
        check(samples.publish(7,full(1,0,0,false))&&
              !poll->ready_assigned(*roles,*controls,samples,RetailInputRole::role0,7,
                RetailAnalogDeadzone{48},f)&&f.kind==RetailAssignedPollError::disconnected,
              "disconnected route is not status5 or assigned-ready");
        const auto after=observe(*roles,RetailInputRole::role0);
        check(before.current==after.current&&before.previous==after.previous&&
              before.rising==after.rising&&before.axes==after.axes,
              "failure did not partially mutate role");
        auto legacy=input::digital_state(1,true);
        check(samples.publish(7,legacy)&&
              !poll->ready_assigned(*roles,*controls,samples,RetailInputRole::role0,7,
                RetailAnalogDeadzone{48},f)&&
              f.kind==RetailAssignedPollError::missing_native_axes,
              "legacy byte-only sample cannot masquerade as full native axis sample");
        auto foreign_roles=NativeRetailInputRoles::create(role_failure);
        input::ControllerInput foreign_samples;
        check(!poll->ready_assigned(*foreign_roles,*controls,samples,RetailInputRole::role0,7,
                RetailAnalogDeadzone{48},f)&&f.kind==RetailAssignedPollError::foreign_owner,
              "foreign role owner refused");
        check(!poll->ready_assigned(*roles,*controls,foreign_samples,RetailInputRole::role0,7,
                RetailAnalogDeadzone{48},f)&&f.kind==RetailAssignedPollError::foreign_owner,
              "foreign sample store refused");
        check(!poll->ready_assigned(*roles,*controls,samples,static_cast<RetailInputRole>(9),7,
                RetailAnalogDeadzone{48},f)&&f.kind==RetailAssignedPollError::invalid_role,
              "invalid role refused");
        check(!poll->ready_assigned(*roles,*controls,samples,RetailInputRole::role2,7,
                RetailAnalogDeadzone{48},f)&&f.kind==RetailAssignedPollError::invalid_role,
              "source 2214d0 never samples role two as a physical position");
        bool wrong_thread=false;
        std::thread other([&] {
            RetailAssignedPollFailure off;
            wrong_thread=!poll->store_gate_from_163930(*roles,*controls,0,off)&&
                off.kind==RetailAssignedPollError::wrong_thread;
        });
        other.join();
        check(wrong_thread,"foreign thread cannot mutate source gate or role records");
        auto retained_controls=controls.get();
        controls.reset();
        check(poll->store_gate_from_163930(*roles,*retained_controls,0,f),
              "poll owner retains actual game controls across caller release");
        std::puts("PASS native assigned-ready source transition and full precision host axes");
        return 0;
    } catch(const std::exception& e) {
        std::fprintf(stderr,"%s\n",e.what());
        return 1;
    }
}
