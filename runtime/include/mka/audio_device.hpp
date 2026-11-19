#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
namespace mka::sound {
enum class AudioConversion {unspecified,exact_shared,engine_conversion};
struct AudioDeviceConfig {
    std::string endpoint_id_utf8; // Explicit MMDevice endpoint ID; never a default endpoint.
    std::uint32_t sample_rate=0,channels=0,channel_mask=0;
    std::uint64_t buffer_duration_100ns=0;
    AudioConversion conversion=AudioConversion::unspecified;
};
enum class AudioDeviceError {none,unsupported_host,invalid_format,invalid_endpoint,
    invalid_duration,invalid_conversion,host_capacity,allocation,com_not_initialized,
    unsupported_apartment,wrong_thread,invalid_owner,closed,invalid_state,
    endpoint_open,endpoint_unavailable,format_unsupported,initialize,service,
    device_lost,service_unavailable,failed_owner,invalid_pcm,buffer_full,buffer_query,
    buffer_acquire,buffer_release,start,stop,reset,clock,invalid_role,enumeration};
struct AudioDeviceFailure {
    AudioDeviceError kind=AudioDeviceError::none;
    std::int32_t native_code=0;
    const char* operation=""; // Static operation label, including exact failing API.
};
enum class AudioEndpointRole {unspecified,console,multimedia,communications};
enum class AudioMixEncoding {unknown,ieee_float,pcm};
struct AudioNativeMixFormat {
    std::uint32_t sample_rate=0,channels=0,channel_mask=0;
    std::uint32_t container_bits=0,valid_bits=0;
    AudioMixEncoding encoding=AudioMixEncoding::unknown;
};
struct AudioEndpointInfo {
    std::string endpoint_id_utf8;
    bool default_for_role=false;
    // A format failure is explicit per endpoint; no entry is silently skipped
    // or assigned a mono/stereo layout when the OS supplied no channel mask.
    AudioNativeMixFormat mix;
    AudioDeviceFailure mix_failure;
};
struct AudioEndpointInventory {
    AudioEndpointRole requested_role=AudioEndpointRole::unspecified;
    std::string default_endpoint_id_utf8;
    AudioDeviceFailure default_failure; // e.g. no default exists for this role.
    std::vector<AudioEndpointInfo> active_render_endpoints;
};
// Read-only, caller-invoked discovery on initialized STA COM. Does not start,
// initialize an audio stream, queue PCM, change a default, or select for open().
std::optional<AudioEndpointInventory> inspect_audio_endpoints(AudioEndpointRole,AudioDeviceFailure&);
struct AudioQueueStatus {
    std::uint32_t capacity_frames=0,padding_frames=0,writable_frames=0;
    std::uint64_t submitted_frames=0,reset_epoch=0;
    bool started=false;
};
struct AudioClockReading {
    // Device-defined units. Only position/frequency denotes elapsed seconds;
    // neither field is caller PCM frames. QPC timestamp is already100ns units.
    std::uint64_t position=0,frequency=0,qpc_100ns=0,reset_epoch=0;
    bool precise=false; // GetPosition S_OK vs S_FALSE (valid but imprecise).
};
// Pure checks; do not initialize COM or touch a device.
bool validate_audio_device_config(const AudioDeviceConfig&,AudioDeviceFailure&)noexcept;
bool validate_audio_pcm(const float*,std::size_t sample_count,std::uint32_t channels,
    std::uint32_t& frames,AudioDeviceFailure&)noexcept;
class NativeAudioDevice {
public:
    ~NativeAudioDevice();
    NativeAudioDevice(const NativeAudioDevice&)=delete;
    NativeAudioDevice& operator=(const NativeAudioDevice&)=delete;
    NativeAudioDevice(NativeAudioDevice&&)noexcept;
    NativeAudioDevice& operator=(NativeAudioDevice&&)noexcept;
    // Caller must own initialized STA COM for this object's entire lifetime.
    // All methods and destruction must run on its creating thread. No callbacks,
    // hidden worker, endpoint changes or automatic reopen after device loss.
    static std::unique_ptr<NativeAudioDevice> open(const AudioDeviceConfig&,AudioDeviceFailure&);
    std::optional<AudioQueueStatus> queue_status(AudioDeviceFailure&);
    // Copies exactly sample_count interleaved float values, without gain/limiter.
    // No partial acceptance; buffer_full leaves submitted count unchanged.
    bool queue(const float*,std::size_t sample_count,AudioDeviceFailure&);
    bool start(AudioDeviceFailure&);
    bool stop(AudioDeviceFailure&); // Stops streaming; does not flush/reset.
    bool reset(AudioDeviceFailure&); // Explicit stopped-stream flush + new epoch.
    std::optional<AudioClockReading> clock(AudioDeviceFailure&);
    bool close(AudioDeviceFailure&); // Owner-thread release, including failed stream.
private:
    struct Impl;
    explicit NativeAudioDevice(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> impl_;
};
} // namespace mka::sound
