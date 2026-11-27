#include "mka/retail_ssf_texture.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <new>
#include <stdexcept>

namespace mka::retail {
namespace {
struct Decoder {
    std::span<const std::byte> bytes;
    RetailSsfTextureFailure& failure;
    bool fail(RetailSsfTextureError e,const char* op) noexcept {failure={e,op};return false;}
    bool range(std::uint64_t at,std::uint64_t count) noexcept {
        return (at<=bytes.size()&&count<=bytes.size()-at)||
            fail(RetailSsfTextureError::truncated,"SSF texture serialized range");
    }
    std::uint32_t word(std::size_t at) const noexcept {
        return std::to_integer<std::uint32_t>(bytes[at])|
            (std::to_integer<std::uint32_t>(bytes[at+1])<<8)|
            (std::to_integer<std::uint32_t>(bytes[at+2])<<16)|
            (std::to_integer<std::uint32_t>(bytes[at+3])<<24);
    }
    bool chunk(std::size_t at,std::uint32_t kind,std::uint64_t length) noexcept {
        if(!range(at,12))return false;
        if(word(at)!=kind||word(at+4)!=length)
            return fail(RetailSsfTextureError::malformed_chunk,"SSF texture unsupported chunk type/length");
        if(word(at+8)!=0x1803ffff)
            return fail(RetailSsfTextureError::unsupported_version,"SSF texture unproved RenderWare stamp");
        return range(at+12,length);
    }
    bool transfer(std::size_t at,std::uint32_t width,std::uint32_t height,
        std::uint32_t position,std::uint64_t payload_bytes) noexcept {
        if(payload_bytes%16||payload_bytes/16>0x7fff)
            return fail(RetailSsfTextureError::unsupported_transfer,"SSF texture one serialized image-transfer extent");
        if(!range(at,80)||!range(at+80,payload_bytes))return false;
        const std::array<std::uint32_t,20> expected={3,0x10000000,14,0,
            0,position,0x51,0,width,height,0x52,0,0,0,0x53,0,
            static_cast<std::uint32_t>(payload_bytes/16),0x08000000,0,0};
        for(std::size_t i=0;i<expected.size();++i)
            if(word(at+4*i)!=expected[i])
                return fail(RetailSsfTextureError::unsupported_transfer,"SSF texture unsupported serialized transfer prefix");
        return true;
    }
};
// Serialized format permutations shared with the separately verified system-
// art codec (GSmanualv6 pp164/166/167). No device storage or register model.
constexpr unsigned block[4][8]={{0,1,4,5,16,17,20,21},{2,3,6,7,18,19,22,23},
    {8,9,12,13,24,25,28,29},{10,11,14,15,26,27,30,31}};
constexpr unsigned word[2][2][8]={{{0,1,4,5,8,9,12,13},{2,3,6,7,10,11,14,15}},
    {{8,9,12,13,0,1,4,5},{10,11,14,15,2,3,6,7}}};
}

std::optional<RetailSsfTextureData> decode_retail_ssf_texture(
    std::span<const std::byte> bytes,std::uint64_t section_offset,
    RetailSsfTextureFailure& failure) noexcept try {
    failure={};Decoder d{bytes,failure};
    if(!d.range(0,5))return {};
    const auto name_bytes=std::to_integer<std::uint8_t>(bytes[4]);
    if(!name_bytes||!d.range(5,std::uint64_t(name_bytes)+8)){
        if(failure.kind==RetailSsfTextureError::none)
            d.fail(RetailSsfTextureError::malformed_wrapper,"SSF texture empty wrapper name");
        return {};
    }
    RetailSsfTextureData out;
    out.name.reserve(name_bytes);
    for(std::size_t i=0;i<name_bytes;++i){
        const auto c=std::to_integer<unsigned char>(bytes[5+i]);
        if(c<0x20||c>0x7e){d.fail(RetailSsfTextureError::malformed_wrapper,"SSF texture wrapper name is not bounded ASCII");return {};}
        out.name.push_back(static_cast<char>(c));
    }
    const auto rw=std::size_t(5)+name_bytes+8;
    out.width=d.word(5+name_bytes);out.height=d.word(9+name_bytes);
    if(!d.chunk(rw,1,8))return {};
    if(d.word(rw+12)!=0x00325350){d.fail(RetailSsfTextureError::unsupported_raster_profile,"SSF texture platform is not PS2 native");return {};}
    const auto raw_sampler=d.word(rw+16);
    if(raw_sampler!=0x1101){d.fail(RetailSsfTextureError::unsupported_raster_profile,"SSF texture unproved filter/address profile");return {};}
    if(!d.chunk(rw+20,2,4)||!d.chunk(rw+36,2,4)||!d.range(rw+52,12))return {};
    if(bytes[rw+32]!=std::byte{0}||bytes[rw+48]!=std::byte{0}){
        d.fail(RetailSsfTextureError::unsupported_raster_profile,"SSF texture nonempty internal name/mask profile");return {};
    }
    if(!d.chunk(rw+64,1,64))return {};
    const auto header=rw+76,data=header+64;
    std::array<std::uint32_t,16> fields{};
    for(std::size_t i=0;i<fields.size();++i)fields[i]=d.word(header+i*4);
    if(!out.width||!out.height||out.width!=fields[0]||out.height!=fields[1]||
        fields[2]!=8||fields[3]!=0x22504||fields[7]!=0){
        d.fail(RetailSsfTextureError::unsupported_raster_profile,"SSF texture requires proved single-mip mode2 PAL8 raster profile");return {};
    }
    // Serialized TEX0 PSM20..25/CPSM51..54/CSM55 select this PAL8/CT32-CLUT
    // permutation. They never select a host address or instantiate a device.
    if(((fields[4]>>20)&63u)!=0x13u||((fields[5]>>19)&15u)!=0u||
        (fields[5]&(1u<<23))!=0u){
        d.fail(RetailSsfTextureError::unsupported_raster_profile,"SSF texture requires PSMT8 and CSM1 CT32 palette storage");return {};
    }
    const std::uint64_t transfer_extents=std::uint64_t(fields[12])+fields[13];
    if(transfer_extents+0xc0<rw||!d.chunk(rw+52,1,transfer_extents+0xc0-rw)||
        !d.chunk(data,1,transfer_extents)||!d.range(data+12,12))return {};
    const auto backing=d.word(data+12),pixel_offset=d.word(data+16),palette_offset=d.word(data+20);
    const auto cursor=std::uint64_t(data)+24;
    if(section_offset>std::numeric_limits<std::uint64_t>::max()-cursor-127){
        d.fail(RetailSsfTextureError::invalid_extent,"SSF texture serialized alignment overflow");return {};
    }
    const auto base=((section_offset+cursor+127)&~std::uint64_t(127))-section_offset;
    if(!d.range(base,backing)||pixel_offset>backing||palette_offset>backing||
        backing-pixel_offset<80||backing-palette_offset<1104){
        if(failure.kind==RetailSsfTextureError::none)d.fail(RetailSsfTextureError::invalid_extent,"SSF texture backing/header extents");return {};
    }
    out.backing_offset=static_cast<std::size_t>(base);out.backing_bytes=backing;
    out.pixel_header=out.backing_offset+pixel_offset;out.palette_header=out.backing_offset+palette_offset;
    const auto transfer_width=d.word(out.pixel_header+32),transfer_height=d.word(out.pixel_header+36);
    const auto palette_position=d.word(out.palette_header+20);
    const std::uint64_t transfer_pixels=std::uint64_t(transfer_width)*transfer_height;
    if(!transfer_width||!transfer_height||transfer_pixels>backing/4||
        transfer_pixels*4>backing-pixel_offset-80||out.pixel_header+80+transfer_pixels*4>out.palette_header||
        (palette_position&0x000f000fu)){
        d.fail(RetailSsfTextureError::invalid_extent,"SSF texture image/palette transfer regions overlap or exceed backing");return {};
    }
    if(!d.transfer(out.pixel_header,transfer_width,transfer_height,0,transfer_pixels*4)||
        !d.transfer(out.palette_header,16,16,palette_position,1024))return {};
    const std::uint64_t pixels=std::uint64_t(out.width)*out.height;
    if(pixels>transfer_pixels*4||pixels>out.indices.max_size()||pixels>out.rgba_raw_alpha.max_size()/4){
        d.fail(RetailSsfTextureError::invalid_extent,"SSF texture logical pixels exceed serialized or host capacity");return {};
    }
    out.indices.resize(static_cast<std::size_t>(pixels));out.rgba_raw_alpha.resize(static_cast<std::size_t>(pixels*4));
    std::array<unsigned,2048> inverse{};
    for(unsigned y=0;y<32;++y)for(unsigned x=0;x<64;++x){
        const auto a=block[y/8][x/8]*256+((y&7)/2)*64+word[0][y&1][x&7]*4;
        inverse[a/4]=y*64+x;
    }
    for(std::uint32_t y=0;y<out.height;++y)for(std::uint32_t x=0;x<out.width;++x){
        const auto lx=x%128,ly=y%64,wx=lx%16,wy=ly%16,col=wy/4,row=wy%4;
        const auto a=block[ly/16][lx/16]*256+col*64+word[((row&2)/2)^(col&1)][row&1][wx&7]*4+(wx&8)/4+(row&2)/2;
        const auto xy=inverse[a/4],cx=xy%64,cy=xy/64;
        const std::uint64_t source=(((std::uint64_t(y/64)*32+cy)*transfer_width+std::uint64_t(x/128)*64+cx)*4)+(a&3);
        if(source>=transfer_pixels*4){d.fail(RetailSsfTextureError::unsupported_transfer,"SSF texture format permutation outside transfer");return {};}
        const auto index=std::to_integer<std::uint8_t>(bytes[out.pixel_header+80+static_cast<std::size_t>(source)]);
        const auto at=std::size_t(y)*out.width+x;
        const auto palette_index=(index&~0x18u)|((index&8u)<<1)|((index&16u)>>1);
        out.indices[at]=index;
        for(unsigned c=0;c<4;++c)out.rgba_raw_alpha[at*4+c]=std::to_integer<std::uint8_t>(bytes[out.palette_header+80+palette_index*4+c]);
    }
    // Source3015E0 selects linear for a non-mip raster and preserves U/V.
    out.sampler_metadata=(raw_sampler&0xffffff00u)|2u;
    return out;
} catch(const std::bad_alloc&) {failure={RetailSsfTextureError::allocation,"SSF texture native allocation"};return {};}
  catch(const std::length_error&) {failure={RetailSsfTextureError::allocation,"SSF texture host capacity"};return {};}
  catch(...) {failure={RetailSsfTextureError::native_exception,"SSF texture native exception"};return {};}
} // namespace mka::retail
