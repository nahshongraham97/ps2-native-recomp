#pragma once
#include "mka/movie_adx_stream.hpp"
namespace mka::video {
// Stateless compiled source-reference data only. This is not an initial
// session-mode/speaker policy or a general coefficient calculator.
class NativeMovieAdxProfileResolver final : public MovieAdxProfileResolver {
public:
    std::optional<AdxBlockProfile> resolve(const MovieAdxHeader&,
        MovieAdxStreamFailure&) noexcept override;
};
} // namespace mka::video
