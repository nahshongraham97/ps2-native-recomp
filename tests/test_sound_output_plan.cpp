#include "mka/sound_output.hpp"
#include "mka/program_host_clock.hpp"
#include <climits>
#include <cstdio>
#include <stdexcept>
using namespace mka::sound;
namespace {void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}}
int main() try {
    AudioEndpointInventory inventory;
    inventory.requested_role=AudioEndpointRole::console;inventory.default_endpoint_id_utf8="queried-game-endpoint";
    AudioEndpointInfo device;device.endpoint_id_utf8=inventory.default_endpoint_id_utf8;device.default_for_role=true;
    device.mix={44100,6,0x3f,32,32,AudioMixEncoding::ieee_float};device.default_period_100ns=100000;
    inventory.active_render_endpoints.push_back(device);
    AudioDeviceFailure f;
    auto plan=plan_game_sound_output(inventory,f);
    check(plan&&plan->endpoint_id_utf8==device.endpoint_id_utf8&&plan->sample_rate==44100&&
        plan->channels==2&&plan->channel_mask==3&&plan->buffer_duration_100ns==200000&&
        plan->conversion==AudioConversion::engine_conversion,"actual game endpoint/rate/period with explicit stereo conversion");
    inventory.active_render_endpoints[0].mix.sample_rate=96000;
    inventory.active_render_endpoints[0].default_period_100ns=1000;
    plan=plan_game_sound_output(inventory,f);
    check(plan&&plan->sample_rate==96000&&plan->buffer_duration_100ns==100000,
        "host rate is queried independently; buffering also covers the source5ms worker");
    auto bad=inventory;bad.default_endpoint_id_utf8="absent-game-endpoint";
    check(!plan_game_sound_output(bad,f)&&f.kind==AudioDeviceError::endpoint_unavailable,
        "another active endpoint never replaces the missing game endpoint");
    bad=inventory;bad.requested_role=AudioEndpointRole::multimedia;
    check(!plan_game_sound_output(bad,f)&&f.kind==AudioDeviceError::invalid_role,"explicit game role");
    bad=inventory;bad.default_failure={AudioDeviceError::endpoint_unavailable,-41,"original default lookup"};
    check(!plan_game_sound_output(bad,f)&&f.native_code==-41,"default lookup detail preserved");
    bad=inventory;bad.active_render_endpoints.push_back(bad.active_render_endpoints[0]);
    check(!plan_game_sound_output(bad,f)&&f.kind==AudioDeviceError::invalid_endpoint,"ambiguous inventory refused");
    bad=inventory;bad.active_render_endpoints[0].mix_failure={AudioDeviceError::device_lost,-42,"original format query"};
    check(!plan_game_sound_output(bad,f)&&f.kind==AudioDeviceError::device_lost&&f.native_code==-42,"format failure not replaced by a profile");
    bad=inventory;bad.active_render_endpoints[0].period_failure={AudioDeviceError::device_period,-43,"original period query"};
    check(!plan_game_sound_output(bad,f)&&f.native_code==-43,"period failure not replaced by a duration");
    bad=inventory;bad.active_render_endpoints[0].default_period_100ns=0;
    check(!plan_game_sound_output(bad,f)&&f.kind==AudioDeviceError::invalid_duration,"zero period refused");
    bad=inventory;bad.active_render_endpoints[0].default_period_100ns=static_cast<std::uint64_t>(INT64_MAX);
    check(!plan_game_sound_output(bad,f)&&f.kind==AudioDeviceError::host_capacity,"buffer margin has checked host arithmetic");
    bad=inventory;bad.active_render_endpoints[0].mix.channel_mask=3;
    check(!plan_game_sound_output(bad,f)&&f.kind==AudioDeviceError::format_unsupported,"host layout validated without an inferred mask");
    ProgramHostClockFailure clock_failure;
    auto native=NativeProgramHostClock::create(clock_failure);
    auto injected=NativeProgramHostClock::create_with_reader(1000,[](ProgramHostClockFailure&){return std::optional<std::int64_t>{0};},clock_failure);
    check(native&&native->uses_native_counter()&&injected&&!injected->uses_native_counter(),
        "only the fixed native counter has serialized-reader provenance");
    std::puts("PASS native game-output planning/counter provenance; pure checks, no endpoint opened");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL output plan: %s\n",e.what());return 1;}
