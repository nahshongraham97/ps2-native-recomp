#include "mka/movie_pcm_binding.hpp"
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <utility>
using namespace mka::video;
namespace {bool fail_allocation=false;}
void* operator new(std::size_t n){if(std::exchange(fail_allocation,false))throw std::bad_alloc();if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
MoviePcmBindingConfiguration config(std::size_t count=2){MoviePcmBindingConfiguration c;c.source_key="LOGON.SFD";c.source_layout.channel_count=count;c.source_sample_rate=48000;c.client_format={48000,2,3};c.mode=MoviePcmMixMode::generic_stateless;for(std::size_t i=0;i<count;++i)c.routes.emplace_back(0,i==1?15:-15,0,1);return c;}
MovieAudioFrame frame(){MovieAudioFrame a;a.sample_rate=48000;a.channel_layout.channel_count=2;a.channels={{-32768,-1,1000},{32767,1,-1000}};return a;}
int main(){try{
    MoviePcmBindingFailure f;auto c=config();auto p=NativeMoviePcmBinding::create(c,f);check(bool(p),"explicit binding create");c.source_key="changed";c.routes[0].explicit_pan=15;check(p->source_key()=="LOGON.SFD"&&p->client_format()==mka::sound::AudioStreamFormat{48000,2,3},"owned immutable config");
    auto a=frame();const auto raw=a.channels;std::vector<float> out{42};check(p->prepare_audio(a,0,out,f)&&out==std::vector<float>({-32767.0f/32768,32767.0f/32768,-1.0f/32768,1.0f/32768,1000.0f/32768,-1000.0f/32768}),"literal fullscale planes/output order and final clip");const auto accepted=out;check(p->prepare_audio(a,-24,out,f)&&out[4]==757.0f/32768&&out[5]==-758.0f/32768,"new gain from original signed floor");check(a.channels==raw&&accepted[4]==1000.0f/32768,"raw and earlier accepted output immutable");auto changed=out;check(p->prepare_audio(a,-24,out,f)&&out==changed,"pure repeated gain");
    auto bad=a;bad.channel_layout.mask=3;check(!p->prepare_audio(bad,0,out,f)&&f.kind==MoviePcmBindingError::source_layout_mismatch&&out==changed,"full UNSPEC layout mismatch atomic");bad=a;bad.sample_rate=44100;check(!p->prepare_audio(bad,0,out,f)&&f.kind==MoviePcmBindingError::source_rate_mismatch&&out==changed,"rate mismatch atomic");bad=a;bad.channels[1].pop_back();check(!p->prepare_audio(bad,0,out,f)&&f.kind==MoviePcmBindingError::invalid_frame&&out==changed,"unequal planes atomic");
    fail_allocation=true;check(!p->prepare_audio(a,0,out,f)&&f.kind==MoviePcmBindingError::allocation&&out==changed,"real staging allocation failure atomic");
    auto invalid=config();invalid.mode=MoviePcmMixMode::unspecified;check(!NativeMoviePcmBinding::create(invalid,f)&&f.kind==MoviePcmBindingError::unsupported_profile,"no implicit generic mode");invalid=config();invalid.routes[0].output1=0;check(!NativeMoviePcmBinding::create(invalid,f),"no overlapping output pair");invalid=config();invalid.routes[0].output1=2;check(!NativeMoviePcmBinding::create(invalid,f),"route output bounds");invalid=config();invalid.client_format.channel_mask=1;check(!NativeMoviePcmBinding::create(invalid,f),"complete client channel mask");invalid=config();invalid.client_format.sample_rate=44100;check(!NativeMoviePcmBinding::create(invalid,f),"no resampling");invalid=config();invalid.routes.pop_back();check(!NativeMoviePcmBinding::create(invalid,f),"one explicit route per original plane");
    auto many=config(65);for(auto& route:many.routes)route.explicit_pan=-15;auto dynamic=NativeMoviePcmBinding::create(std::move(many),f);MovieAudioFrame wide;wide.sample_rate=48000;wide.channel_layout.channel_count=65;wide.channels.assign(65,std::vector<std::int16_t>(513,1));check(dynamic&&dynamic->prepare_audio(wide,0,out,f)&&out.size()==1026,"native dynamic geometry");for(std::size_t i=0;i<513;++i)check(out[2*i]==65.0f/32768&&std::bit_cast<std::uint32_t>(out[2*i+1])==0,"dynamic sum and explicit unused output zero");auto held=out;dynamic.reset();check(held[1024]==65.0f/32768,"owned output after binding lifetime");
    std::puts("PASS explicit immutable PCM binding; no retail profile admission or playback");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL PCM binding %s\n",e.what());return 1;}}
