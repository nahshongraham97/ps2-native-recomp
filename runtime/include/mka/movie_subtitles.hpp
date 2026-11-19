#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace mka::video {
enum class MovieSubtitleError {
    none, moved_document, foreign_cursor, truncated_header, invalid_hex_length,
    body_out_of_bounds, wrong_root, trailing_root_bytes, missing_type,
    unterminated_type, missing_number, malformed_number, number_out_of_range,
    invalid_time_unit, boundary_overflow, allocation_failed, host_capacity,
    inactive_content, unsupported_language, unterminated_content,
    missing_language, unterminated_language
};
struct MovieSubtitleFailure {
    MovieSubtitleError kind = MovieSubtitleError::none;
    std::size_t offset = 0; // offset in the owned root bytes, never a guest address
};
const char* movie_subtitle_error_name(MovieSubtitleError) noexcept;
struct MovieSubtitleTag {
    std::array<std::uint8_t, 8> raw_tag;
    std::size_t header_offset, body_offset, body_size;
    std::optional<std::size_t> parent_tag;
};
// Native timeline ABI, independent of the legacy SDK register width. Serialized
// %d fields remain signed32. Units must be positive; no clock/unit is inferred.
struct MovieSubtitleClock { std::int64_t value, unit; };
// Explicit original 3E4E50 locale mapping. No guessed/default locale.
enum class MovieSubtitleLanguage { english, french, german, spanish, italian };
struct MovieSubtitleStorage;
class NativeMovieSubtitles;
class MovieSubtitleCursor {
public:
    std::int32_t channel;
    std::int64_t offset;
    std::optional<std::size_t> last_event_identity() const noexcept { return last_; }
private:
    MovieSubtitleCursor(std::shared_ptr<const MovieSubtitleStorage> owner,
                        std::int32_t ch, std::int64_t off)
        : channel(ch), offset(off), owner_(std::move(owner)) {}
    std::shared_ptr<const MovieSubtitleStorage> owner_;
    std::optional<std::size_t> last_;
    friend class NativeMovieSubtitles;
};
class MovieSubtitleQuery {
public:
    bool active = false, changed = false;
    std::optional<std::size_t> event_identity;
    std::int32_t time_unit = 0, intime = 0, duration = 0;
    // Includes all original bytes (including any NUL and legacy encoding).
    // Result owns a shared immutable lifetime independent of document/cursor.
    std::span<const std::uint8_t> content() const noexcept;
private:
    std::shared_ptr<const MovieSubtitleStorage> owner_;
    std::size_t content_offset_ = 0, content_size_ = 0;
    friend class NativeMovieSubtitles;
};
// Bounded original 3E40C0 normal-content operation: first literal language
// marker before the C-string NUL, then bytes until the next '<'. Returns owned
// legacy bytes, including literal backslash-n; performs no UTF8/font/layout
// work. Missing/unterminated/unsupported input rejects rather than unsafe scan.
std::optional<std::vector<std::uint8_t>> extract_movie_subtitle_language(
    const MovieSubtitleQuery&, MovieSubtitleLanguage, MovieSubtitleFailure&);
// Owns one explicit serialized CRITAGS header+body. No PS demux, continuation,
// playback clock, language/text decoding, font, renderer or player integration.
class NativeMovieSubtitles {
public:
    NativeMovieSubtitles(const NativeMovieSubtitles&) = default;
    NativeMovieSubtitles& operator=(const NativeMovieSubtitles&) = default;
    NativeMovieSubtitles(NativeMovieSubtitles&&) noexcept = default;
    NativeMovieSubtitles& operator=(NativeMovieSubtitles&&) noexcept = default;
    static std::optional<NativeMovieSubtitles> parse(std::vector<std::uint8_t> owned_root,
                                                   MovieSubtitleFailure&);
    std::span<const std::uint8_t> raw_bytes() const noexcept;
    // Unknown bodies, including SFVFTRS, remain opaque in raw_bytes. Only the
    // queried first SUBTITLE group's known containers are structurally opened.
    std::span<const MovieSubtitleTag> tags() const noexcept;
    std::optional<MovieSubtitleCursor> cursor(std::int32_t channel, std::int64_t offset,
                                             MovieSubtitleFailure&) const;
    // nullopt means error; an inactive result means a successful empty query.
    // Errors and empty queries do not change last_event_identity().
    std::optional<MovieSubtitleQuery> query(MovieSubtitleCursor&, MovieSubtitleClock,
                                          MovieSubtitleFailure&) const;
private:
    explicit NativeMovieSubtitles(std::shared_ptr<const MovieSubtitleStorage> s)
        : storage_(std::move(s)) {}
    std::shared_ptr<const MovieSubtitleStorage> storage_;
};
} // namespace mka::video
