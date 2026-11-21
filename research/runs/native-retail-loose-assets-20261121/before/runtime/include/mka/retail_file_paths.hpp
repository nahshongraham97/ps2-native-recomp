#pragma once

#include "mka/content_volume.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mka::retail {

enum class RetailFilePathError {
    none,
    invalid_source_path,
    unknown_alias,
    duplicate_alias,
    empty_file_key,
    allocation,
    host_capacity,
    content_file,
    native_exception
};

struct RetailFilePathFailure {
    RetailFilePathError kind = RetailFilePathError::none;
    content::ContentVolumeFailure content{};
};

// Native owner for the eight aliases registered by retail 2DF470. The built-in
// retail /cdrom provider becomes the extracted content-volume root. No IOP
// module, guest pointer, sector address, or device handle is retained here.
class NativeRetailFilePaths final {
public:
    NativeRetailFilePaths(const NativeRetailFilePaths&) = delete;
    NativeRetailFilePaths& operator=(const NativeRetailFilePaths&) = delete;

    static std::unique_ptr<NativeRetailFilePaths> create(RetailFilePathFailure&) noexcept;

    // Resolve the original leading-slash alias syntax, then open the actual
    // file through NativeContentVolume. Its validated file owner survives this
    // path owner's destruction.
    std::optional<content::NativeContentFile> open(
        const content::NativeContentVolume&, std::string_view retail_path,
        RetailFilePathFailure&) const noexcept;

private:
    struct Alias {
        std::string name;
        std::string volume_prefix;
    };
    std::vector<Alias> aliases_;

    NativeRetailFilePaths() = default;
    bool add_source_alias(std::string_view name, std::string_view target,
                          RetailFilePathFailure&);
    std::optional<std::string> resolve(std::string_view path,
                                       RetailFilePathFailure&) const;
};

} // namespace mka::retail
