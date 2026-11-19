#include "mka/movie_pcm_mix.hpp"
#include <algorithm>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <stdexcept>
#include <string_view>
#include <utility>
using namespace mka::video;
namespace {bool fail_next_allocation=false;}
void* operator new(std::size_t n){if(std::exchange(fail_next_allocation,false))throw std::bad_alloc();if(void*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void*p) noexcept{std::free(p);}
void operator delete(void*p,std::size_t) noexcept{std::free(p);}
namespace {
void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
std::vector<std::span<const std::int16_t>> views(const std::vector<std::vector<std::int16_t>>& p){std::vector<std::span<const std::int16_t>> out;for(const auto&v:p)out.emplace_back(v);return out;}
MoviePcmMixProfile profile(std::size_t planes,std::size_t outputs,std::uint16_t coefficient){return {MoviePcmMixMode::generic_stateless,outputs,std::vector<std::vector<std::uint16_t>>(planes,std::vector<std::uint16_t>(outputs,coefficient))};}
void literals(){
 MoviePcmMixFailure f;
 const auto gain=movie_pcm_gain(-1000,976);check(gain.effective_tenth_db==-24&&gain.table_value==194,"gain wide sum before game clamp");
 check(movie_pcm_gain(INT64_MAX,INT64_MAX).table_value==256&&movie_pcm_gain(INT64_MIN,INT64_MIN).effective_tenth_db==-999,"gain nativewide same-sign extremes");
 check(movie_pcm_gain(INT64_MAX,INT64_MIN).effective_tenth_db==-1&&movie_pcm_gain(INT64_MAX,INT64_MIN).table_value==253,"gain wide cancellation");
 for(const auto pair:std::array<std::array<int,2>,6>{{{{0,256}},{{-24,194}},{{-49,145}},{{-85,96}},{{-145,48}},{{-960,0}}}})check(movie_pcm_gain(pair[0],0).table_value==pair[1],"source literal gain table");
 check(movie_pcm_pan(256,INT64_MIN,f)==std::array<std::uint16_t,2>{255,0}&&movie_pcm_pan(256,INT64_MAX,f)==std::array<std::uint16_t,2>{0,255},"explicit pan game clamp/endpoints/final255");
 check(movie_pcm_pan(255,0,f)==std::array<std::uint16_t,2>{180,180}&&movie_pcm_pan(256,0,f)==std::array<std::uint16_t,2>{181,181},"literal center pan quantization");
 check(movie_pcm_pan(194,0,f)==std::array<std::uint16_t,2>{137,137}&&movie_pcm_pan(1,-14,f)==std::array<std::uint16_t,2>{1,0},"pan integer rounding");
 check(!movie_pcm_pan(257,0,f)&&f.kind==MoviePcmMixError::invalid_gain,"invalid gain cannot infer source table");
 const std::vector<std::vector<std::int16_t>> source{{-32768,-257,-256,-255,-1,0,1,255,256,257,32767}};const auto source_views=views(source);
 const std::array<std::uint16_t,5> coefficients{0,64,128,194,255};
 const std::array<std::array<std::int16_t,11>,5> expected{{
  {{0,0,0,0,0,0,0,0,0,0,0}},
  {{-8192,-65,-64,-64,-1,0,0,63,64,64,8191}},
  {{-16384,-129,-128,-128,-1,0,0,127,128,128,16383}},
  {{-24832,-195,-194,-194,-1,0,0,193,194,194,24831}},
  {{-32767,-257,-256,-255,-1,0,1,255,256,257,32767}}
 }};
 for(std::size_t c=0;c<coefficients.size();++c){auto p=profile(1,1,coefficients[c]);auto r=mix_movie_pcm(p,source_views,f);check(r&&f.kind==MoviePcmMixError::none&&std::equal(r->outputs[0].begin(),r->outputs[0].end(),expected[c].begin(),expected[c].end()),"literal floor/fullscale/final clipping");}
 std::vector<std::vector<std::int16_t>> cancellation{{32767,-32768},{32767,-32768},{-32767,32767}};auto p=profile(3,1,255);auto cancellation_views=views(cancellation);auto summed=mix_movie_pcm(p,cancellation_views,f);check(summed&&summed->outputs[0]==std::vector<std::int16_t>({32767,-32767}),"sum before clipping, never per voice");
 std::vector<std::vector<std::int16_t>> wide(65,std::vector<std::int16_t>(513,1));auto wide_views=views(wide);auto wp=profile(65,5,255);auto wr=mix_movie_pcm(wp,wide_views,f);check(wr&&wr->outputs.size()==5&&wr->frames==513,"dynamic native geometry exceeds old source caps");for(const auto&output:wr->outputs)for(auto sample:output)check(sample==65,"wide dynamic exact sum");
 // Genuine allocation fault in the production staging path, not a fake size/pointer.
 fail_next_allocation=true;auto failed=mix_movie_pcm(wp,wide_views,f);check(!failed&&f.kind==MoviePcmMixError::allocation,"owned staging allocation named failure");check(wr->outputs[4][512]==65&&wide[0][0]==1,"allocation failure preserves inputs and earlier owned result");
 auto bad=wp;bad.mode=MoviePcmMixMode::unspecified;check(!mix_movie_pcm(bad,wide_views,f)&&f.kind==MoviePcmMixError::unsupported_profile,"no default generic/stereo profile");bad.mode=static_cast<MoviePcmMixMode>(99);check(!mix_movie_pcm(bad,wide_views,f),"unknown profile reject");bad=wp;bad.coefficients[0][0]=256;check(!mix_movie_pcm(bad,wide_views,f)&&f.kind==MoviePcmMixError::invalid_matrix,"coefficient must be admitted final0..255");bad=wp;bad.coefficients.pop_back();check(!mix_movie_pcm(bad,wide_views,f),"plane matrix count");bad=wp;bad.coefficients[0].pop_back();check(!mix_movie_pcm(bad,wide_views,f),"row output extent");bad=wp;bad.output_count=0;check(!mix_movie_pcm(bad,wide_views,f),"no guessed output layout");std::vector<std::span<const std::int16_t>> empty;check(!mix_movie_pcm(profile(0,1,0),empty,f)&&f.kind==MoviePcmMixError::invalid_planes,"empty source is not fake successful PCM");auto wrong_views=wide_views;wrong_views[0]=wrong_views[0].subspan(1);check(!mix_movie_pcm(wp,wrong_views,f)&&f.kind==MoviePcmMixError::invalid_planes,"unequal frame extents");
 MoviePcmMixResult floats{{{1,-1},{32767,-32767}},2};auto interleaved=movie_pcm_float_interleaved(floats,f);check(interleaved&&interleaved->size()==4,"float ownership");constexpr std::array<std::uint32_t,4> bits{0x38000000,0x3f7ffe00,0xb8000000,0xbf7ffe00};for(std::size_t i=0;i<bits.size();++i)check(std::bit_cast<std::uint32_t>((*interleaved)[i])==bits[i],"exact float bits and explicit output order");
 MoviePcmMixResult zero{{{0}},1};check(std::bit_cast<std::uint32_t>(movie_pcm_float_interleaved(zero,f)->front())==0,"positive-zero native representation");
 fail_next_allocation=true;check(!movie_pcm_float_interleaved(floats,f)&&f.kind==MoviePcmMixError::allocation,"float allocation transactional");floats.outputs[0][0]=-32768;check(!movie_pcm_float_interleaved(floats,f)&&f.kind==MoviePcmMixError::invalid_planes,"float adapter rejects nonfinal source domain");
 // New gain uses original retained samples; old accepted prefix is separately owned.
 std::vector<std::vector<std::int16_t>> raw{{-1,1000,-1000,32767}};auto rv=views(raw);auto oldp=profile(1,1,255),newp=profile(1,1,64);auto old=mix_movie_pcm(oldp,rv,f);check(bool(old),"tail old mix");const auto accepted_prefix=std::vector<std::int16_t>(old->outputs[0].begin(),old->outputs[0].begin()+2);std::array<std::span<const std::int16_t>,1> tail{std::span<const std::int16_t>(raw[0]).subspan(2)};auto changed=mix_movie_pcm(newp,tail,f);check(changed&&changed->outputs[0]==std::vector<std::int16_t>({-250,8191})&&accepted_prefix==std::vector<std::int16_t>({-1,1000})&&old->outputs[0][2]==-1000,"stateless original tail only, accepted prefix/owned old result immutable");
}
}
int main(int argc,char**argv){try{
 check(argc==1||(argc==2&&std::string_view(argv[1])=="--dump-tables"),"usage: test_movie_pcm_mix [--dump-tables]");literals();
 if(argc==2){for(int i=0;i<1000;++i)std::printf("gain\t%d\t%u\n",i,unsigned(movie_pcm_gain(-i,0).table_value));MoviePcmMixFailure f;for(int i=0;i<31;++i){auto p=movie_pcm_pan(255,i-15,f);check(bool(p),"pan public table dump");std::printf("pan\t%d\t%u\t%u\n",i,unsigned((*p)[0]),unsigned((*p)[1]));}}
 if(argc==2){
  MoviePcmMixFailure f;
  for(unsigned gain=0;gain<=256;++gain)for(int pan=-15;pan<=15;++pan){auto pair=movie_pcm_pan(static_cast<std::uint16_t>(gain),pan,f);check(pair&&f.kind==MoviePcmMixError::none,"complete public pan grid");std::printf("pan_grid\t%u\t%d\t%u\t%u\n",gain,pan,unsigned((*pair)[0]),unsigned((*pair)[1]));}
  const std::vector<std::vector<std::int16_t>> source{{-32768,-257,-256,-255,-1,0,1,255,256,257,32767}};auto sv=views(source);auto matrix=profile(1,256,0);for(std::size_t c=0;c<256;++c)matrix.coefficients[0][c]=static_cast<std::uint16_t>(c);auto mixed=mix_movie_pcm(matrix,sv,f);check(mixed&&f.kind==MoviePcmMixError::none&&mixed->outputs.size()==256&&mixed->frames==11,"complete sample coefficient grid");
  for(std::size_t c=0;c<256;++c)for(std::size_t i=0;i<11;++i)std::printf("sample_grid\t%zu\t%d\t%d\n",c,int(source[0][i]),int(mixed->outputs[c][i]));
 }
 std::puts("PASS explicit stateless movie PCM business kernel; no profile admission or playback");check(!std::fflush(stdout)&&!std::ferror(stdout),"complete fixture output");return 0;
}catch(const std::exception&e){std::fprintf(stderr,"FAIL movie PCM: %s\n",e.what());return 1;}}
