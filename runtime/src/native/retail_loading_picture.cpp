#include "mka/retail_platform.hpp"
#include <new>
#include <system_error>

namespace mka::retail {
std::optional<RetailLoadingPicture> load_retail_loading_picture(
    const content::NativeContentVolume& volume,RetailPlatformFailure& failure) noexcept {
    failure={};
    try {
        RetailLoadingPicture picture;
        constexpr std::size_t stored_bytes=256u*128u*3u;
        auto file=volume.open_resource("\\LOADING.RAW",failure.content);
        if(!file) {
            // This is the original game's explicit missing-file image, not a
            // stand-in for an unavailable native service. Unsafe roots, denied
            // access and other failures remain named native errors.
            if(failure.content.kind!=content::ContentVolumeError::open_failed ||
                failure.content.host_error!=std::errc::no_such_file_or_directory) {
                failure.kind=RetailPlatformError::content;
                failure.operation="native loading picture open";
                return {};
            }
            picture.origin=RetailLoadingOrigin::source_missing_file_pattern;
            picture.rgb.assign(stored_bytes,0x80);
            return picture;
        }
        const auto length=file->length(failure.content);
        if(!length) {
            failure.kind=RetailPlatformError::content;
            failure.operation="native loading picture length";
            return {};
        }
        if(*length!=stored_bytes) {
            failure.kind=RetailPlatformError::invalid_picture;
            failure.operation="retail loading RAW24 exact source extent";
            return {};
        }
        picture.rgb.resize(stored_bytes);
        const auto read=file->read(picture.rgb.data(),picture.rgb.size(),failure.content);
        if(read.error || read.bytes!=stored_bytes) {
            failure.kind=RetailPlatformError::content;
            failure.operation="native loading picture complete read";
            return {};
        }
        return picture;
    } catch(const std::bad_alloc&) {
        failure.kind=RetailPlatformError::allocation;
        failure.operation="native loading picture allocation";
    } catch(...) {
        failure.kind=RetailPlatformError::native_exception;
        failure.operation="native loading picture";
    }
    return {};
}
} // namespace mka::retail
