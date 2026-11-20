#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>
namespace mka::video {
enum class MoviePrefixError {none,invalid_owner,failed_owner,noncontiguous,offset_overflow,after_finish,allocation,host_capacity};
struct MoviePrefixFailure {
    MoviePrefixError kind=MoviePrefixError::none,cause=MoviePrefixError::none;
    std::uint64_t expected_offset=0,provided_offset=0;
};
struct MoviePrefixEvent {
    std::uint64_t es_prefix_first_byte=0;
    std::uint32_t raw_codeword=0,classification=0;
    bool operator==(const MoviePrefixEvent&) const = default;
};
struct MoviePrefixBatch {
    std::uint64_t es_begin=0,es_end=0;
    std::vector<MoviePrefixEvent> events;
};
struct MoviePrefixTail {
    std::uint64_t es_begin=0,source_end=0;
    // Last up-to3 input bytes, including bytes already observed in events.
    // Format lookbehind only, not an unconsumed/malformed MPEG diagnosis.
    std::vector<std::uint8_t> bytes;
};
struct MoviePrefixState {
    std::uint64_t initial_es_begin=0,next_es_byte=0,tail_begin=0;
    std::array<std::uint8_t,3> tail{};
    std::size_t tail_size=0;
    bool finished=false,failed=false;
    bool operator==(const MoviePrefixState&) const = default;
};
// Exact raw4-byte classifier7F3308, not parser success or timeline admission.
std::uint32_t movie_prefix_classification(std::uint32_t raw_codeword) noexcept;
// Serialized owned ES producer. Chunk boundaries have no semantic meaning.
// No GOP counter, CE batching, selected-frame association or clock is inferred.
class NativeMoviePrefixEvents {
public:
    static std::optional<NativeMoviePrefixEvents> create(std::uint64_t initial_es_begin,MoviePrefixFailure&) noexcept;
    NativeMoviePrefixEvents(const NativeMoviePrefixEvents&)=delete;
    NativeMoviePrefixEvents& operator=(const NativeMoviePrefixEvents&)=delete;
    NativeMoviePrefixEvents(NativeMoviePrefixEvents&&) noexcept;
    NativeMoviePrefixEvents& operator=(NativeMoviePrefixEvents&&) noexcept;
    std::optional<MoviePrefixBatch> feed(std::uint64_t es_begin,std::span<const std::uint8_t>,MoviePrefixFailure&) noexcept;
    // Idempotent until a later invalid feed. No malformed-codec inference from
    // an incomplete final prefix. All returned bytes/events are value owned.
    std::optional<MoviePrefixTail> finish(MoviePrefixFailure&) noexcept;
    // Non-allocating observation remains available after terminal feed failure.
    std::optional<MoviePrefixState> snapshot(MoviePrefixFailure&) const noexcept;
private:
    explicit NativeMoviePrefixEvents(std::uint64_t first) noexcept;
    bool admit(MoviePrefixFailure&) const noexcept;
    void terminal(MoviePrefixFailure&,MoviePrefixError,std::uint64_t provided=0) noexcept;
    MoviePrefixState state_;
    MoviePrefixFailure failure_;
    bool valid_=true;
};
} // namespace mka::video
