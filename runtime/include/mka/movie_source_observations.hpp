#pragma once
#include "mka/movie_pes_index.hpp"
#include "mka/movie_prefix_events.hpp"
#include "mka/mpeg2_timing.hpp"
#include <memory>
#include <string>
#include <variant>
#include <vector>
#include <utility>

namespace mka::video {
struct MoviePictureSource {
    std::uint64_t packet_es_begin=0,picture_es_byte=0;
    MovieVideoPesSpan pes;
    std::optional<std::uint64_t> quarter_pts;
    bool operator==(const MoviePictureSource&) const = default;
};
// Native lifetime identity, never a guest address or a serialized pointer.
struct MovieSourceObservationIdentity {};
class NativeMovieDecoder;
// Authentic native input acceptance, not decoded-picture/work/output success.
// Only the actual decoder may mint one. Copies of the shared owner retain the
// same identity and must still be checked for replay by the receiving ledger.
class MovieCodecInputReceipt final {
public:
    MovieCodecInputReceipt(const MovieCodecInputReceipt&)=delete;
    MovieCodecInputReceipt& operator=(const MovieCodecInputReceipt&)=delete;
    ~MovieCodecInputReceipt()=default;
    const std::string& source_key() const noexcept{return source_key_;}
    const std::shared_ptr<const MovieSourceObservationIdentity>& source_identity() const noexcept{return source_;}
    std::uint64_t input_id() const noexcept{return input_id_;}
    std::uint8_t stream_id() const noexcept{return stream_id_;}
    std::uint64_t es_begin() const noexcept{return es_begin_;}
    std::uint64_t es_end() const noexcept{return es_end_;}
private:
    friend class NativeMovieDecoder;
    MovieCodecInputReceipt(std::string key,std::shared_ptr<const MovieSourceObservationIdentity> source,
        std::uint64_t id,std::uint8_t stream,std::uint64_t begin,std::uint64_t end)
        :source_key_(std::move(key)),source_(std::move(source)),input_id_(id),stream_id_(stream),es_begin_(begin),es_end_(end){}
    const std::string source_key_;
    const std::shared_ptr<const MovieSourceObservationIdentity> source_;
    const std::uint64_t input_id_;
    const std::uint8_t stream_id_;
    const std::uint64_t es_begin_,es_end_;
};
struct MovieCodecInputAccepted {
    std::shared_ptr<const MovieCodecInputReceipt> receipt;
};
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
// Raw input/prefix, native accepted input and actual codec output observations. These do not claim
// HeaderAdmitted, SDK-public-position, a clock, a source stage or playback end.
using MovieSourceObservationValue=std::variant<MovieVideoEsObserved,
    MovieVideoPrefixObserved,MovieDecodedPictureObserved,MovieProgramEndObservation,MovieCodecInputAccepted>;
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
