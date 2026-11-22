#include "mka/font_atlas.hpp"
#include "font_atlas_variants.hpp"
#include <array>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace mka {
const char* font_atlas_error_name(FontAtlasError e) noexcept {
    switch(e) {
    case FontAtlasError::none:return "none";
    case FontAtlasError::truncated:return "truncated";
    case FontAtlasError::malformed_wrapper:return "malformed_wrapper";
    case FontAtlasError::malformed_chunk:return "malformed_chunk";
    case FontAtlasError::unsupported_variant:return "unsupported_variant";
    case FontAtlasError::invalid_transfer:return "invalid_transfer";
    case FontAtlasError::invalid_extent:return "invalid_extent";
    case FontAtlasError::allocation_failure:return "allocation_failure";
    }
    return "unknown_font_atlas_error";
}
namespace {
using Bytes=std::vector<std::uint8_t>;
bool range(const Bytes& b,std::size_t p,std::size_t n) { return p<=b.size() && n<=b.size()-p; }
std::uint32_t u32(const Bytes& b,std::size_t p) {
    return std::uint32_t(b[p]) | std::uint32_t(b[p+1])<<8 |
        std::uint32_t(b[p+2])<<16 | std::uint32_t(b[p+3])<<24;
}
struct Parser {
    const Bytes& b; FontAtlasFailure& f;
    bool fail(FontAtlasError e,const char* d) { f={e,d};return false; }
    bool need(std::size_t p,std::size_t n) {
        return range(b,p,n) || fail(FontAtlasError::truncated,"serialized atlas range");
    }
    bool chunk(std::size_t p,std::uint32_t type,std::uint32_t len) {
        if(!need(p,12))return false;
        if(u32(b,p)!=type || u32(b,p+4)!=len)
            return fail(FontAtlasError::malformed_chunk,"unexpected chunk type/length for supported mode2 variant");
        if(u32(b,p+8)!=0x1803ffff)
            return fail(FontAtlasError::unsupported_variant,"RenderWare version");
        return need(p+12,len);
    }
    bool transfer(std::size_t p,std::uint32_t w,std::uint32_t h,std::uint32_t qwords,std::uint32_t destination_position=0) {
        if(!need(p,80))return false;
        const std::array<std::uint32_t,20> expected={3,0x10000000,14,0,
            0,destination_position,0x51,0,w,h,0x52,0,0,0,0x53,0,qwords,0x08000000,0,0};
        for(std::size_t i=0;i<expected.size();++i)
            if(u32(b,p+4*i)!=expected[i])
                return fail(FontAtlasError::invalid_transfer,"CT32 upload prefix does not match supported serialized variant");
        return true;
    }
};
// GS manual v6 pp164/166/167: serialized format address tables only.
constexpr unsigned block[4][8]={{0,1,4,5,16,17,20,21},{2,3,6,7,18,19,22,23},
    {8,9,12,13,24,25,28,29},{10,11,14,15,26,27,30,31}};
constexpr unsigned word[2][2][8]={{{0,1,4,5,8,9,12,13},{2,3,6,7,10,11,14,15}},
    {{8,9,12,13,0,1,4,5},{10,11,14,15,2,3,6,7}}};
}
std::optional<FontAtlas> FontAtlas::decode(Bytes serialized,FontAtlasFailure& failure) {
    failure={};
    try {
        Parser p{serialized,failure};
        if(!p.need(0,5))return {};
        // First word is the original mutable-link placeholder; retain it, do not relocate.
        const std::size_t names=serialized[4];
        if(!p.need(5,names+8))return {};
        const std::size_t rw=5+names+8;
        const auto width=u32(serialized,5+names),height=u32(serialized,9+names);
        if(!p.chunk(rw,1,8))return {};
        if(u32(serialized,rw+12)!=0x00325350 || u32(serialized,rw+16)!=0x1101) {
            p.fail(FontAtlasError::unsupported_variant,"native texture platform/flags");return {};
        }
        if(!p.need(rw+52,12)||!p.chunk(rw+20,2,4)||!p.chunk(rw+36,2,4)||!p.chunk(rw+64,1,64))return {};
        const auto s=rw+76,d=s+64;
        if(!p.need(d,24))return {};
        std::array<std::uint32_t,16> fields{};for(unsigned i=0;i<16;++i)fields[i]=u32(serialized,s+4*i);
        const font_data_detail::Variant* variant=nullptr;
        for(const auto& v:font_data_detail::system_art_variants)if(v.fields==fields){variant=&v;break;}
        if(!variant||width!=fields[0]||height!=fields[1]){
            p.fail(FontAtlasError::unsupported_variant,"unproved serialized system-art raster variant");return {};
        }
        if(!p.chunk(rw+52,1,fields[12]+fields[13]+0xc0-std::uint32_t(rw))||!p.chunk(d,1,fields[12]+fields[13]))return {};
        const auto backing_bytes=u32(serialized,d+12);
        if(backing_bytes!=variant->backing[0]||u32(serialized,d+16)!=variant->backing[1]||u32(serialized,d+20)!=variant->backing[2]){
            p.fail(FontAtlasError::invalid_extent,"mode2 serialized backing/transfer extents");return {};
        }
        const std::size_t cursor=d+24;
        if(cursor>std::numeric_limits<std::size_t>::max()-127) {
            p.fail(FontAtlasError::invalid_extent,"alignment overflow");return {};
        }
        const std::size_t base=(cursor+127)&~std::size_t(127);
        if(!p.need(base,backing_bytes))return {};
        if(serialized.size()!=variant->serialized_size || backing_bytes>serialized.size()-base) {
            p.fail(FontAtlasError::invalid_extent,"unexplained bytes beyond retained backing");return {};
        }
        // 002297F0: two 64-byte header records; align their end plus 0x8f.
        // Palette header follows the 128-byte-aligned pixel transfer extent.
        // The two serialized prefix words are corroboration, not blindly
        // interpreted as arbitrary offset fields for other variants.
        const auto pixel_header=base+variant->backing[1];
        const auto palette_header=base+variant->backing[2];
        const auto pixel_bytes=std::size_t(variant->transfer_width)*variant->transfer_height*4;
        if(!p.transfer(pixel_header,variant->transfer_width,variant->transfer_height,std::uint32_t(pixel_bytes/16))||
           !p.transfer(palette_header,16,16,0x40,variant->palette_position))return {};
        const auto pixels=pixel_header+80,palette=palette_header+80;
        if(!p.need(pixels,pixel_bytes)||!p.need(palette,1024))return {};
        FontAtlas result;result.width_=width;result.height_=height;
        result.source_=std::move(serialized);
        result.indices_.resize(std::size_t(width)*height);result.rgba_.resize(std::size_t(width)*height*4);
        // Inverse per-page CT32 word coordinates. This is a format permutation,
        // not an allocation indexed by guest addresses or a simulated device.
        std::array<unsigned,2048> inverse{};
        for(unsigned y=0;y<32;++y)for(unsigned x=0;x<64;++x) {
            auto a=block[y/8][x/8]*256+((y&7)/2)*64+word[0][y&1][x&7]*4;
            inverse[a/4]=y*64+x;
        }
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
            const unsigned page_x=x/128,page_y=y/64,lx=x%128,ly=y%64;
            const unsigned wx=lx%16,wy=ly%16,col=wy/4,row=wy%4;
            const auto a=block[ly/16][lx/16]*256+col*64+
                word[((row&2)/2)^(col&1)][row&1][wx&7]*4+(wx&8)/4+(row&2)/2;
            const auto xy=inverse[a/4],cx=xy%64,cy=xy/64;
            const auto source=(((page_y*32+cy)*variant->transfer_width+page_x*64+cx)*4)+(a&3);
            if(source>=pixel_bytes){failure={FontAtlasError::invalid_transfer,"serialized pixel mapping outside transfer"};return {};}
            const auto i=result.source_[pixels+source];
            const auto output=y*width+x;
            result.indices_[output]=i;
            const auto j=(i&~0x18u)|((i&8u)<<1)|((i&16u)>>1);
            for(unsigned c=0;c<4;++c)result.rgba_[output*4+c]=result.source_[palette+j*4+c];
        }
        return result;
    } catch(const std::bad_alloc&) { failure={FontAtlasError::allocation_failure,"owned atlas allocation"}; }
      catch(const std::length_error&) { failure={FontAtlasError::allocation_failure,"owned atlas size"}; }
    return {};
}
} // namespace mka
