#pragma once
#include "mka/movie_subtitles.hpp"
#include <cstdint>
#include <optional>
#include <string>

namespace mka::video {
enum class MovieViewSelection { normal, alternate };
enum class MovieRoute { blocking, slot_owned };
enum class MovieCatalogError { none, invalid_id, invalid_view, unsupported_route, allocation };
struct MovieCatalogFailure {
    MovieCatalogError kind = MovieCatalogError::none;
    const char* operation = ""; // static storage
};
struct MovieLogicalExtent { std::uint64_t width, height; };
struct RetailMovieRequest {
    std::uint32_t id;
    std::string logical_key;
    MovieLogicalExtent logical_extent;
    MovieViewSelection requested_view, selected_view;
    MovieRoute route;
    std::uint32_t route_extra;
    MovieSubtitleLanguage subtitle_language;
    std::string subtitle_tag;
};
struct RetailMovieVariant {
    std::string logical_key;
    MovieLogicalExtent logical_extent;
};
struct RetailMovieMetadata {
    std::uint32_t id;
    RetailMovieVariant normal;
    std::optional<RetailMovieVariant> alternate;
    std::string opaque_tail_normal, opaque_tail_alternate;
};
// Semantic source catalog only. Its41 selectable keys are NOT an eager file
// catalog: actual38 asset bindings/preflight belong to NativeMovieAssetStore.
// Requests own their strings and survive this stateless catalog's lifetime.
// Logical extents do not select decoded extent, canvas, SAR or GPU policy.
class RetailMovieCatalog {
public:
    std::optional<RetailMovieRequest> resolve(std::int64_t id, MovieViewSelection,
        std::int64_t game_language_code, MovieCatalogFailure&) const noexcept;
    std::optional<RetailMovieMetadata> metadata(std::int64_t id,
        MovieCatalogFailure&) const noexcept;
};
const char* movie_catalog_error_name(MovieCatalogError) noexcept;
// Skip callbacks, actual caller/factory/player, clocks/audio and asset opening
// are separate explicit bindings. No guest pointers, environment or defaults.
} // namespace mka::video
