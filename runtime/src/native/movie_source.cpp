#include "mka/movie_source.hpp"
#include <new>

namespace mka::video {
std::unique_ptr<NativeRetailMovieSource> NativeRetailMovieSource::open(
        const NativeMovieAssetStore& store, const std::filesystem::path& library_directory,
        std::int64_t id, MovieViewSelection view, std::int64_t code, MovieSourceFailure& failure) {
    failure = {};
    try {
        MovieCatalogFailure catalog_failure;
        auto request = RetailMovieCatalog{}.resolve(id,view,code,catalog_failure);
        if (!request) {
            failure.kind = MovieSourceError::catalog;
            failure.operation = "native movie semantic request";
            failure.catalog_failure = catalog_failure;
            return nullptr;
        }
        MovieAssetFailure asset_failure;
        auto asset = store.open(request->logical_key,asset_failure);
        if (!asset) {
            failure.kind = MovieSourceError::asset;
            failure.operation = "native movie exact associated asset";
            failure.asset_failure = asset_failure;
            return nullptr;
        }
        MovieDecodeFailure decoder_failure;
        // The ONLY decoder entry here consumes the exact already-open handle.
        auto decoder = NativeMovieDecoder::open(library_directory,std::move(*asset),decoder_failure);
        if (!decoder) {
            failure.kind = MovieSourceError::decoder;
            failure.operation = "native movie owned-asset decoder open";
            failure.decoder_failure = std::move(decoder_failure);
            return nullptr;
        }
        return std::unique_ptr<NativeRetailMovieSource>(
            new NativeRetailMovieSource(std::move(*request),std::move(decoder)));
    } catch (const std::bad_alloc&) {
        failure.kind = MovieSourceError::allocation;
        failure.operation = "native movie source ownership allocation";
        return nullptr;
    }
}
std::optional<bool> NativeRetailMovieSource::audio_present(MovieSourceFailure& failure) noexcept {
    failure={};
    if(!decoder_){failure.kind=MovieSourceError::invalid_owner;failure.operation="native movie moved source audio presence";return {};}
    try {
        MovieDecodeFailure why;auto audit=decoder_->audio_audit(why);
        if(!audit||audit->decoder_failed){
            failure.kind=MovieSourceError::decoder;failure.operation="native movie owned audio presence audit";
            failure.decoder_failure=std::move(why);
            if(audit&&audit->decoder_failed)failure.decoder_failure={MovieDecodeError::failed_owner,0,"native movie audio presence failed decoder"};
            return {};
        }
        return audit->present;
    }catch(const std::bad_alloc&){failure.kind=MovieSourceError::allocation;failure.operation="native movie source audio presence allocation";return {};}
}
MovieReadResult NativeRetailMovieSource::next(MovieSourceFailure& failure) {
    failure = {};
    if (!decoder_) {
        failure.kind = MovieSourceError::invalid_owner;
        failure.operation = "native movie moved source";
        return {};
    }
    try {
        MovieDecodeFailure decoder_failure;
        auto result = decoder_->next(decoder_failure);
        if (result.status == MovieReadStatus::error) {
            failure.kind = MovieSourceError::decoder;
            failure.operation = "native movie source decoder read";
            failure.decoder_failure = std::move(decoder_failure);
        }
        return result;
    } catch (const std::bad_alloc&) {
        failure.kind = MovieSourceError::allocation;
        failure.operation = "native movie source frame allocation";
        return {};
    }
}
const char* movie_source_error_name(MovieSourceError error) noexcept {
    switch(error) {
    case MovieSourceError::none: return "none";
    case MovieSourceError::catalog: return "catalog";
    case MovieSourceError::asset: return "asset";
    case MovieSourceError::decoder: return "decoder";
    case MovieSourceError::allocation: return "allocation";
    case MovieSourceError::invalid_owner: return "invalid_owner";
    }
    return "unknown_movie_source_error";
}
} // namespace mka::video
