#pragma once
#include "mka/audio_device.hpp"

namespace mka::video {
// Owned packet/client accounting only. Identity and source rate come from the
// same owning source context, never inferred from these receipt coordinates.
struct MoviePlaybackAudioAcceptance {
    std::uint64_t source_first_frame, source_frames;
    std::uint64_t packet_client_frames, packet_client_offset;
    sound::AudioSubmission submission;
};
} // namespace mka::video
