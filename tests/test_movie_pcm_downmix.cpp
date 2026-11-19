#include "mka/movie_pcm_downmix.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <string_view>
#include <utility>
using namespace mka::video;
namespace {bool fail_next=false;}
void* operator new(std::size_t n){if(std::exchange(fail_next,false))throw std::bad_alloc();if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}
void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
int main(int argc,char** argv){try{
    check(argc==1||(argc==2&&std::string_view(argv[1])=="--dump-sums"),"usage: test_native_movie_pcm_downmix [--dump-sums]");
    MoviePcmMixFailure f;
    constexpr std::array<std::int16_t,11> a{-1,-1,0,1,1,32767,-32768,32767,100,-100,-257};
    constexpr std::array<std::int16_t,11> b{0,-1,0,0,1,32767,-32768,-32768,0,0,0};
    constexpr std::array<std::int16_t,11> expected{0,-1,0,0,1,32767,-32768,0,70,-70,-179};
    auto result=movie_pcm_downmix_duplicate(a,b,f);check(result&&f.kind==MoviePcmMixError::none&&result->frames==11&&result->outputs.size()==2,"literal result shape");
    for(const auto& plane:result->outputs)check(std::equal(plane.begin(),plane.end(),expected.begin(),expected.end()),"literal truncation, clamp and duplicate outputs");
    check(!movie_pcm_downmix_duplicate({},b,f)&&f.kind==MoviePcmMixError::invalid_planes,"empty input named");check(!movie_pcm_downmix_duplicate(a,std::span(b).first(10),f)&&f.kind==MoviePcmMixError::invalid_planes,"unequal input named");
    fail_next=true;check(!movie_pcm_downmix_duplicate(a,b,f)&&f.kind==MoviePcmMixError::allocation,"real allocation fault named");check(result->outputs[0][6]==-32768&&a[6]==-32768,"failure preserves old output and input");
    std::vector<std::int16_t> large(131073,257),other(large.size(),-1);auto dynamic=movie_pcm_downmix_duplicate(large,other,f);check(dynamic&&dynamic->frames==131073&&dynamic->outputs[0].back()==179&&dynamic->outputs[1].back()==179,"dynamic count beyond transport sizes");auto held=std::move(*dynamic);dynamic.reset();check(held.outputs[1][131072]==179,"owned result lifetime");
    if(argc==2){std::vector<std::int16_t> first,second;first.reserve(131071);second.reserve(131071);for(int sum=-65536;sum<=65534;++sum){const int x=std::max(-32768,sum-32767);first.push_back(static_cast<std::int16_t>(x));second.push_back(static_cast<std::int16_t>(sum-x));}auto all=movie_pcm_downmix_duplicate(first,second,f);check(all&&all->frames==131071&&all->outputs.size()==2,"complete sum-class batch");for(std::size_t i=0;i<131071;++i)std::printf("sum %d %d %d %d %d\n",int(i)-65536,int(first[i]),int(second[i]),int(all->outputs[0][i]),int(all->outputs[1][i]));}
    std::puts(argc==2?"PASS native downmix complete signed16 sum classes; no decoder mode admission":"PASS native downmix literals and ownership; no decoder mode admission");check(!std::fflush(stdout)&&!std::ferror(stdout),"complete fixture output");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL downmix %s\n",e.what());return 1;}}
