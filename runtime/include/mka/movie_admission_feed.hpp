#pragma once
#include "mka/movie_header_boundary.hpp"
#include "mka/movie_header_unit.hpp"

namespace mka::video {
enum class MovieAdmissionFeedError { none, invalid_owner, wrong_thread, invalid_source, foreign_source,
    invalid_order, invalid_prefix, range, overflow, allocation, host_capacity, parser, failed_owner, reentry };
struct MovieAdmissionFeedFailure {
    MovieAdmissionFeedError kind = MovieAdmissionFeedError::none;
    MovieHeaderBoundaryFailure boundary;
    MovieHeaderUnitFailure unit;
};
struct MovieAdmissionEpoch {
    std::string source_key;
    std::shared_ptr<const MovieSourceObservationIdentity> identity;
};
struct MovieCurrentHeaderUnit {
    MovieVideoPrefixObserved origin; // Actual FIRST-byte PES, no invented packet begin.
    std::uint64_t consumed_end;
    MovieHeaderUnit unit;
};
// Independently last parsed values: a new GOP can coexist with a PREVIOUS
// picture until the real next picture is parsed. No future source-index record.
struct MovieAdmissionCurrent {
    std::shared_ptr<const MovieAdmissionEpoch> source;
    std::shared_ptr<const MovieCurrentHeaderUnit> sequence, sequence_extension, display_extension,
        gop, picture, coding, quant_matrix, userdata;
    std::uint64_t gop_parse_entries = 0, successful_gops = 0, sequence_parse_entries = 0;
    std::uint8_t last_primary_kind = 0;
    // Backend scalar initialization is source-proven zero; this is not the
    // outer compatibility record's separate signed -1 initial sentinel.
    std::uint8_t info54 = 0, info57 = 0;
};
enum class MovieAdmissionCandidateKind { need_more, parsed_header, first_slice, other, blocked };
struct MovieAdmissionCandidate {
    MovieAdmissionCandidateKind kind = MovieAdmissionCandidateKind::need_more;
    std::uint64_t es_begin = 0, consumed_bytes = 0;
    std::optional<MovieVideoPrefixObserved> prefix;
    std::shared_ptr<const MovieCurrentHeaderUnit> parsed;
    MovieAdmissionCurrent current;
};
struct MovieAdmissionFeedSnapshot {
    MovieAdmissionCurrent current;
    std::uint64_t observation_sequence = 0, es_begin = 0, es_end = 0;
    std::size_t buffered_bytes = 0;
    bool work_pending = false, program_end_observed = false, failed = false;
};
struct MovieAdmissionFeedState;
class NativeMovieAdmissionFeed final {
public:
    static std::unique_ptr<NativeMovieAdmissionFeed> create(std::string_view source_key,
        std::shared_ptr<const MovieSourceObservationIdentity>, std::uint64_t initial_es_begin,
        MovieAdmissionFeedFailure&) noexcept;
    ~NativeMovieAdmissionFeed();
    NativeMovieAdmissionFeed(const NativeMovieAdmissionFeed&) = delete;
    NativeMovieAdmissionFeed& operator=(const NativeMovieAdmissionFeed&) = delete;
    NativeMovieAdmissionFeed(NativeMovieAdmissionFeed&&) noexcept;
    NativeMovieAdmissionFeed& operator=(NativeMovieAdmissionFeed&&) noexcept;
    // Ingress does not parse/admit headers, increment GOP serial or consume.
    bool push(const MovieSourceObservationBatch&, MovieAdmissionFeedFailure&) noexcept;
    // Explicit source parser invocation; actual checked parser extent is the
    // only automatic consumption. First slice has consumed_bytes=0.
    std::optional<MovieAdmissionCandidate> parse_next(MovieAdmissionFeedFailure&) noexcept;
    // Source C/V proposal cache, never proof of parser/admission progress.
    std::optional<MovieHeaderBoundaryProposal> proposal(MovieAdmissionFeedFailure&) noexcept;
    // Caller acknowledges actual non-parser consumption, not a proposal/packet.
    bool skip(std::uint64_t actual_bytes, MovieAdmissionFeedFailure&) noexcept;
    std::optional<MovieAdmissionFeedSnapshot> snapshot(MovieAdmissionFeedFailure&) const noexcept;
private:
    explicit NativeMovieAdmissionFeed(std::unique_ptr<MovieAdmissionFeedState>);
    bool access(MovieAdmissionFeedFailure&, bool allow_failed = false) const noexcept;
    bool consume(std::uint64_t, MovieHeaderConsumeKind, MovieAdmissionFeedFailure&) noexcept;
    std::unique_ptr<MovieAdmissionFeedState> state_;
};
} // namespace mka::video
