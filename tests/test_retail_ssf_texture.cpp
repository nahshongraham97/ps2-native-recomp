#include "mka/retail_ssf_texture.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <vector>

using namespace mka::retail;
namespace {
std::size_t checks{};
bool check(bool value,const char* message){
    ++checks;if(!value)std::fprintf(stderr,"SSF_TEXTURE_ASSERTION: %s\n",message);return value;
}
std::uint64_t hash(const std::vector<std::uint8_t>& data){
    std::uint64_t h=14695981039346656037ull;
    for(auto c:data)h=(h^c)*1099511628211ull;
    return h;
}
void word(std::vector<std::byte>& bytes,std::size_t at,std::uint32_t value){
    for(unsigned i=0;i<4;++i)bytes[at+i]=std::byte((value>>(i*8))&255);
}
}
int main(int argc,char** argv){
    if(argc!=2){std::fputs("SSF texture requires authenticated retail PAK path\n",stderr);return 2;}
    std::ifstream file(std::filesystem::path(argv[1]),std::ios::binary);
    // Pinned source tuple: member219,section0,record65. The bundle integration
    // test resolves this tuple through the actual parser; this independent CPU
    // test reads its authenticated original bytes directly, without a search.
    constexpr std::uint64_t source_at=1330399744,section_at=2337280;
    std::vector<std::byte> bytes(5888);
    file.seekg(source_at);file.read(reinterpret_cast<char*>(bytes.data()),bytes.size());
    if(!check(file.gcount()==static_cast<std::streamsize>(bytes.size()),"actual ST_Banner source tuple readable"))return 1;
    RetailSsfTextureFailure f;
    auto result=decode_retail_ssf_texture(bytes,section_at,f);
    if(!check(bool(result),"actual ST_Banner native mode2 decode"))return 1;
    if(!check(result->name=="ST_Banner"&&result->width==32&&result->height==128,
        "wrapper name and dimensions"))return 1;
    if(!check(result->sampler_metadata==0x1102&&result->indices.size()==4096&&result->rgba_raw_alpha.size()==16384,
        "source linear sampler and host image extents"))return 1;
    // Fixed source-backed expectation from the independent address-permutation
    // analysis, not produced by the component under test during this run.
    if(!check(hash(result->rgba_raw_alpha)==0xeeef66da6ee0a397ull,"all4096 original palette-derived RGBA texels"))return 1;
    const std::array<std::pair<std::size_t,std::array<std::uint8_t,4>>,5> pixels={{{0,{49,47,42,128}},
        {32,{69,68,61,128}},{2047,{152,156,88,128}},{2048,{152,156,88,128}},{4095,{75,82,70,128}}}};
    for(const auto& [at,value]:pixels)
        if(!check(std::equal(value.begin(),value.end(),result->rgba_raw_alpha.begin()+at*4),"fixed original texel/alpha retained"))return 1;
    std::printf("SSF_TEXTURE_READY name=ST_Banner width=32 height=128 raw_alpha=128\n");
    auto refused=[&](const std::vector<std::byte>& input,std::uint64_t offset,const char* name){
        auto bad=decode_retail_ssf_texture(input,offset,f);
        return check(!bad&&f.kind!=RetailSsfTextureError::none,name);
    };
    for(auto length:std::array<std::size_t,17>{0,1,4,5,13,21,22,33,41,73,85,97,161,185,255,513,5823}){
        auto cut=bytes;cut.resize(length);if(!refused(cut,section_at,"truncated bounded texture"))return 1;
    }
    const std::array<std::pair<std::size_t,std::uint32_t>,17> mutations={{{14,0},{26,0xffffffff},
        {34,0x00325351},{38,0x1100},{102,0xffffffff},{106,4},{110,0x12504},
        {114,0xd5008000},{118,0x20080005},{118,0x20800005},
        {126,4},{162,7},{174,0xffffffff},{178,0xffffffff},{182,0x100},
        {512,4},{512+64,0x7fff}}};
    for(const auto& [at,value]:mutations){auto changed=bytes;word(changed,at,value);if(!refused(changed,section_at,"malformed/unsupported native texture field"))return 1;}
    auto invalid_name=bytes;invalid_name[5]=std::byte{0};
    if(!refused(invalid_name,section_at,"wrapper embedded NUL refused"))return 1;
    if(!refused(bytes,std::numeric_limits<std::uint64_t>::max(),"serialized alignment overflow refused"))return 1;
    if(!refused(bytes,section_at+64,"incorrect serialized alignment refused"))return 1;
    // Data edits keep the format valid but must fail the full image expectation.
    auto planted=bytes;
    const auto first_index=result->indices.front();
    const auto first_palette=(first_index&~0x18u)|((first_index&8u)<<1)|((first_index&16u)>>1);
    planted[result->palette_header+80+first_palette*4]^=std::byte{1};
    auto altered=decode_retail_ssf_texture(planted,section_at,f);
    if(!check(altered&&hash(altered->rgba_raw_alpha)!=0xeeef66da6ee0a397ull,"planted palette-byte error changes expected image"))return 1;
    std::printf("PASS native SSF texture checks=%zu actual_texels=4096\n",checks);
    return 0;
}
