#include "mka/movie_adx_header.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <utility>
using namespace mka::video;
namespace {bool fail_next=false;}
void* operator new(std::size_t n){if(std::exchange(fail_next,false))throw std::bad_alloc();if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}
namespace {
void check(bool yes,const char* message){if(!yes)throw std::runtime_error(message);}
void set16(std::vector<std::uint8_t>& b,std::size_t i,std::uint16_t v){b[i]=v>>8;b[i+1]=v&255;}
void set32(std::vector<std::uint8_t>& b,std::size_t i,std::uint32_t v){b[i]=v>>24;b[i+1]=(v>>16)&255;b[i+2]=(v>>8)&255;b[i+3]=v&255;}
std::vector<std::uint8_t> header(std::size_t size){
    check(size>=38&&size<=65539,"fixture serialized extent");
    std::vector<std::uint8_t> b(size,0xa5);set16(b,0,0x8000);set16(b,2,static_cast<std::uint16_t>(size-4));
    b[4]=3;b[5]=18;b[6]=4;b[7]=2;set32(b,8,48000);set32(b,12,12345);set16(b,16,500);b[18]=4;b[19]=0;
    // Distinct signed words expose parser temporary/backend transpose mistakes.
    set16(b,24,1);set16(b,26,0xfffe);set16(b,28,300);set16(b,30,0xfc00);
    constexpr std::array<std::uint8_t,6> cookie{'(','c',')','C','R','I'};
    std::copy(cookie.begin(),cookie.end(),b.end()-cookie.size());return b;
}
}
int main(int argc,char**){try{
    check(argc==1,"usage: test_native_movie_adx_header (no arguments)");MovieAdxHeaderFailure f;
    auto bytes=header(288);const auto original=bytes;auto parsed=parse_movie_adx_header(bytes,f);
    check(parsed&&f.kind==MovieAdxHeaderError::none&&parsed->data_start==288&&parsed->raw_header==original,"owned exact header and extent");
    check(parsed->encoding==3&&parsed->block_bytes==18&&parsed->sample_bits==4&&parsed->source_planes==2&&parsed->sample_rate==48000&&parsed->declared_samples==12345&&parsed->cutoff==500&&parsed->version==4&&!parsed->flags,"explicit fields");
    check(parsed->histories[0]==std::pair<std::int16_t,std::int16_t>{1,-2}&&parsed->histories[1]==std::pair<std::int16_t,std::int16_t>{300,-1024},"final perplane latest/older, not parser temporary arrays");
    for(std::size_t n=0;n<original.size();++n)check(!parse_movie_adx_header(std::span(original).first(n),f)&&f.kind==MovieAdxHeaderError::incomplete,"every truncated prefix incomplete");
    for(auto size:{std::size_t(38),std::size_t(40),std::size_t(4096),std::size_t(65539)}){
        auto b=header(size);b.insert(b.end(),{0x80,0,0xff,0xaa});auto p=parse_movie_adx_header(b,f);
        check(p&&p->data_start==size&&p->raw_header.size()==size&&std::equal(p->raw_header.begin(),p->raw_header.end(),b.begin()),"variable extent, trailing payload untouched/excluded");
    }
    auto bad=original;bad[0]=0;check(!parse_movie_adx_header(bad,f)&&f.kind==MovieAdxHeaderError::signature,"wrong signature never searched");
    for(std::uint16_t offset:{0,1,27,28,33}){bad=original;set16(bad,2,offset);check(!parse_movie_adx_header(bad,f)&&f.kind==MovieAdxHeaderError::extent,"overlapping/short declared extent");}
    bad=original;set16(bad,2,65535);check(!parse_movie_adx_header(bad,f)&&f.kind==MovieAdxHeaderError::incomplete,"declared header beyond input");
    for(std::size_t n=0;n<6;++n){bad=original;bad[bad.size()-6+n]^=1;check(!parse_movie_adx_header(bad,f)&&f.kind==MovieAdxHeaderError::extent,"each copyright byte validated");}
    for(std::size_t i:{4u,5u,6u,7u,18u,19u}){bad=original;bad[i]^=1;check(!parse_movie_adx_header(bad,f)&&f.kind==MovieAdxHeaderError::unsupported_profile,"unsupported serialized profile field");}
    for(std::size_t i:{8u,12u,16u}){bad=original;if(i==16)set16(bad,i,0);else set32(bad,i,0);check(!parse_movie_adx_header(bad,f)&&f.kind==MovieAdxHeaderError::invalid_field,"no zero rate/count/cutoff default");}
    auto positive=original;set32(positive,8,0xffffffffu);set32(positive,12,0xffffffffu);set16(positive,16,0xffffu);auto wide=parse_movie_adx_header(positive,f);check(wide&&wide->sample_rate==0xffffffffull&&wide->declared_samples==0xffffffffull&&wide->cutoff==65535,"retain positive serialized values, no implicit coefficient admission");
    fail_next=true;check(!parse_movie_adx_header(original,f)&&f.kind==MovieAdxHeaderError::allocation,"real owned header allocation failure");check(parsed->raw_header==original,"failure preserves earlier owned header");
    bytes.assign(1,0);check(parsed->raw_header==original,"returned header independent of original mutation");auto held=std::move(*parsed);parsed.reset();check(held.raw_header==original&&held.histories[1].second==-1024,"moved owned header lifetime");
    std::puts("PASS owned explicit ADX header fields/history; no coefficient or packet admission");check(!std::fflush(stdout)&&!std::ferror(stdout),"complete header fixture output");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL ADX header %s\n",e.what());return 1;}}
