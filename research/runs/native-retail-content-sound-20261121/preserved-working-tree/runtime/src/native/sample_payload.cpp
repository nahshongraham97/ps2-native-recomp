#include "mka/sample_payload.hpp"
#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
#include <type_traits>

namespace mka::sound {
namespace {
template<class Bytes> std::uint32_t u32(const Bytes& b, std::size_t at) {
    return std::uint32_t(b[at]) | (std::uint32_t(b[at+1])<<8) |
           (std::uint32_t(b[at+2])<<16) | (std::uint32_t(b[at+3])<<24);
}
template<class File> bool file_position(File& file, std::uint64_t& value, PayloadFailure& failure) {
    if constexpr (std::is_same_v<File, content::NativeContentFile>) {
        content::ContentVolumeFailure error;
        auto result = file.position(error);
        if (!result) { failure.content = error; failure.host_error = error.host_error; return false; }
        value = *result; return true;
    } else return file.position(value, failure.host_error);
}
template<class File> bool file_length(File& file, std::uint64_t& value, PayloadFailure& failure) {
    if constexpr (std::is_same_v<File, content::NativeContentFile>) {
        content::ContentVolumeFailure error;
        auto result = file.length(error);
        if (!result) { failure.content = error; failure.host_error = error.host_error; return false; }
        value = *result; return true;
    } else return file.length(value, failure.host_error);
}
template<class File> bool file_seek(File& file, std::int64_t offset, PayloadFailure& failure) {
    if constexpr (std::is_same_v<File, content::NativeContentFile>) {
        content::ContentVolumeFailure error;
        const bool ok = file.seek(offset, content::SeekOrigin::begin, error);
        if (!ok) { failure.content = error; failure.host_error = error.host_error; }
        return ok;
    } else return file.seek(offset, content::SeekOrigin::begin, failure.host_error);
}
template<class File> content::FileRead file_read(File& file, void* out, std::size_t size, PayloadFailure& failure) {
    if constexpr (std::is_same_v<File, content::NativeContentFile>) {
        content::ContentVolumeFailure error;
        auto result = file.read(out, size, error);
        if (result.error) { failure.content = error; failure.host_error = error.host_error; }
        return result;
    } else {
        const auto result = file.read(out, size);
        if (result.error) failure.host_error = result.error;
        return result;
    }
}
}
std::uint32_t SamplePayload::word(std::size_t at) const noexcept {
    return bytes_.size() >= 20 ? u32(bytes_,at) : 0;
}
std::array<std::uint8_t,20> SamplePayload::header() const noexcept {
    std::array<std::uint8_t,20> out{};
    if (bytes_.size() >= out.size()) std::copy_n(bytes_.begin(),out.size(),out.begin());
    return out;
}
std::uint32_t SamplePayload::plane_count() const noexcept { return word(8); }
std::uint32_t SamplePayload::rate_value() const noexcept { return word(12); }
std::uint32_t SamplePayload::bytes_per_plane() const noexcept { return word(16); }
PayloadBytes SamplePayload::encoded_payload() const noexcept {
    if (bytes_.size()<20) return {};
    return {bytes_.data()+20,bytes_.size()-20};
}
template<class File> std::optional<SamplePayload> SamplePayload::read_opened(File& file,const ResourceLookup& resource,PayloadFailure& failure) {
    failure={};
    auto fail=[&](PayloadError kind) -> std::optional<SamplePayload> { failure.kind=kind; return std::nullopt; };
    if (resource.status!=ResourceLookupStatus::found) return fail(PayloadError::resource_not_found);
    // Named records store a string offset here; relocation changes its
    // meaning. Raw bit20000000 alone never proves the numeric extent route.
    if (resource.key_mode!=ResourceKeyMode::numeric) return fail(PayloadError::unsupported_key_mode);
    // MSL2F618..2F62C selects the separate2E79C creation route first.
    if (u32(resource.bytes,0x20)&1u) return fail(PayloadError::unsupported_alternate_streaming);
    const std::uint64_t offset=u32(resource.bytes,0x10), extent=u32(resource.bytes,0x14);
    // MSL2DE74/2DEE8/2DEF8: this bounded view requires key bit20000000
    // and nonzero resource+10; other keys use an unresolved lookup route.
    if (!(u32(resource.bytes,8)&0x20000000u) || !offset) return fail(PayloadError::unsupported_key_lookup_route);
    std::uint64_t saved=0;
    if (!file_position(file,saved,failure)) return fail(PayloadError::io_error);
    auto restore=[&](std::optional<SamplePayload> value) -> std::optional<SamplePayload> {
        PayloadFailure restore_failure;
        if (saved>std::uint64_t(INT64_MAX) || !file_seek(file,static_cast<std::int64_t>(saved),restore_failure)) {
            failure=restore_failure; return fail(PayloadError::cursor_restore_failed);
        }
        return value;
    };
    std::uint64_t actual_length=0;
    if (!file_length(file,actual_length,failure)) return restore(fail(PayloadError::io_error));
    if (offset>actual_length || extent>actual_length-offset) return restore(fail(PayloadError::extent_out_of_bounds));
    if (extent<20) return restore(fail(PayloadError::truncated_header));
    if (!file_seek(file,static_cast<std::int64_t>(offset),failure)) return restore(fail(PayloadError::io_error));
    std::array<std::uint8_t,20> header{};
    const auto first=file_read(file,header.data(),header.size(),failure);
    if (first.error || first.bytes!=header.size()) return restore(fail(PayloadError::io_error));
    const auto count=u32(header,8),rate=u32(header,12),stride=u32(header,16);
    //2DFB8 truncates count to16 bits,2DFD8 loads signed16. Support only
    // canonical positive encodings, never silently mask malformed metadata.
    if (count>32767) return restore(fail(PayloadError::unsupported_count_encoding));
    if (!count) return restore(fail(PayloadError::unsupported_zero_count));
    if (!rate) return restore(fail(PayloadError::unsupported_zero_rate));
    if (rate!=u32(resource.bytes,12)) return restore(fail(PayloadError::unsupported_rate_mismatch));
    if (!stride) return restore(fail(PayloadError::unsupported_zero_plane_size));
    // Source per-plane count/length loop, corroborated by all8591 inspected
    // normal records. No allocation is made until checked composition agrees.
    if ((extent-20)%stride || (extent-20)/stride!=count) return restore(fail(PayloadError::inconsistent_extent));
    SamplePayload result;
    if (extent>result.bytes_.max_size()) return restore(fail(PayloadError::host_capacity));
    try { result.bytes_.resize(static_cast<std::size_t>(extent)); }
    catch (const std::bad_alloc&) { return restore(fail(PayloadError::allocation_failed)); }
    catch (const std::length_error&) { return restore(fail(PayloadError::host_capacity)); }
    std::copy(header.begin(),header.end(),result.bytes_.begin());
    const auto payload=file_read(file,result.bytes_.data()+20,result.bytes_.size()-20,failure);
    if (payload.error || payload.bytes!=result.bytes_.size()-20) return restore(fail(PayloadError::io_error));
    return restore(std::optional<SamplePayload>(std::move(result)));
}
std::optional<SamplePayload> SamplePayload::read(content::NativeFile& file,const ResourceLookup& resource,PayloadFailure& failure) {
    return read_opened(file,resource,failure);
}
std::optional<SamplePayload> SamplePayload::read(content::NativeContentFile& file,const ResourceLookup& resource,PayloadFailure& failure) {
    return read_opened(file,resource,failure);
}
std::optional<SamplePayload> SamplePayload::read(const std::filesystem::path& path,const ResourceLookup& resource,PayloadFailure& failure) {
    content::NativeFile file; std::error_code error;
    if (!file.open(path,error)) { failure={PayloadError::io_error,error}; return std::nullopt; }
    return read(file,resource,failure);
}
} // namespace mka::sound
