#pragma once
#include "mka/movie_admission_feed.hpp"
#include <thread>

namespace mka::video {
enum class MovieOutputPositionProfile { unspecified, successful_frame_no_repeat_no_skip };
enum class MovieOutputPositionError { none, invalid_owner, invalid_source, foreign_source,
    wrong_thread, unsupported_profile, invalid_work, invalid_selected, replay, closed, overflow, allocation };
struct MovieOutputPositionFailure {
    MovieOutputPositionError kind = MovieOutputPositionError::none;
    const char* operation = "";
};
// Explicit coordinator completion, NOT inferred from decoder output/pump.
// IDs are native producer identities; current header is not selected pixels.
struct MovieOutputPositionWork {
    std::uint64_t work_id, admission_id;
    std::shared_ptr<const MovieAdmissionCurrent> current_header;
};
struct MovieOutputPositionPublication {
    std::string source_key;
    std::shared_ptr<const MovieSourceObservationIdentity> identity;
    MovieOutputPositionWork work;
    MoviePictureSource selected;
    std::uint64_t value;
    bool final_flush;
};
struct MovieOutputPositionUpdate {
    std::optional<MovieOutputPositionPublication> publication;
    std::uint64_t successful_work;
    bool flushed;
};
struct MovieOutputPositionSnapshot {
    std::uint64_t successful_work = 0, last_work_id = 0;
    std::optional<std::uint64_t> first_output_work, published_position;
    std::size_t published_pictures = 0;
    bool flushed = false;
    bool operator==(const MovieOutputPositionSnapshot&) const = default;
};
struct MovieOutputPositionState;
// Pure business publication order. No codec reference buffers, timestamp or
// frame ordinal is used. Caller authenticates successful work/accepted output
// against its admission coordinator and actual selected native decoder source.
class NativeMovieOutputPosition final {
public:
    static std::unique_ptr<NativeMovieOutputPosition> create(std::string source_key,
        std::shared_ptr<const MovieSourceObservationIdentity>, MovieOutputPositionProfile,
        MovieOutputPositionFailure&) noexcept;
    ~NativeMovieOutputPosition();
    NativeMovieOutputPosition(const NativeMovieOutputPosition&) = delete;
    NativeMovieOutputPosition& operator=(const NativeMovieOutputPosition&) = delete;
    NativeMovieOutputPosition(NativeMovieOutputPosition&&) noexcept;
    NativeMovieOutputPosition& operator=(NativeMovieOutputPosition&&) noexcept;
    // A successful no-output completion still advances work; first accepted
    // output latches current work, publishes zero, then normal work advances.
    std::optional<MovieOutputPositionUpdate> complete_work(std::string_view source_key,
        const std::shared_ptr<const MovieSourceObservationIdentity>&,
        const MovieOutputPositionWork&, const std::optional<MoviePictureSource>& accepted_selected,
        MovieOutputPositionFailure&) noexcept;
    // Explicit successful final output/control operation. Never decoder EOF.
    // Publishes current work minus origin without normal post-publication advance.
    std::optional<MovieOutputPositionUpdate> flush(std::string_view source_key,
        const std::shared_ptr<const MovieSourceObservationIdentity>&,
        const MovieOutputPositionWork&, const std::optional<MoviePictureSource>& accepted_selected,
        MovieOutputPositionFailure&) noexcept;
    std::optional<MovieOutputPositionSnapshot> snapshot(MovieOutputPositionFailure&) const noexcept;
private:
    explicit NativeMovieOutputPosition(std::unique_ptr<MovieOutputPositionState>);
    bool access(MovieOutputPositionFailure&) const noexcept;
    std::optional<MovieOutputPositionUpdate> apply(std::string_view,
        const std::shared_ptr<const MovieSourceObservationIdentity>&, const MovieOutputPositionWork&,
        const std::optional<MoviePictureSource>&, bool, MovieOutputPositionFailure&) noexcept;
    std::unique_ptr<MovieOutputPositionState> state_;
};
} // namespace mka::video
