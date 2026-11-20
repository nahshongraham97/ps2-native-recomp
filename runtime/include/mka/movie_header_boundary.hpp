#pragma once
#include "mka/movie_source_observations.hpp"
#include <optional>
#include <string_view>
#include <thread>
namespace mka::video {
enum class MovieHeaderBoundaryError {none,invalid_owner,wrong_thread,invalid_source,foreign_source,
    malformed_range,noncontiguous,range,overflow,invalid_consume_kind,allocation,host_capacity};
struct MovieHeaderBoundaryFailure {MovieHeaderBoundaryError kind=MovieHeaderBoundaryError::none;};
enum class MovieHeaderConsumeKind {parser,skip};
struct MovieHeaderBoundaryProposal {
    std::uint64_t es_begin=0,count=0;
    std::uint32_t classification=0;
    bool other=false,empty=false,need_lookahead=false;
    bool operator==(const MovieHeaderBoundaryProposal&) const = default;
};
struct MovieHeaderBoundaryState {
    std::string source_key;
    std::shared_ptr<const MovieSourceObservationIdentity> identity;
    std::uint64_t initial_es_begin=0,es_begin=0,es_end=0;
    std::vector<std::uint8_t> bytes;
    std::optional<std::uint64_t> cached_boundary,searched_available_end;
    bool operator==(const MovieHeaderBoundaryState&) const = default;
};
// Pure source-byte boundary proposals, not parser admission/progress/completion.
// One serialized native source epoch; no guest pointer, fixed buffer or clock.
class NativeMovieHeaderBoundary {
public:
    static std::optional<NativeMovieHeaderBoundary> create(std::string_view,
        std::shared_ptr<const MovieSourceObservationIdentity>,std::uint64_t initial_es_begin,
        MovieHeaderBoundaryFailure&) noexcept;
    NativeMovieHeaderBoundary(const NativeMovieHeaderBoundary&)=delete;
    NativeMovieHeaderBoundary& operator=(const NativeMovieHeaderBoundary&)=delete;
    NativeMovieHeaderBoundary(NativeMovieHeaderBoundary&&) noexcept;
    NativeMovieHeaderBoundary& operator=(NativeMovieHeaderBoundary&&) noexcept;
    bool append(std::string_view,const std::shared_ptr<const MovieSourceObservationIdentity>&,
        const MovieVideoEsObserved&,MovieHeaderBoundaryFailure&) noexcept;
    // May update C/V cache only. NEVER consumes bytes or admits a header.
    // count4 is not generally parseable; caller retains its count<5/end guard.
    std::optional<MovieHeaderBoundaryProposal> propose(MovieHeaderBoundaryFailure&) noexcept;
    bool consume(std::string_view,const std::shared_ptr<const MovieSourceObservationIdentity>&,
        MovieHeaderConsumeKind,std::uint64_t amount,MovieHeaderBoundaryFailure&) noexcept;
    // Owned diagnostic copy. A rejected operation has no scalar/byte/cache commit.
    std::optional<MovieHeaderBoundaryState> snapshot(MovieHeaderBoundaryFailure&) const noexcept;
private:
    NativeMovieHeaderBoundary(std::string,std::shared_ptr<const MovieSourceObservationIdentity>,std::uint64_t) noexcept;
    bool admit(MovieHeaderBoundaryFailure&) const noexcept;
    bool source(std::string_view,const std::shared_ptr<const MovieSourceObservationIdentity>&,
        MovieHeaderBoundaryFailure&) const noexcept;
    std::uint32_t classify(std::uint64_t) const noexcept;
    std::optional<std::uint64_t> first(std::uint32_t) const noexcept;
    std::optional<std::uint64_t> last(std::uint32_t) const noexcept;
    bool refresh(std::uint32_t,std::optional<std::uint64_t>) const noexcept;
    MovieHeaderBoundaryState state_;
    std::thread::id owner_;
    bool valid_=true;
};
} // namespace mka::video
