#pragma once
#include "mka/movie_pes_index.hpp"
#include "mka/movie_prefix_events.hpp"
#include "mka/mpeg2_timing.hpp"
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace mka::video {
struct MoviePictureSource {
    std::uint64_t packet_es_begin=0,picture_es_byte=0;
    MovieVideoPesSpan pes;
    std::optional<std::uint64_t> quarter_pts;
    bool operator==(const MoviePictureSource&) const = default;
};
// Native lifetime identity, never a guest address or a serialized pointer.
struct MovieSourceObservationIdentity {};
struct MovieVideoEsObserved {
    std::uint64_t es_begin=0,es_end=0;
    std::vector<std::uint8_t> bytes;
};
struct MovieVideoPrefixObserved {
    MoviePrefixEvent prefix;
    MovieVideoPesSpan pes; // FIRST byte's original owner, including cross-PES codes
    std::optional<std::uint64_t> quarter_pts;
};
struct MovieDecodedPictureObserved {
    MoviePictureSource picture;
    Mpeg2PictureTiming timing;
};
// Raw input/prefix and actual codec output observations. These do not claim
// HeaderAdmitted, SDK-public-position, a clock, a source stage or playback end.
using MovieSourceObservationValue=std::variant<MovieVideoEsObserved,
    MovieVideoPrefixObserved,MovieDecodedPictureObserved,MovieProgramEndObservation>;
struct MovieSourceObservation {
    std::uint64_t sequence=0;
    MovieSourceObservationValue value;
};
struct MovieSourceObservationBatch {
    std::string source_key;
    std::shared_ptr<const MovieSourceObservationIdentity> identity;
    std::vector<MovieSourceObservation> events;
};
} // namespace mka::video
