#include "mka/movie_adx_header.hpp"
#include <algorithm>
#include <bit>
#include <new>
#include <stdexcept>
namespace mka::video {
namespace {
bool fail(MovieAdxHeaderFailure& f,MovieAdxHeaderError e,const char* operation) noexcept {
    f={e,operation};return false;
}
std::uint16_t be16(const std::uint8_t* p) noexcept {
    return static_cast<std::uint16_t>((std::uint16_t(p[0])<<8)|p[1]);
}
std::uint32_t be32(const std::uint8_t* p) noexcept {
    return (std::uint32_t(p[0])<<24)|(std::uint32_t(p[1])<<16)|
        (std::uint32_t(p[2])<<8)|p[3];
}
}
std::optional<MovieAdxHeader> parse_movie_adx_header(std::span<const std::uint8_t> bytes,
        MovieAdxHeaderFailure& f) noexcept {
    f={};
    if(bytes.size()<2){fail(f,MovieAdxHeaderError::incomplete,"ADX signature prefix incomplete");return {};}
    if(!bytes.data()){fail(f,MovieAdxHeaderError::invalid_field,"ADX nonempty input has no data");return {};}
    if(be16(bytes.data())!=0x8000){fail(f,MovieAdxHeaderError::signature,"ADX ES byte0 signature");return {};}
    if(bytes.size()<4){fail(f,MovieAdxHeaderError::incomplete,"ADX declared extent prefix incomplete");return {};}
    // Serialized BE16 +4, widened BEFORE addition. This is format data, not a
    // host capacity. History bytes end at32; six cookie bytes cannot overlap.
    const std::uint64_t extent=std::uint64_t(be16(bytes.data()+2))+4;
    if(extent<38){fail(f,MovieAdxHeaderError::extent,"ADX header must contain histories and terminal copyright");return {};}
    if(extent>bytes.size()){fail(f,MovieAdxHeaderError::incomplete,"ADX declared header extent incomplete");return {};}
    const auto size=static_cast<std::size_t>(extent); // extent <= bytes.size()
    constexpr std::array<std::uint8_t,6> cookie{'(','c',')','C','R','I'};
    if(!std::equal(cookie.begin(),cookie.end(),bytes.begin()+size-cookie.size())){
        fail(f,MovieAdxHeaderError::extent,"ADX terminal copyright at declared extent");return {};
    }
    if(bytes[4]!=3||bytes[5]!=18||bytes[6]!=4||bytes[7]!=2||bytes[18]!=4||bytes[19]!=0){
        fail(f,MovieAdxHeaderError::unsupported_profile,"ADX encoding/block/bits/planes/version/flags not admitted");return {};
    }
    const auto rate=be32(bytes.data()+8),samples=be32(bytes.data()+12);
    const auto cutoff=be16(bytes.data()+16);
    if(!rate||!samples||!cutoff){
        fail(f,MovieAdxHeaderError::invalid_field,"ADX positive sample rate/count/cutoff required");return {};
    }
    try {
        MovieAdxHeader result;
        result.raw_header.assign(bytes.begin(),bytes.begin()+size);
        result.data_start=extent;result.encoding=bytes[4];result.block_bytes=bytes[5];
        result.sample_bits=bytes[6];result.source_planes=bytes[7];
        result.sample_rate=rate;result.declared_samples=samples;result.cutoff=cutoff;
        result.version=bytes[18];result.flags=bytes[19];
        result.histories[0]={std::bit_cast<std::int16_t>(be16(bytes.data()+24)),
            std::bit_cast<std::int16_t>(be16(bytes.data()+26))};
        result.histories[1]={std::bit_cast<std::int16_t>(be16(bytes.data()+28)),
            std::bit_cast<std::int16_t>(be16(bytes.data()+30))};
        return result;
    }catch(const std::bad_alloc&){fail(f,MovieAdxHeaderError::allocation,"owned ADX header allocation");return {};}
    catch(const std::length_error&){fail(f,MovieAdxHeaderError::host_capacity,"owned ADX header vector extent");return {};}
}
} // namespace mka::video
