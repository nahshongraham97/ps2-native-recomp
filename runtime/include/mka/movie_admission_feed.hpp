#pragma once
#include "mka/movie_header_boundary.hpp"
#include "mka/movie_header_unit.hpp"
#include "mka/movie_input_acceptance.hpp"

namespace mka::video {
enum class MovieAdmissionFeedError { none, invalid_owner, wrong_thread, invalid_source, foreign_source,
    invalid_order, invalid_prefix, range, overflow, allocation, host_capacity, parser, failed_owner, reentry, unsupported, scope };
struct MovieAdmissionFeedFailure {
    MovieAdmissionFeedError kind = MovieAdmissionFeedError::none;
    MovieHeaderBoundaryFailure boundary;
    MovieHeaderUnitFailure unit;
    MovieInputReceiptError input_receipt=MovieInputReceiptError::none;
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
// Issued only after the same Feed actually parses an OUTER header batch.
// Initiating prefix and checked parser endpoint are separate native facts.
class MovieOuterHeaderBatch final {
public:
    ~MovieOuterHeaderBatch()=default;
    MovieOuterHeaderBatch(const MovieOuterHeaderBatch&)=delete;
    MovieOuterHeaderBatch& operator=(const MovieOuterHeaderBatch&)=delete;
    const MovieVideoPrefixObserved& initiating_prefix() const noexcept{return origin_;}
    const std::optional<MovieVideoPrefixObserved>& stopping_prefix() const noexcept{return stop_;}
    std::uint64_t parse_end() const noexcept{return end_;}
    std::uint64_t offered_end() const noexcept{return offered_end_;}
    const std::optional<MovieVideoPrefixObserved>& timestamp_prefix() const noexcept{return timestamp_prefix_;}
    std::uint64_t batch_id() const noexcept{return id_;}
    std::uint64_t parsed_units() const noexcept{return units_;}
    const std::shared_ptr<const MovieAdmissionCurrent>& current() const noexcept{return current_;}
private:
    friend class NativeMovieAdmissionFeed;
    MovieOuterHeaderBatch(MovieVideoPrefixObserved origin,std::optional<MovieVideoPrefixObserved> stop,
        std::uint64_t end,std::uint64_t id,std::uint64_t units,std::shared_ptr<const MovieAdmissionCurrent> current,
        std::uint64_t offered_end,std::optional<MovieVideoPrefixObserved> timestamp_prefix)
        :origin_(std::move(origin)),stop_(std::move(stop)),end_(end),id_(id),units_(units),current_(std::move(current)),
         offered_end_(offered_end),timestamp_prefix_(std::move(timestamp_prefix)){}
    const MovieVideoPrefixObserved origin_;
    const std::optional<MovieVideoPrefixObserved> stop_;
    const std::uint64_t end_,id_,units_;
    const std::shared_ptr<const MovieAdmissionCurrent> current_;
    const std::uint64_t offered_end_;
    const std::optional<MovieVideoPrefixObserved> timestamp_prefix_;
};
enum class MovieOuterHeaderBatchProgress {need_more,first_slice,sequence_end};
struct MovieOuterHeaderBatchResult {
    MovieOuterHeaderBatchProgress progress=MovieOuterHeaderBatchProgress::need_more;
    std::shared_ptr<const MovieOuterHeaderBatch> batch;
};

enum class MovieAdmissionCandidateKind { need_more, parsed_header, first_slice, other, blocked };
struct MovieAdmissionCandidate {
    MovieAdmissionCandidateKind kind = MovieAdmissionCandidateKind::need_more;
    std::uint64_t es_begin = 0, consumed_bytes = 0;
    std::optional<MovieVideoPrefixObserved> prefix;
    std::shared_ptr<const MovieCurrentHeaderUnit> parsed;
    MovieAdmissionCurrent current;
};
enum class MoviePictureBodyExtentState { need_more, boundary_found };
struct MoviePictureBodyExtent {
    MoviePictureBodyExtentState state = MoviePictureBodyExtentState::need_more;
    std::uint64_t es_begin = 0, es_end = 0, slice_count = 0;
    bool ended_at_program_end = false;
    MovieVideoPrefixObserved first_slice;
    std::optional<MovieVideoPrefixObserved> next_header;
    MovieAdmissionCurrent current;
};
struct MovieAdmissionFeedSnapshot {
    MovieAdmissionCurrent current;
    std::uint64_t observation_sequence = 0, es_begin = 0, es_end = 0;
    std::size_t buffered_bytes = 0;
    bool work_pending = false, program_end_observed = false, failed = false;
    std::shared_ptr<const MovieCodecInputReceipt> last_input_receipt;
    bool inner_parse_active=false,outer_batch_pending=false;
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
    // Separate persistent parser-current scopes; byte/provenance/consumption
    // state stays shared. Counters/kind are never inherited from the outer scope.
    bool begin_inner_parse(MovieAdmissionFeedFailure&) noexcept;
    bool end_inner_parse(MovieAdmissionFeedFailure&) noexcept;
    // Ingress does not parse/admit headers, increment GOP serial or consume.
    bool push(const MovieSourceObservationBatch&, MovieAdmissionFeedFailure&) noexcept;
    // Explicit source parser invocation; actual checked parser extent is the
    // only automatic consumption. First slice has consumed_bytes=0.
    std::optional<MovieAdmissionCandidate> parse_next(MovieAdmissionFeedFailure&) noexcept;
    std::optional<MovieOuterHeaderBatchResult> parse_outer_batch(MovieAdmissionFeedFailure&) noexcept;
    // Data-only range proposal for the pending picture's contiguous slice units.
    // Never consumes bytes, proves decoder work, admits or completes playback.
    std::optional<MoviePictureBodyExtent> picture_body_extent(MovieAdmissionFeedFailure&) const noexcept;
    // Authentic accepted ORIGINAL codec interval bounds a pending slice body
    // even without following input. This remains data-only/non-consuming.
    std::optional<MoviePictureBodyExtent> accepted_picture_body(
        std::shared_ptr<const MovieCodecInputReceipt>, MovieAdmissionFeedFailure&) const noexcept;
    // Source C/V proposal cache, never proof of parser/admission progress.
    std::optional<MovieHeaderBoundaryProposal> proposal(MovieAdmissionFeedFailure&) noexcept;
    // Caller acknowledges actual non-parser consumption, not a proposal/packet.
    bool skip(std::uint64_t actual_bytes, MovieAdmissionFeedFailure&) noexcept;
    std::optional<MovieAdmissionFeedSnapshot> snapshot(MovieAdmissionFeedFailure&) const noexcept;
private:
    explicit NativeMovieAdmissionFeed(std::unique_ptr<MovieAdmissionFeedState>);
    bool access(MovieAdmissionFeedFailure&, bool allow_failed = false) const noexcept;
    bool consume(std::uint64_t, MovieHeaderConsumeKind, MovieAdmissionFeedFailure&) noexcept;
    std::optional<MovieAdmissionCandidate> parse_next_owned(MovieAdmissionFeedFailure&) noexcept;
    std::unique_ptr<MovieAdmissionFeedState> state_;
};
} // namespace mka::video
