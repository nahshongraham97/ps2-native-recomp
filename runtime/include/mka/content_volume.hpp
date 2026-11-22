#pragma once
#include "mka/native_file.hpp"
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
namespace mka::content {
enum class ContentVolumeError {none,unsupported_host,invalid_root,moved_volume,invalid_key,invalid_utf8,foreign_prefix,traversal,unsafe_component,unsupported_suffix,root_changed,root_escape,not_regular_file,open_failed,opened_path_unavailable,moved_file,read_failed,seek_failed,position_failed,length_failed,allocation_failed,host_capacity,native_exception,not_directory,directory_read_failed,close_failed,status_failed};
struct ContentVolumeFailure {ContentVolumeError kind=ContentVolumeError::none;std::error_code host_error;};
class NativeContentVolume;
// Exclusive cursor ownership. The opened file and volume identity survive moves
// and destruction of the public volume. No pathname is reopened by reads.
class NativeContentFile final {
public:
    NativeContentFile(const NativeContentFile&)=delete;
    NativeContentFile& operator=(const NativeContentFile&)=delete;
    NativeContentFile(NativeContentFile&&) noexcept=default;
    NativeContentFile& operator=(NativeContentFile&&) noexcept=default;
    bool valid() const noexcept;
    std::string_view logical_key() const noexcept;
    bool same_volume(const NativeContentFile&) const noexcept;
    FileRead read(void*,std::size_t,ContentVolumeFailure&);
    bool seek(std::int64_t,SeekOrigin,ContentVolumeFailure&);
    std::optional<std::uint64_t> length(ContentVolumeFailure&);
    std::optional<std::uint64_t> position(ContentVolumeFailure&);
    bool close_checked(ContentVolumeFailure&) noexcept;
    std::optional<bool> end_of_file(ContentVolumeFailure&)const noexcept;
private:
    NativeContentFile(std::shared_ptr<const void>,std::string,NativeFile&&);
    bool available(ContentVolumeFailure&) const;
    std::shared_ptr<const void> volume_;
    std::string key_;
    NativeFile file_;
    friend class NativeContentVolume;
    friend class NativeFileReadJob;
};
class NativeContentVolume final {
public:
    NativeContentVolume(const NativeContentVolume&)=delete;
    NativeContentVolume& operator=(const NativeContentVolume&)=delete;
    NativeContentVolume(NativeContentVolume&&) noexcept=default;
    NativeContentVolume& operator=(NativeContentVolume&&) noexcept=default;
    static std::optional<NativeContentVolume> open(const std::filesystem::path& explicit_root,ContentVolumeFailure&);
    // Validate the retained native directory identity without opening a game file.
    bool validate(ContentVolumeFailure&) const noexcept;
    // Retain the same opened native root identity; no reopen or second volume.
    std::optional<NativeContentVolume> retain(ContentVolumeFailure&) const noexcept;
    bool same_volume(const NativeContentVolume& other) const noexcept{return state_&&state_==other.state_;}
    std::optional<NativeContentFile> open_resource(std::string_view,ContentVolumeFailure&) const;
    // Direct regular files in a retained native subdirectory. Every returned
    // file owns its opened handle and directory identity. No recursive search,
    // archive provider, or pathname reopening on reads.
    std::optional<std::vector<NativeContentFile>> open_directory_files(
        std::string_view,ContentVolumeFailure&) const;
private:
    struct State;
    explicit NativeContentVolume(std::shared_ptr<State> s):state_(std::move(s)){}
    std::shared_ptr<State> state_;
};
} // namespace mka::content
