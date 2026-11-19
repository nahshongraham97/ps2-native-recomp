#include "mka/movie_container.hpp"
#include <array>
#include <algorithm>
#include <new>
#include <stdexcept>
#include <utility>
namespace mka::video {
namespace {
bool fail(MovieContainerFailure& f,MovieContainerError kind,std::uint64_t at){f.kind=kind;f.offset=at;return false;}
struct Reader {
    NativeMovieAsset& asset;MovieContainerFailure& failure;std::uint64_t size=0,pos=0;
    bool take(void* data,std::size_t n){
        if(pos>size||n>size-pos)return fail(failure,MovieContainerError::truncated,pos);
        auto read=asset.read(data,n,failure.asset);if(read.error)return fail(failure,MovieContainerError::asset_io,pos);
        if(read.bytes!=n)return fail(failure,MovieContainerError::truncated,pos);
        pos+=n;return true;
    }
    bool skip(std::uint64_t end){
        if(end<pos||end>size)return fail(failure,MovieContainerError::extent_out_of_bounds,pos);
        const auto distance=end-pos;
        if(distance>INT64_MAX)return fail(failure,MovieContainerError::extent_overflow,pos);
        if(!asset.seek(static_cast<std::int64_t>(distance),content::SeekOrigin::current,failure.asset))return fail(failure,MovieContainerError::asset_io,pos);
        pos=end;return true;
    }
};
bool pack1(const std::array<unsigned char,8>& bytes){
    unsigned bit=0;
    const auto get=[&](unsigned count){unsigned v=0;while(count--){v=(v<<1)|((bytes[bit/8]>>(7-bit%8))&1);++bit;}return v;};
    if(get(4)!=2)return false;get(3);if(get(1)!=1)return false;get(15);if(get(1)!=1)return false;get(15);if(get(1)!=1||get(1)!=1)return false;get(22);return get(1)==1;
}
std::optional<NativeMovieContainer> scan(NativeMovieAsset& asset,std::uint64_t size,MovieContainerFailure& f){
    NativeMovieContainer result;result.file_bytes=size;Reader reader{asset,f,size,0};
    std::optional<std::pair<std::uint64_t,std::array<std::uint8_t,12>>> preceding_pack;
    while(reader.pos<size){
        auto previous=std::move(preceding_pack);preceding_pack.reset();
        const auto start=reader.pos;std::array<unsigned char,4> code;if(!reader.take(code.data(),code.size()))return {};
        if(code[0]||code[1]||code[2]!=1){fail(f,MovieContainerError::bad_prefix,start);return {};}
        if(!result.packet_count&&code[3]!=0xBA){fail(f,MovieContainerError::missing_pack_header,start);return {};}
        ++result.packet_count;
        if(code[3]==0xBA){
            std::array<unsigned char,8> header;if(!reader.take(header.data(),1))return {};
            if((header[0]&0xF0)!=0x20){fail(f,MovieContainerError::unsupported_pack,start);return {};}
            if(!reader.take(header.data()+1,7))return {};
            if(!pack1(header)){fail(f,MovieContainerError::bad_pack_marker,start);return {};}
            std::array<std::uint8_t,12> prefix{};std::copy(code.begin(),code.end(),prefix.begin());
            std::copy(header.begin(),header.end(),prefix.begin()+4);preceding_pack=std::make_pair(start,prefix);
            ++result.pack_count;
        }else if(code[3]==0xB9){
            result.program_end_offset=start;std::array<unsigned char,4096> tail;
            while(reader.pos<size){const auto n=static_cast<std::size_t>(std::min<std::uint64_t>(tail.size(),size-reader.pos));const auto at=reader.pos;if(!reader.take(tail.data(),n))return {};
                for(std::size_t i=0;i<n;++i)if(tail[i]!=0xFF){fail(f,MovieContainerError::non_ff_tail,at+i);return {};}
                result.trailing_ff_bytes+=n;
            }return result;
        }else if(code[3]==0xBB||code[3]==0xBE||code[3]==0xBF||(code[3]>=0xC0&&code[3]<=0xEF)){
            std::array<unsigned char,2> length;if(!reader.take(length.data(),2))return {};
            const auto n=static_cast<std::uint16_t>((length[0]<<8)|length[1]);
            if(!n){fail(f,MovieContainerError::unsupported_zero_length,start);return {};}
            if(code[3]==0xBB&&n<6){fail(f,MovieContainerError::bad_system_header,start);return {};}
            const auto end=movie_container_packet_extent(start,n,size,f);if(!end)return {};
            if(code[3]==0xBF){
                MoviePrivate2Payload payload;payload.packet_offset=start;payload.payload_offset=reader.pos;
                if(previous){
                    MoviePrivate2Prefix prefix;prefix.file_offset=previous->first;
                    std::copy(previous->second.begin(),previous->second.end(),prefix.bytes.begin());
                    std::copy(code.begin(),code.end(),prefix.bytes.begin()+12);
                    std::copy(length.begin(),length.end(),prefix.bytes.begin()+16);
                    payload.adjacent_mpeg1_prefix=std::move(prefix);
                }
                payload.bytes.resize(n);if(!reader.take(payload.bytes.data(),n))return {};result.private2.push_back(std::move(payload));
            }
            else if(!reader.skip(*end))return {};
        }else{fail(f,MovieContainerError::unsupported_packet,start);return {};}
    }fail(f,MovieContainerError::missing_program_end,reader.pos);return {};
}
}
std::optional<std::uint64_t> movie_container_packet_extent(std::uint64_t offset,std::uint16_t length,std::uint64_t size,MovieContainerFailure& f)noexcept{
    f={};if(offset>UINT64_MAX-6||length>UINT64_MAX-offset-6){fail(f,MovieContainerError::extent_overflow,offset);return {};}
    const auto end=offset+6+length;if(end>size){fail(f,MovieContainerError::extent_out_of_bounds,offset);return {};}return end;
}
std::optional<NativeMovieContainer> extract_movie_private2(NativeMovieAsset& asset,MovieContainerFailure& f){
    f={};auto position=asset.position(f.asset);if(!position){fail(f,MovieContainerError::asset_io,0);return {};}
    if(*position>INT64_MAX){fail(f,MovieContainerError::extent_overflow,*position);return {};}
    std::optional<NativeMovieContainer> result;
    try{
        auto size=asset.length(f.asset);
        if(!size)fail(f,MovieContainerError::asset_io,*position);
        else if(!asset.seek(0,content::SeekOrigin::begin,f.asset))fail(f,MovieContainerError::asset_io,*position);
        else result=scan(asset,*size,f);
    }catch(const std::bad_alloc&){fail(f,MovieContainerError::allocation,0);}
     catch(const std::length_error&){fail(f,MovieContainerError::host_capacity,0);}
    MovieAssetFailure restore;
    if(!asset.seek(static_cast<std::int64_t>(*position),content::SeekOrigin::begin,restore)){
        const auto before=f.kind;f={MovieContainerError::cursor_restore,*position,restore,before};return {};
    }
    return result;
}
} // namespace mka::video
