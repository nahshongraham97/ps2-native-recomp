#pragma once
#include "mka/audio_device.hpp"
#include <memory>
namespace mka::sound {
// Source5244 -> AA88 -> thbase33 supplies5000 microseconds. This is a game
// service interval, not an IOP clock or an audio-device sample rate/capacity.
inline constexpr std::uint64_t retail_sound_worker_microseconds=5000;
// Pure native host policy: the Windows game-role endpoint supplies its rate
// and engine period. The current mixer produces stereo float; WASAPI performs
// the explicit conversion to the endpoint layout. Never selects another device.
std::optional<AudioDeviceConfig> plan_game_sound_output(const AudioEndpointInventory&,AudioDeviceFailure&);
// Owned by, used on, and destroyed on the native sound worker's thread.
class NativeSoundOutput {
public:
    virtual ~NativeSoundOutput()=default;
    virtual AudioStreamFormat format() const noexcept=0;
    virtual std::optional<AudioQueueStatus> queue_status(AudioDeviceFailure&)=0;
    virtual std::optional<AudioSubmission> queue(const float*,std::size_t,AudioDeviceFailure&)=0;
    virtual bool start(AudioDeviceFailure&)=0;
    virtual bool close(AudioDeviceFailure&) noexcept=0;
};
// Real production destination, including worker-thread STA COM ownership.
// Tests may wrap this symbol; production never supplies an injected profile.
std::unique_ptr<NativeSoundOutput> open_native_game_sound_output(AudioDeviceFailure&);
}
