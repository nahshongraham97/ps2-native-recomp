#include "mka/movie_catalog.hpp"
#include <array>
#include <exception>
#include <new>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace mka::video {
namespace {
struct Row {
    std::uint32_t id;
    std::string_view normal;
    std::uint64_t width, height;
    std::string_view alternate;
    std::uint64_t alternate_width, alternate_height;
    std::uint32_t route, extra;
    std::string_view tail_normal, tail_alternate;
};
#define MKA_MOVIE_CATALOG_ROW(id,n,w,h,a,aw,ah,r,e,tn,ta) Row{id,n,w,h,a,aw,ah,r,e,tn,ta},
constexpr Row rows[] = {
#include "generated/movie_catalog_rows.inc"
};
#undef MKA_MOVIE_CATALOG_ROW
constexpr bool valid_key(std::string_view key) {
    if (key.empty() || key.front() == '/' || key.back() == '/') return false;
    for (auto c : key)
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '_' || c == '.' || c == '/')) return false;
    return key.find("..") == key.npos && key.find("//") == key.npos;
}
constexpr bool valid_rows() {
    if (std::size(rows) != 33) return false;
    std::array<bool,33> seen{};
    std::size_t keys = 0;
    for (const auto& row : rows) {
        if (row.id >= seen.size() || seen[row.id] || !valid_key(row.normal) ||
            !row.width || !row.height || row.route != 0 || row.extra != 0) return false;
        seen[row.id] = true;
        ++keys;
        if (row.alternate.empty()) {
            if (row.alternate_width || row.alternate_height) return false;
        } else {
            if (!valid_key(row.alternate) || !row.alternate_width || !row.alternate_height ||
                row.alternate == row.normal) return false;
            ++keys;
        }
        for (const auto& other : rows) {
            if (other.id == row.id) continue;
            if (row.normal == other.normal || row.normal == other.alternate ||
                (!row.alternate.empty() && (row.alternate == other.normal || row.alternate == other.alternate)))
                return false;
        }
    }
    for (bool present : seen) if (!present) return false;
    return keys == 41;
}
static_assert(valid_rows(), "Mandatory retail movie data must cover33 unique IDs/41 unique active keys/positive extents/known blocking routes");
const Row& row_for(std::uint32_t id) noexcept {
    // IDs are checked before entry; ordering is intentionally not assumed.
    for (const auto& row : rows) if (row.id == id) return row;
    std::terminate(); // impossible after compile-time coverage; no ID0 fallback
}
std::pair<MovieSubtitleLanguage,const char*> language(std::int64_t code) noexcept {
    switch (code) {
    case 1: return {MovieSubtitleLanguage::spanish, "<SPA>"};
    case 2: return {MovieSubtitleLanguage::german, "<GER>"};
    case 3: return {MovieSubtitleLanguage::french, "<FRE>"};
    case 4: return {MovieSubtitleLanguage::italian, "<ITA>"};
    default: return {MovieSubtitleLanguage::english, "<ENG>"};
    }
}
bool valid_id(std::int64_t id, MovieCatalogFailure& failure) noexcept {
    failure = {};
    if (id >= 0 && id < 33) return true;
    failure = {MovieCatalogError::invalid_id,"retail movie ID0..32"}; return false;
}
}
std::optional<RetailMovieRequest> RetailMovieCatalog::resolve(std::int64_t id,
        MovieViewSelection view, std::int64_t code, MovieCatalogFailure& failure) const noexcept {
    if (!valid_id(id,failure)) return std::nullopt;
    if (view != MovieViewSelection::normal && view != MovieViewSelection::alternate) {
        failure = {MovieCatalogError::invalid_view,"retail movie game view"}; return std::nullopt;
    }
    const auto& row = row_for(static_cast<std::uint32_t>(id));
    if (row.route != 0) {
        failure = {MovieCatalogError::unsupported_route,"retail movie separate slot-owned route"}; return std::nullopt;
    }
    const bool alternate = view == MovieViewSelection::alternate && !row.alternate.empty();
    const auto selected_language = language(code);
    try {
        return RetailMovieRequest{row.id,std::string(alternate ? row.alternate : row.normal),
            {alternate ? row.alternate_width : row.width,alternate ? row.alternate_height : row.height},
            view,alternate ? MovieViewSelection::alternate : MovieViewSelection::normal,
            MovieRoute::blocking,row.extra,selected_language.first,selected_language.second};
    } catch (const std::bad_alloc&) {
        failure = {MovieCatalogError::allocation,"owned retail movie request"}; return std::nullopt;
    } catch (const std::length_error&) {
        failure = {MovieCatalogError::allocation,"owned retail movie request extent"}; return std::nullopt;
    }
}
std::optional<RetailMovieMetadata> RetailMovieCatalog::metadata(std::int64_t id,
        MovieCatalogFailure& failure) const noexcept {
    if (!valid_id(id,failure)) return std::nullopt;
    const auto& row = row_for(static_cast<std::uint32_t>(id));
    try {
        RetailMovieMetadata result{row.id,{std::string(row.normal),{row.width,row.height}},
            std::nullopt,std::string(row.tail_normal),std::string(row.tail_alternate)};
        if (!row.alternate.empty()) result.alternate = RetailMovieVariant{
            std::string(row.alternate),{row.alternate_width,row.alternate_height}};
        return result;
    } catch (const std::bad_alloc&) {
        failure = {MovieCatalogError::allocation,"owned retail movie metadata"}; return std::nullopt;
    } catch (const std::length_error&) {
        failure = {MovieCatalogError::allocation,"owned retail movie metadata extent"}; return std::nullopt;
    }
}
const char* movie_catalog_error_name(MovieCatalogError error) noexcept {
    switch(error) {
    case MovieCatalogError::none: return "none";
    case MovieCatalogError::invalid_id: return "invalid_id";
    case MovieCatalogError::invalid_view: return "invalid_view";
    case MovieCatalogError::unsupported_route: return "unsupported_route";
    case MovieCatalogError::allocation: return "allocation";
    }
    return "unknown_movie_catalog_error";
}
} // namespace mka::video
