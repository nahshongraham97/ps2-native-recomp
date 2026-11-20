#pragma once
#include "mka/movie_timecode.hpp"
#include <optional>
#include <cstdint>
namespace mka::video {
// Complete native semantic descriptor. flag is the separate signed source
// descriptor field ignored by ordinary timecode conversion.
struct MovieRewriteDescriptor {
    MovieTimecodeDescriptor timecode;
    std::int64_t flag=0;
};
struct MovieDescriptorCache {
    MovieRewriteDescriptor descriptor;
    MovieTimecodeRational relative;
};
struct MovieDescriptorRewriteState {
    bool enabled=false;
    std::optional<std::uint64_t> previous_gop_serial;
    std::optional<MovieTimecodeRational> first_baseline;
    std::optional<MovieDescriptorCache> cache;
    std::optional<MovieDescriptorCache> published;
};
struct MovieDescriptorRewriteInput {
    // Actual complete current value AFTER the source's direct/history stage.
    MovieRewriteDescriptor current;
    std::optional<std::uint64_t> timestamp22500;
    std::uint64_t gop_serial;
    bool repeat;
    std::int64_t comparison_margin;
    bool preserve_direct_without_cache;
};
enum class MovieDescriptorRewriteBranch {direct,position,zero_origin,cached,incremented};
enum class MovieDescriptorRewriteError {none,invalid_owner,unsupported_rate,invalid_temporal,overflow,timecode};
struct MovieDescriptorRewriteFailure {
    MovieDescriptorRewriteError kind=MovieDescriptorRewriteError::none;
    MovieTimecodeFailure timecode;
};
struct MovieDescriptorRewriteResult {
    MovieDescriptorCache published;
    MovieTimecodeRational first_baseline;
    MovieDescriptorRewriteBranch branch=MovieDescriptorRewriteBranch::direct;
    bool gop_changed=false,enabled_latched=false,cache_updated=false;
};
// Pure fixed-storage scalar owner; no clocks, callbacks, guest fields or
// implicit admission. Every failed update leaves all state unchanged.
class NativeMovieDescriptorRewrite final {
public:
    NativeMovieDescriptorRewrite() noexcept=default;
    NativeMovieDescriptorRewrite(const NativeMovieDescriptorRewrite&)=delete;
    NativeMovieDescriptorRewrite& operator=(const NativeMovieDescriptorRewrite&)=delete;
    NativeMovieDescriptorRewrite(NativeMovieDescriptorRewrite&&) noexcept;
    NativeMovieDescriptorRewrite& operator=(NativeMovieDescriptorRewrite&&) noexcept;
    bool set_rewrite_enabled(bool,MovieDescriptorRewriteFailure&) noexcept;
    std::optional<MovieDescriptorRewriteState> snapshot(MovieDescriptorRewriteFailure&) const noexcept;
    std::optional<MovieDescriptorRewriteResult> update(const MovieDescriptorRewriteInput&,MovieDescriptorRewriteFailure&) noexcept;
private:
    MovieDescriptorRewriteState state_;
    bool valid_=true;
};
}
