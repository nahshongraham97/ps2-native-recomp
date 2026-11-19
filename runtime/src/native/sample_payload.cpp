#include "mka/sample_payload.hpp"
#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>

namespace mka::sound {
namespace {
template<class Bytes> std::uint32_t u32(const Bytes& b, std::size_t at) {
    return std::uint32_t(b[at]) | (std::uint32_t(b[at+1])<<8) |
           (std::uint32_t(b[at+2])<<16) | (std::uint32_t(b[at+3])<<24);
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
std::optional<SamplePayload> SamplePayload::read(content::NativeFile& file,const ResourceLookup& resource,PayloadFailure& failure) {
    failure={};
    auto fail=[&](PayloadError kind,std::error_code ec={}) -> std::optional<SamplePayload> { failure={kind,ec}; return std::nullopt; };
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
    std::uint64_t saved=0; std::error_code error;
    if (!file.position(saved,error)) return fail(PayloadError::io_error,error);
    auto restore=[&](std::optional<SamplePayload> value) -> std::optional<SamplePayload> {
        std::error_code restore_error;
        if (saved>std::uint64_t(INT64_MAX) || !file.seek(static_cast<std::int64_t>(saved),content::SeekOrigin::begin,restore_error))
            return fail(PayloadError::cursor_restore_failed,restore_error);
        return value;
    };
    std::uint64_t length=0;
    if (!file.length(length,error)) return restore(fail(PayloadError::io_error,error));
    if (offset>length || extent>length-offset) return restore(fail(PayloadError::extent_out_of_bounds));
    if (extent<20) return restore(fail(PayloadError::truncated_header));
    if (!file.seek(static_cast<std::int64_t>(offset),content::SeekOrigin::begin,error)) return restore(fail(PayloadError::io_error,error));
    std::array<std::uint8_t,20> header{};
    const auto first=file.read(header.data(),header.size());
    if (first.error || first.bytes!=header.size()) return restore(fail(PayloadError::io_error,first.error));
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
    const auto payload=file.read(result.bytes_.data()+20,result.bytes_.size()-20);
    if (payload.error || payload.bytes!=result.bytes_.size()-20) return restore(fail(PayloadError::io_error,payload.error));
    return restore(std::optional<SamplePayload>(std::move(result)));
}
std::optional<SamplePayload> SamplePayload::read(const std::filesystem::path& path,const ResourceLookup& resource,PayloadFailure& failure) {
    content::NativeFile file; std::error_code error;
    if (!file.open(path,error)) { failure={PayloadError::io_error,error}; return std::nullopt; }
    return read(file,resource,failure);
}
} // namespace mka::sound
