#include "mka/movie_color.hpp"
#include <emmintrin.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace {
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
// Independent native SIMD reference for the literal signed-saturating lane
// sequence pinned in MOVIER7E7728: UV add, Y add, shift6, byte clamp/pack.
std::uint32_t instruction_reference(const std::uint8_t* raw,unsigned y,unsigned u,unsigned v){
    const auto Y=_mm_loadl_epi64(reinterpret_cast<const __m128i*>(raw+y*8));
    const auto U=_mm_loadl_epi64(reinterpret_cast<const __m128i*>(raw+2048+u*8));
    const auto V=_mm_loadl_epi64(reinterpret_cast<const __m128i*>(raw+4096+v*8));
    const auto result=_mm_packus_epi16(_mm_srai_epi16(_mm_adds_epi16(Y,_mm_adds_epi16(U,V)),6),_mm_setzero_si128());
    return static_cast<std::uint32_t>(_mm_cvtsi128_si32(result));
}
}
int main(int argc,char** argv){
    using namespace mka::video;
    try{
        check(argc==2,"usage: test_native_movie_color original_MOVIER.ovl");
        std::ifstream input(std::filesystem::u8path(argv[1]),std::ios::binary);check(bool(input),"open original overlay as data");
        std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
        MovieColorError f;
        check(!NativeMovieColorTable::decode(MovieColorRole::unspecified,bytes.data(),6144,f)&&f==MovieColorError::missing_table_role,"no guessed table selection");
        std::array<std::uint8_t,6144> zero{};
        check(!NativeMovieColorTable::decode(MovieColorRole::helper_default,zero.data(),zero.size(),f)&&f==MovieColorError::table_identity_mismatch,"arbitrary zero table cannot impersonate retail role");
        for(unsigned variant=0;variant!=2;++variant){
            const std::size_t offset=variant?0x795d8:0x77dd8;
            check(bytes.size()>=offset+6144,"original table extent");
            const auto* raw=bytes.data()+offset;const auto role=variant?MovieColorRole::fresh_preset21:MovieColorRole::helper_default;
            auto table=NativeMovieColorTable::decode(role,raw,6144,f);check(bool(table),"exact role/table SHA256 admission");
            check(!NativeMovieColorTable::decode(variant?MovieColorRole::helper_default:MovieColorRole::fresh_preset21,raw,6144,f)&&f==MovieColorError::table_identity_mismatch,"swapped table role rejects");
            check(!NativeMovieColorTable::decode(role,raw,6143,f)&&f==MovieColorError::invalid_table_bytes,"truncated table rejects");
            for(unsigned y=0;y!=256;++y)for(unsigned u=0;u!=256;++u)for(unsigned v=0;v!=256;++v){
                const auto actual=table->pixel(y,u,v);std::uint32_t packed;std::memcpy(&packed,actual.data(),4);
                check(packed==instruction_reference(raw,y,u,v),"every YUV triple matches independent original-operation SIMD reference");
            }
            MovieVideoFrame frame;frame.width=3;frame.height=3;frame.planes[0]={0,16,128,235,255,42,91,72,215};frame.planes[1]={0,64,128,255};frame.planes[2]={255,128,64,0};
            frame.timestamp={12345,1,90000,true};frame.source_pts={12000,1,90000,true};frame.sample_aspect_numerator=14;frame.sample_aspect_denominator=15;
            frame.color_range=1;frame.color_space=4;frame.color_primaries=5;frame.color_transfer=5;
            auto output=convert_movie_yuv420(frame,*table,f);check(output&&output->rgba.size()==36,"odd native-owned plane shape has no16-column transport limit");
            for(unsigned y=0;y!=3;++y)for(unsigned x=0;x!=3;++x){
                std::uint32_t actual;std::memcpy(&actual,output->rgba.data()+(y*3+x)*4,4);
                check(actual==instruction_reference(raw,frame.planes[0][y*3+x],frame.planes[1][(y/2)*2+x/2],frame.planes[2][(y/2)*2+x/2]),"nearest2x2 chroma and linear output layout");
            }
            check(output->conversion_role==role&&output->timestamp.value==12345&&output->source_pts.value==12000&&output->source_sample_aspect_numerator==14&&output->source_sample_aspect_denominator==15&&output->source_color_space==4,"conversion role, timing and source aspect/color provenance retained");
            const auto retained=output->rgba;frame.planes[0].clear();check(output->rgba==retained,"converted output owns pixels");
            check(!convert_movie_yuv420(frame,*table,f)&&f==MovieColorError::invalid_frame,"incomplete plane never produces guessed pixels");
            std::printf("[native-movie-color] table=%u all16777216 YUV triples match original SIMD arithmetic\n",variant);
        }
        std::puts("PASS exact retail table identity/arithmetic/chroma/layout/ownership; no live movie/presentation claim");return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
}
