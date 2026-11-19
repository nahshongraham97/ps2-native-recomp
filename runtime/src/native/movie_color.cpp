#include "mka/movie_color.hpp"
#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#endif

namespace mka::video {
std::optional<NativeMovieColorTable> NativeMovieColorTable::decode(MovieColorRole role,const std::uint8_t* bytes,std::size_t size,MovieColorError& f){
    f=MovieColorError::none;
    if(role!=MovieColorRole::helper_default&&role!=MovieColorRole::fresh_preset21){f=MovieColorError::missing_table_role;return std::nullopt;}
    if(!bytes||size!=6144){f=MovieColorError::invalid_table_bytes;return std::nullopt;}
#ifdef _WIN32
    std::array<unsigned char,32> digest{};
    struct HashState {
        BCRYPT_ALG_HANDLE algorithm=nullptr;
        BCRYPT_HASH_HANDLE hash=nullptr;
        std::vector<unsigned char> object;
        ~HashState(){if(hash)BCryptDestroyHash(hash);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);}
    } state;
    ULONG object_bytes=0,used=0;
    if(BCryptOpenAlgorithmProvider(&state.algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0||
       BCryptGetProperty(state.algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&object_bytes),sizeof(object_bytes),&used,0)<0){f=MovieColorError::verification_unavailable;return std::nullopt;}
    try{state.object.resize(object_bytes);}catch(const std::bad_alloc&){f=MovieColorError::host_capacity;return std::nullopt;}catch(const std::length_error&){f=MovieColorError::host_capacity;return std::nullopt;}
    if(BCryptCreateHash(state.algorithm,&state.hash,state.object.data(),object_bytes,nullptr,0,0)<0||
       BCryptHashData(state.hash,const_cast<unsigned char*>(bytes),static_cast<ULONG>(size),0)<0||
       BCryptFinishHash(state.hash,digest.data(),digest.size(),0)<0){f=MovieColorError::verification_unavailable;return std::nullopt;}
    const char* expected=role==MovieColorRole::helper_default?
        "b08ea2cc7202e4e8911b04569f476b858925e61475c6f1031890d6c42102d4f4":
        "a183204e91cd28728d57915127d966469ca2da523b5ae4b54b8002fd339742a8";
    constexpr char hex[]="0123456789abcdef";
    for(std::size_t i=0;i<digest.size();++i)if(hex[digest[i]>>4]!=expected[i*2]||hex[digest[i]&15]!=expected[i*2+1]){f=MovieColorError::table_identity_mismatch;return std::nullopt;}
#else
    f=MovieColorError::verification_unavailable;return std::nullopt;
#endif
    NativeMovieColorTable result;result.role_=role;
    for(std::size_t i=0;i<result.values_.size();++i){const unsigned raw=unsigned(bytes[i*2])|(unsigned(bytes[i*2+1])<<8);result.values_[i]=static_cast<std::int16_t>(raw<32768?int(raw):int(raw)-65536);}
    return result;
}
std::array<std::uint8_t,4> NativeMovieColorTable::pixel(std::uint8_t y,std::uint8_t u,std::uint8_t v)const noexcept{
    std::array<std::uint8_t,4> result;
    for(unsigned c=0;c!=4;++c){
        const int uv=std::clamp(int(values_[1024+unsigned(u)*4+c])+int(values_[2048+unsigned(v)*4+c]),-32768,32767);
        const int sum=std::clamp(int(values_[unsigned(y)*4+c])+uv,-32768,32767);
        // Explicit floor division reproduces the original arithmetic shift
        // for negative values without relying on signed implementation details.
        const int shifted=sum>=0?sum/64:-((-sum+63)/64);
        result[c]=static_cast<std::uint8_t>(std::clamp(shifted,0,255));
    }
    return result;
}
std::optional<MovieRgbaFrame> convert_movie_yuv420(const MovieVideoFrame& input,const NativeMovieColorTable& table,MovieColorError& f){
    f=MovieColorError::none;
    if(table.role()==MovieColorRole::unspecified){f=MovieColorError::missing_table_role;return std::nullopt;}
    const std::size_t w=input.width,h=input.height,cw=(w+1)/2,ch=(h+1)/2;
    if(!w||!h||h>SIZE_MAX/w||ch>SIZE_MAX/cw||w*h>SIZE_MAX/4||
       input.planes[0].size()!=w*h||input.planes[1].size()!=cw*ch||input.planes[2].size()!=cw*ch){f=MovieColorError::invalid_frame;return std::nullopt;}
    try{
        MovieRgbaFrame output;output.width=input.width;output.height=input.height;output.source_pts=input.source_pts;output.timestamp=input.timestamp;
        output.source_sample_aspect_numerator=input.sample_aspect_numerator;output.source_sample_aspect_denominator=input.sample_aspect_denominator;
        output.source_color_range=input.color_range;output.source_color_space=input.color_space;output.source_color_primaries=input.color_primaries;output.source_color_transfer=input.color_transfer;
        output.rgba.resize(w*h*4);
        for(std::size_t y=0;y<h;++y)for(std::size_t x=0;x<w;++x){
            const auto chroma=(y/2)*cw+x/2;
            const auto pixel=table.pixel(input.planes[0][y*w+x],input.planes[1][chroma],input.planes[2][chroma]);
            std::copy(pixel.begin(),pixel.end(),output.rgba.begin()+(y*w+x)*4);
        }
        return output;
    }catch(const std::bad_alloc&){f=MovieColorError::host_capacity;}catch(const std::length_error&){f=MovieColorError::host_capacity;}
    return std::nullopt;
}
} // namespace mka::video
