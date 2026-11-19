#include "mka/movie_adx_blocks.hpp"
#include <cstdio>
#include <array>
#include <string_view>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <utility>
using namespace mka::video;
namespace {bool fail_next=false;}
void* operator new(std::size_t n){if(std::exchange(fail_next,false))throw std::bad_alloc();if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}
void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
std::vector<std::uint8_t> block(std::uint16_t scale,std::uint8_t packed){std::vector<std::uint8_t> b(18,packed);b[0]=scale>>8;b[1]=scale&255;return b;}
int main(int argc,char** argv){try{
    check(argc==1||(argc==2&&std::string_view(argv[1])=="--dump-plan"),"usage: test_native_movie_adx_blocks [--dump-plan]");
    AdxBlockFailure f;AdxBlockProfile plain(AdxBlockMode::version4_flags0,{{0,0}},{{0,0}});
    auto raw=block(0,0x1f);auto r=decode_movie_adx_blocks(raw,plain,f);check(r&&r->frames==32&&r->planes[0][0]==1&&r->planes[0][1]==-1&&r->final_histories[0]==std::pair<std::int16_t,std::int16_t>{-1,1},"scale zero means one; high then signed low");
    auto mask=decode_movie_adx_blocks(block(0x6000,0x1f),plain,f);check(mask&&mask->planes==r->planes,"bits13/14 masked before plusone");
    AdxBlockProfile seeded(AdxBlockMode::version4_flags0,{{4096,0}},{{10,-4}});auto s=decode_movie_adx_blocks(block(0,0x10),seeded,f);check(s&&s->planes[0][0]==11&&s->planes[0][1]==11&&s->final_histories[0].first==26&&seeded.histories[0].first==10,"explicit history every sample, immutable profile");
    AdxBlockProfile floor(AdxBlockMode::version4_flags0,{{-1,0}},{{1,0}});auto floor_result=decode_movie_adx_blocks(block(0,0),floor,f);check(floor_result&&floor_result->planes[0][0]==-1&&floor_result->planes[0][1]==0,"negative predictor floor distinct from truncation");
    auto clip=decode_movie_adx_blocks(block(0x1fff,0x78),plain,f);check(clip&&clip->planes[0][0]==32767&&clip->planes[0][1]==-32768,"individual signed16 saturation");
    auto first=block(0,0x10),second=block(0,0xf0);auto together=first;together.insert(together.end(),second.begin(),second.end());auto both=decode_movie_adx_blocks(together,seeded,f);AdxBlockProfile follow(AdxBlockMode::version4_flags0,seeded.coefficients,s->final_histories);auto next=decode_movie_adx_blocks(second,follow,f);check(both&&next&&both->final_histories==next->final_histories,"packet continuation histories");for(std::size_t i=0;i<32;++i)check(both->planes[0][i]==s->planes[0][i]&&both->planes[0][i+32]==next->planes[0][i],"split versus complete continuity");
    AdxBlockProfile two(AdxBlockMode::version4_flags0,{{0,0},{4096,0}},{{0,0},{100,0}});auto paired=first;paired.insert(paired.end(),second.begin(),second.end());auto stereo=decode_movie_adx_blocks(paired,two,f);check(stereo&&stereo->planes.size()==2&&stereo->planes[0][0]==1&&stereo->planes[1][0]==99&&stereo->planes[1][1]==99,"group plane ordinal and separate history");
    auto marker=together;marker[18]=0x80;check(!decode_movie_adx_blocks(marker,seeded,f)&&f.kind==AdxBlockError::marker&&seeded.histories[0].first==10&&s->planes[0][0]==11,"marker after valid group publishes no partial result/history");raw.pop_back();check(!decode_movie_adx_blocks(raw,plain,f)&&f.kind==AdxBlockError::invalid_extent,"truncated block");check(!decode_movie_adx_blocks({},plain,f),"empty input");AdxBlockProfile unknown(AdxBlockMode::unspecified,{{0,0}},{{0,0}});check(!decode_movie_adx_blocks(first,unknown,f)&&f.kind==AdxBlockError::unsupported_profile,"no default scale mode");AdxBlockProfile mismatch(AdxBlockMode::version4_flags0,{{0,0}},{});check(!decode_movie_adx_blocks(first,mismatch,f)&&f.kind==AdxBlockError::invalid_profile,"no default history");
    fail_next=true;check(!decode_movie_adx_blocks(first,plain,f)&&f.kind==AdxBlockError::allocation,"actual allocation failure");
    AdxBlockProfile dynamic(AdxBlockMode::version4_flags0,std::vector<std::pair<std::int16_t,std::int16_t>>(65,{0,0}),std::vector<std::pair<std::int16_t,std::int16_t>>(65,{0,0}));std::vector<std::uint8_t> many(18*65*17,0);auto large=decode_movie_adx_blocks(many,dynamic,f);check(large&&large->planes.size()==65&&large->frames==544&&large->planes[64][543]==0,"dynamic planes and groups beyond transport sizes");auto owned=std::move(*large);large.reset();check(owned.final_histories.size()==65,"owned result lifetime");
    if(argc==2){
        constexpr std::array<std::uint16_t,5> scales{0,1,0x1fff,0x2000,0x7fff};
        constexpr std::array<std::pair<std::int16_t,std::int16_t>,5> coeffs{{{0,0},{4096,0},{0,4096},{1,-1},{7951,-3860}}};
        constexpr std::array<std::pair<std::int16_t,std::int16_t>,3> seeds{{{0,0},{-1,1},{32767,-32768}}};
        std::size_t id=0;
        const auto emit=[&](std::uint16_t scale,std::pair<std::int16_t,std::int16_t> coefficient,std::pair<std::int16_t,std::int16_t> seed,bool extension){
            auto encoded=block(scale,0);for(std::size_t i=0;i<16;++i)encoded[2+i]=static_cast<std::uint8_t>((2*(i%8)<<4)|(2*(i%8)+1));
            AdxBlockProfile profile(AdxBlockMode::version4_flags0,{coefficient},{seed});auto decoded=decode_movie_adx_blocks(encoded,profile,f);check(decoded&&f.kind==AdxBlockError::none,"declared trace public decode");
            std::printf("{\"case\":%zu,\"mode\":\"version4_flags0\",\"native_extension\":%s,\"scale\":%u,\"encoded_hex\":\"",id++,extension?"true":"false",unsigned(scale));for(auto byte:encoded)std::printf("%02x",unsigned(byte));std::printf("\",\"coefficients\":[[%d,%d]],\"histories\":[[%d,%d]],\"error\":\"none\",\"frames\":%zu,\"planes\":[[",int(coefficient.first),int(coefficient.second),int(seed.first),int(seed.second),decoded->frames);for(std::size_t i=0;i<decoded->frames;++i)std::printf("%s%d",i?",":"",int(decoded->planes[0][i]));std::printf("]],\"final_histories\":[[%d,%d]]}\n",int(decoded->final_histories[0].first),int(decoded->final_histories[0].second));
        };
        for(auto scale:scales)for(auto coefficient:coeffs)for(auto seed:seeds)emit(scale,coefficient,seed,false);
        emit(0,{-32768,-32768},{-32768,-32768},true);check(id==76,"complete fixed trace cardinality");
    }
    std::puts("PASS native explicit ADX block codec; no coefficient/header/mode admission");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL ADX blocks %s\n",e.what());return 1;}}
