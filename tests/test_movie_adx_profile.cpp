#include "mka/movie_adx_profile.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <string_view>
#include <utility>
using namespace mka::video;
namespace {int allocations_before_failure=-1;}
void* operator new(std::size_t n){if(allocations_before_failure==0){allocations_before_failure=-1;throw std::bad_alloc();}if(allocations_before_failure>0)--allocations_before_failure;if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}
namespace {
void check(bool v,const char* s){if(!v)throw std::runtime_error(s);}
void put16(std::vector<std::uint8_t>& b,std::size_t i,std::uint16_t v){b[i]=v>>8;b[i+1]=v&255;}
void put32(std::vector<std::uint8_t>& b,std::size_t i,std::uint32_t v){b[i]=v>>24;b[i+1]=(v>>16)&255;b[i+2]=(v>>8)&255;b[i+3]=v&255;}
std::vector<std::uint8_t> bytes(std::uint32_t rate=48000,std::uint16_t cutoff=500){
    std::vector<std::uint8_t> b(40,0);put16(b,0,0x8000);put16(b,2,36);b[4]=3;b[5]=18;b[6]=4;b[7]=2;
    put32(b,8,rate);put32(b,12,2);put16(b,16,cutoff);b[18]=4;put16(b,24,1);put16(b,26,0xfffe);put16(b,28,300);put16(b,30,0xfc00);
    const std::uint8_t cookie[]={'(','c',')','C','R','I'};std::copy(cookie,cookie+6,b.end()-6);return b;
}
MovieAdxHeader parsed(std::uint32_t rate=48000,std::uint16_t cutoff=500){MovieAdxHeaderFailure f;auto h=parse_movie_adx_header(bytes(rate,cutoff),f);check(bool(h),"synthetic owned header");return std::move(*h);}
}
int main(int argc,char**){try{
    check(argc==1,"usage: test_native_movie_adx_profile (no arguments)");NativeMovieAdxProfileResolver resolver;MovieAdxStreamFailure f;
    auto h=parsed();const auto raw=h.raw_header;auto p=resolver.resolve(h,f);
    check(p&&f.kind==MovieAdxStreamError::none&&p->mode==AdxBlockMode::version4_flags0&&p->coefficients==std::vector<std::pair<std::int16_t,std::int16_t>>(2,{7400,-3342}),"actual-rate source-derived literal pair");
    check(p->histories==std::vector<std::pair<std::int16_t,std::int16_t>>({{1,-2},{300,-1024}}),"exact asymmetric final history mapping");
    auto other=resolver.resolve(parsed(44100),f);check(other&&other->coefficients==std::vector<std::pair<std::int16_t,std::int16_t>>(2,{7334,-3283}),"constructor source-derived literal pair");
    for(int which=0;which<12;++which){auto bad=h;switch(which){case 0:++bad.data_start;break;case 1:++bad.encoding;break;case 2:++bad.block_bytes;break;case 3:++bad.sample_bits;break;case 4:++bad.source_planes;break;case 5:++bad.sample_rate;break;case 6:++bad.declared_samples;break;case 7:++bad.cutoff;break;case 8:++bad.version;break;case 9:++bad.flags;break;case 10:++bad.histories[0].second;break;case 11:++bad.histories[1].first;break;}
        check(!resolver.resolve(bad,f)&&f.kind==MovieAdxStreamError::profile&&std::string_view(f.operation)=="ADX profile descriptor/raw-header identity mismatch","every supplied descriptor value validated against raw bytes");}
    for(int which=0;which<4;++which){auto bad=h;if(which==0)bad.raw_header[0]=0;if(which==1)bad.raw_header.push_back(0);if(which==2)bad.raw_header.pop_back();if(which==3)bad.raw_header[24]^=1;check(!resolver.resolve(bad,f)&&f.kind!=MovieAdxStreamError::none,"changed/malformed/trailing raw header rejects");}
    for(auto bad:{parsed(48001),parsed(44100,501),parsed(1,1)})check(!resolver.resolve(bad,f)&&f.kind==MovieAdxStreamError::profile&&std::string_view(f.operation)=="ADX unsupported_coefficient_inputs","no nearest/default/generic coefficient");
    for(auto bad:{parsed(0x80000000u),parsed(48000,0x8000u)})check(!resolver.resolve(bad,f)&&f.kind==MovieAdxStreamError::profile&&std::string_view(f.operation)=="ADX source_signed_coefficient_input_domain unsupported","original signed numeric operand domain explicit");
    // Reparse allocation, coefficient vector, history vector: genuine failures.
    for(int i=0;i<3;++i){allocations_before_failure=i;check(!resolver.resolve(h,f)&&f.kind==MovieAdxStreamError::allocation,"staged resolver allocation errors named");allocations_before_failure=-1;check(h.raw_header==raw&&p->histories[1].second==-1024,"allocation failure retains source and earlier owned result");}
    auto held=std::move(*p);p.reset();h.raw_header.assign(1,0);h.histories[0]={0,0};check(held.histories[0]==std::pair<std::int16_t,std::int16_t>{1,-2},"resolved profile owns header seeds");
    auto extra=bytes();auto stream=NativeMovieAdxStream::create(extra,std::make_unique<NativeMovieAdxProfileResolver>(),f);check(bool(stream),"concrete resolver through owned stream");
    std::vector<std::uint8_t> packet(36+18,0);put16(packet,36,0x8001);put16(packet,38,14);
    auto frame=stream->feed(packet,f);check(frame.status==MovieAdxStreamStatus::frame&&frame.encoded_ended&&frame.frame&&frame.frame->planes[0]==std::vector<std::int16_t>({3,4})&&frame.frame->planes[1]==std::vector<std::int16_t>({1377,2242}),"literal raw-zero residual predictor samples through concrete profile+stream");
    check(stream->finish(f).status==MovieAdxStreamStatus::ended,"explicit marker owner end");
    std::puts("PASS explicit source-derived ADX profile data; no hardware or decoder-mode admission");check(!std::fflush(stdout)&&!std::ferror(stdout),"complete profile fixture output");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL ADX profile %s\n",e.what());return 1;}}
