#pragma once
#include "mka/native_file.hpp"
#include <memory>
#include <optional>
#include <string>
#include <string_view>
namespace mka::video {
struct MovieAssetCatalogEntry {
    std::string logical_utf8;
    std::string relative_host_utf8;
};
enum class MovieAssetError { none, moved_store, moved_asset, invalid_utf8,
    invalid_name, absolute_path, traversal, iso_provider_suffix, invalid_root,
    invalid_host_path, duplicate_key, root_escape, not_regular_file,
    association_changed, unknown_key, file_open, file_read, file_seek,
    file_position, file_length, allocation_failed, host_capacity,
    opened_path_unavailable, opened_path_mismatch };
struct MovieAssetFailure {
    MovieAssetError kind=MovieAssetError::none;
    std::error_code host_error;
    std::size_t catalog_index=0;
};
struct MovieAssetBinding;
class NativeMovieAssetStore;
class NativeMovieAsset {
public:
    NativeMovieAsset(const NativeMovieAsset&)=delete;
    NativeMovieAsset& operator=(const NativeMovieAsset&)=delete;
    NativeMovieAsset(NativeMovieAsset&&) noexcept=default;
    NativeMovieAsset& operator=(NativeMovieAsset&&) noexcept=default;
    bool valid()const noexcept;
    std::string_view logical_key()const noexcept;
    const std::filesystem::path* associated_path()const noexcept;
    content::FileRead read(void*,std::size_t,MovieAssetFailure&);
    bool seek(std::int64_t,content::SeekOrigin,MovieAssetFailure&);
    std::optional<std::uint64_t> position(MovieAssetFailure&);
    std::optional<std::uint64_t> length(MovieAssetFailure&);
private:
    NativeMovieAsset(std::shared_ptr<const MovieAssetBinding> b,content::NativeFile&& f):binding_(std::move(b)),file_(std::move(f)){}
    bool available(MovieAssetFailure&)const;
    std::shared_ptr<const MovieAssetBinding> binding_;
    content::NativeFile file_;
    friend class NativeMovieAssetStore;
};
// BEFORE-provider namespace: supplied root is explicitly the MOVIEPS2 directory.
// Catalog entries are caller-verified logical -> actual relative host mappings.
// ASCII case and separators normalize keys; there is no request-time directory
// search, ISO ';1' add/remove, numeric catalog, sector cursor or fallback path.
// Windows validates the actual opened handle's final path before publication,
// not just a pathname checked before fopen. No byte authenticity/retail
// identity or protection against subsequent content mutation is claimed.
// Binding captures actual handle-reported spelling; caller case aliases do
// not become a guessed case-folded opened-object identity comparison.
// Non-Windows opened-handle path verification is explicitly unsupported.
class NativeMovieAssetStore {
public:
    NativeMovieAssetStore(const NativeMovieAssetStore&)=default;
    NativeMovieAssetStore& operator=(const NativeMovieAssetStore&)=default;
    NativeMovieAssetStore(NativeMovieAssetStore&&) noexcept=default;
    NativeMovieAssetStore& operator=(NativeMovieAssetStore&&) noexcept=default;
    static std::optional<NativeMovieAssetStore> create(const std::filesystem::path& explicit_movie_root,
        const std::vector<MovieAssetCatalogEntry>& verified_catalog,MovieAssetFailure&);
    std::optional<NativeMovieAsset> open(std::string_view logical_utf8,MovieAssetFailure&)const;
private:
    struct State;
    explicit NativeMovieAssetStore(std::shared_ptr<const State> s):state_(std::move(s)){}
    std::shared_ptr<const State> state_;
};
} // namespace mka::video
