#pragma once
#include "mka/movie_source_observations.hpp"
#include <thread>
#include <string_view>
namespace mka::video {
enum class MovieTimestampAssociationError {none,invalid_source,foreign_source,invalid_prefix,
    unsupported_rate,invalid_temporal,invalid_coding,overflow,allocation,host_capacity,wrong_thread,reentry};
struct MovieTimestampAssociationFailure {MovieTimestampAssociationError kind=MovieTimestampAssociationError::none;};
struct MovieTimestampAssociationState {
    std::optional<std::int64_t> origin;
    std::optional<std::uint64_t> cached_raw_timestamp22500;
    std::optional<MovieVideoPrefixObserved> cached_query;
    std::int64_t base_temporal=0,offset=0,max_delta=0;
};
struct MovieTimestampAssociationSnapshot {
    std::string source_key;
    std::shared_ptr<const MovieSourceObservationIdentity> identity;
    MovieTimestampAssociationState values;
};
struct MovieTimestampAssociationResult {
    MovieTimestampAssociationSnapshot snapshot;
    std::optional<MovieVideoPrefixObserved> query;
    std::optional<std::uint64_t> raw_timestamp22500;
    std::optional<std::int64_t> adjusted_timestamp22500;
    bool origin_initialized=false,record_changed=false;
};
// Actual source driver chooses the FIRST RAW picture prefix in its original
// preparse offered range. This owner does not infer that role from outputs.
class NativeMovieTimestampAssociation final {
public:
    static std::unique_ptr<NativeMovieTimestampAssociation> create(std::string_view,
        std::shared_ptr<const MovieSourceObservationIdentity>,MovieTimestampAssociationFailure&) noexcept;
    ~NativeMovieTimestampAssociation();
    NativeMovieTimestampAssociation(const NativeMovieTimestampAssociation&)=delete;
    NativeMovieTimestampAssociation& operator=(const NativeMovieTimestampAssociation&)=delete;
    std::optional<MovieTimestampAssociationResult> update(std::string_view,
        const std::shared_ptr<const MovieSourceObservationIdentity>&,
        const std::optional<MovieVideoPrefixObserved>& first_raw_picture,
        std::uint8_t current_rate_code,std::uint16_t current_temporal,
        std::uint8_t current_coding_type,bool outer_gop_changed,
        MovieTimestampAssociationFailure&) noexcept;
    std::optional<MovieTimestampAssociationSnapshot> snapshot(MovieTimestampAssociationFailure&) const noexcept;
private:
    NativeMovieTimestampAssociation(std::string,std::shared_ptr<const MovieSourceObservationIdentity>);
    bool access(MovieTimestampAssociationFailure&) const noexcept;
    std::string key_;
    std::shared_ptr<const MovieSourceObservationIdentity> identity_;
    MovieTimestampAssociationState state_;
    const std::thread::id owner_=std::this_thread::get_id();
    mutable bool busy_=false;
};
}
