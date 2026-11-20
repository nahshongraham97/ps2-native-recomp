#pragma once
#include "mka/movie_catalog.hpp"
#include "mka/movie_decode.hpp"
#include <filesystem>
#include <memory>

namespace mka::video {
enum class MovieSourceError { none, catalog, asset, decoder, allocation, invalid_owner };
struct MovieSourceFailure {
    MovieSourceError kind = MovieSourceError::none;
    const char* operation = "";
    MovieCatalogFailure catalog_failure;
    MovieAssetFailure asset_failure;
    MovieDecodeFailure decoder_failure;
};
// Owned semantic request -> explicit associated asset handle -> native decoder.
// No path reopen, runtime search, guest ABI, clock, player status or output.
class NativeRetailMovieSource {
public:
    NativeRetailMovieSource(const NativeRetailMovieSource&) = delete;
    NativeRetailMovieSource& operator=(const NativeRetailMovieSource&) = delete;
    NativeRetailMovieSource(NativeRetailMovieSource&&) noexcept = default;
    NativeRetailMovieSource& operator=(NativeRetailMovieSource&& other) noexcept {
        if (this != &other) {
            request_ = std::move(other.request_);
            decoder_ = std::move(other.decoder_);
        }
        return *this;
    }
    ~NativeRetailMovieSource() = default;
    static std::unique_ptr<NativeRetailMovieSource> open(const NativeMovieAssetStore&,
        const std::filesystem::path& explicit_library_directory, std::int64_t id,
        MovieViewSelection, std::int64_t game_language_code, MovieSourceFailure&);
    // Owner validity only. EOF/failure is not presentation or audio completion.
    bool valid() const noexcept { return bool(decoder_); }
    const RetailMovieRequest* request() const noexcept { return valid() ? &request_ : nullptr; }
    std::shared_ptr<const MovieSourceObservationIdentity> observation_identity() const noexcept {
        return decoder_?decoder_->observation_identity():nullptr;
    }
    // Non-consuming fact from the SAME owned decoder. false is successful
    // video-only; nullopt is a named invalid/failed owner or audit failure.
    std::optional<bool> audio_present(MovieSourceFailure&) noexcept;
    MovieReadResult next(MovieSourceFailure&);
private:
    NativeRetailMovieSource(RetailMovieRequest&& request, std::unique_ptr<NativeMovieDecoder>&& decoder)
        : request_(std::move(request)), decoder_(std::move(decoder)) {}
    RetailMovieRequest request_;
    std::unique_ptr<NativeMovieDecoder> decoder_;
};
const char* movie_source_error_name(MovieSourceError) noexcept;
// Request dimensions/language are retained inputs, not yet applied render or
// subtitle policy. Gain, metadata/control, mixer, player/caller remain separate.
} // namespace mka::video
