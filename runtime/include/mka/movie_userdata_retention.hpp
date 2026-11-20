#pragma once
#include "mka/movie_header_unit.hpp"
#include "mka/movie_source_observations.hpp"
#include <string_view>
#include <thread>
namespace mka::video {
enum class MovieUserdataPrimaryKind {initial,sequence,gop,picture};
enum class MovieUserdataRetentionError {none,invalid_owner,wrong_thread,invalid_source,foreign_source,
    invalid_kind,invalid_unit,overflow,allocation,host_capacity};
struct MovieUserdataRetentionFailure {MovieUserdataRetentionError kind=MovieUserdataRetentionError::none;};
struct MovieUserdataRetentionState {
    std::string source_key;std::shared_ptr<const MovieSourceObservationIdentity> identity;
    bool bound=false,phase=false;std::uint64_t field_counter=0;std::vector<std::uint8_t> bytes;
    bool operator==(const MovieUserdataRetentionState&) const = default;
};
// Current host-owned retention only. Invoke after actual registered sink and
// notification, using fresh backend settings; never from prefix scanning alone.
class NativeMovieUserdataRetention {
public:
    static std::optional<NativeMovieUserdataRetention> create(std::string_view,
        std::shared_ptr<const MovieSourceObservationIdentity>,bool bound,MovieUserdataRetentionFailure&) noexcept;
    NativeMovieUserdataRetention(const NativeMovieUserdataRetention&)=delete;
    NativeMovieUserdataRetention& operator=(const NativeMovieUserdataRetention&)=delete;
    NativeMovieUserdataRetention(NativeMovieUserdataRetention&&) noexcept;
    NativeMovieUserdataRetention& operator=(NativeMovieUserdataRetention&&) noexcept;
    bool set_bound(std::string_view,const std::shared_ptr<const MovieSourceObservationIdentity>&,
        bool,MovieUserdataRetentionFailure&) noexcept;
    bool coding_extension(std::string_view,const std::shared_ptr<const MovieSourceObservationIdentity>&,
        const MovieHeaderUnit&,MovieUserdataRetentionFailure&) noexcept;
    bool append(std::string_view,const std::shared_ptr<const MovieSourceObservationIdentity>&,
        MovieUserdataPrimaryKind,const MovieHeaderUnit&,MovieUserdataRetentionFailure&) noexcept;
    std::optional<MovieUserdataRetentionState> snapshot(MovieUserdataRetentionFailure&) const noexcept;
private:
    NativeMovieUserdataRetention(std::string,std::shared_ptr<const MovieSourceObservationIdentity>,bool) noexcept;
    bool owner(MovieUserdataRetentionFailure&) const noexcept;
    bool source(std::string_view,const std::shared_ptr<const MovieSourceObservationIdentity>&,MovieUserdataRetentionFailure&) const noexcept;
    MovieUserdataRetentionState state_;std::thread::id thread_;bool valid_=true;
};
} // namespace mka::video
