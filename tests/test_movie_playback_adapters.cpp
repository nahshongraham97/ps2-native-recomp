// Opt-in component fixture: actual source wrapper and ZERO-ONLY WASAPI adapter.
#include "mka/movie_playback.hpp"
#include "native_test_sha256.hpp"
#include <array>
#include <bit>
#include <charconv>
#include <cstdio>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <objbase.h>
#endif
using namespace mka::video;
namespace sound=mka::sound;
namespace {
void check(bool b,const char* label){if(!b)throw std::runtime_error(label);}
std::uint64_t number(const char* text){std::string_view s(text);std::uint64_t v=0;auto r=std::from_chars(s.data(),s.data()+s.size(),v);check(!s.empty()&&r.ec==std::errc{}&&r.ptr==s.data()+s.size(),"complete unsigned decimal argument");return v;}
std::uint32_t word(const char* s){auto v=number(s);check(v<=UINT32_MAX,"client format u32 representation");return static_cast<std::uint32_t>(v);}
std::vector<MovieAssetCatalogEntry> catalog(const char* name){
 std::ifstream file(std::filesystem::u8path(name),std::ios::binary);check(bool(file),"explicit catalog manifest open");std::vector<MovieAssetCatalogEntry> result;std::set<std::string> keys;std::string line;
 while(std::getline(file,line)){
  if(!line.empty()&&line.back()=='\r')line.pop_back();std::array<std::string_view,4> f{};std::string_view tail(line);
  for(std::size_t i=0;i<4;++i){auto tab=tail.find('\t');if(i==3){check(tab==tail.npos,"manifest extra field");f[i]=tail;}else{check(tab!=tail.npos,"manifest missing field");f[i]=tail.substr(0,tab);tail.remove_prefix(tab+1);}}
  check(!f[0].empty()&&keys.emplace(f[0]).second,"unique manifest key");std::uint64_t length=0;auto parsed=std::from_chars(f[1].data(),f[1].data()+f[1].size(),length);check(parsed.ec==std::errc{}&&parsed.ptr==f[1].data()+f[1].size()&&length>=8,"manifest length");
  for(auto hex:{f[2],f[3]}){check(hex.size()==16,"manifest endpoint hex extent");for(char c:hex)check((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F'),"manifest endpoint hex");}
  result.push_back({std::string(f[0]),std::string(f[0])});
 }
 check(file.eof()&&!file.bad()&&result.size()==38&&keys.count("LOGON.SFD"),"complete explicit38 catalog with LOGON");return result;
}
bool time_equal(const MovieTimestamp&a,const MovieTimestamp&b){return a.value==b.value&&a.numerator==b.numerator&&a.denominator==b.denominator&&a.present==b.present;}
bool request_equal(const RetailMovieRequest&a,const RetailMovieRequest&b){return a.id==b.id&&a.logical_key==b.logical_key&&a.logical_extent.width==b.logical_extent.width&&a.logical_extent.height==b.logical_extent.height&&a.requested_view==b.requested_view&&a.selected_view==b.selected_view&&a.route==b.route&&a.route_extra==b.route_extra&&a.subtitle_language==b.subtitle_language&&a.subtitle_tag==b.subtitle_tag;}
struct Held{std::optional<MovieVideoFrame>video;std::optional<MovieAudioFrame>audio;};
template<class Read> Held first_frames(Read&& read){Held h;while(!h.video||!h.audio){auto r=read();check(r.status==MovieReadStatus::frame,"LOGON must supply first audio and video before EOF");if(auto*v=std::get_if<MovieVideoFrame>(&r.frame)){if(!h.video)h.video=std::move(*v);}else{if(!h.audio)h.audio=std::move(std::get<MovieAudioFrame>(r.frame));}}return h;}
std::string video_hash(const MovieVideoFrame&f){NativeTestSha256 hash;for(const auto&p:f.planes)hash.add(p.data(),p.size());return hash.finish();}
std::string audio_hash(const MovieAudioFrame&f){NativeTestSha256 hash;for(const auto&p:f.channels)hash.add(p.data(),p.size()*sizeof(std::int16_t));return hash.finish();}
void check_zeros(const std::vector<float>& pcm){for(float x:pcm)check(std::bit_cast<std::uint32_t>(x)==0,"EVERY queued sample must be positive zero bits");}
void receipt_json(const sound::AudioSubmission&r){std::printf("{\"first\":%llu,\"frames\":%llu,\"epoch\":%llu}",(unsigned long long)r.first_client_frame,(unsigned long long)r.client_frames,(unsigned long long)r.reset_epoch);}
#ifdef _WIN32
struct Com{Com(){check(SUCCEEDED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)),"explicit fixture STA COM");}~Com(){CoUninitialize();}};
#endif
}
int main(int argc,char**argv){try{
 check(argc==11&&std::string_view(argv[1])=="--silent","usage: --silent library_dir MOVIEPS2_root fixtures_assets.tsv endpoint_id rate channels mask duration100ns exact|convert");
 MovieAssetFailure af;auto store=NativeMovieAssetStore::create(std::filesystem::u8path(argv[3]),catalog(argv[4]),af);check(bool(store),"explicit asset store");MovieSourceFailure sf;
 auto source=NativeRetailMovieSource::open(*store,std::filesystem::u8path(argv[2]),0,MovieViewSelection::normal,0,sf);check(source&&sf.kind==MovieSourceError::none,"exact catalog LOGON source");const RetailMovieRequest request=*source->request();check(request.logical_key=="LOGON.SFD","source is actual LOGON key");
 MovieControlFailure f;check(!own_movie_playback_source({},f)&&f.kind!=MovieControlError::none,"null source adapter rejected");auto wrapped=own_movie_playback_source(std::move(source),f);check(wrapped&&!source&&f.kind==MovieControlError::none,"source ownership consumed");
 auto direct=NativeRetailMovieSource::open(*store,std::filesystem::u8path(argv[2]),0,MovieViewSelection::normal,0,sf);check(direct&&sf.kind==MovieSourceError::none,"separate direct samecatalog reference");check(request_equal(request,wrapped->request())&&request_equal(request,*direct->request()),"owned request exact through wrapper");store.reset();
 auto held=first_frames([&]{auto r=wrapped->next(f);check(f.kind==MovieControlError::none,"actual OwnedSource next error");return r;});
 auto reference=first_frames([&]{auto r=direct->next(sf);check(sf.kind==MovieSourceError::none,"direct source next error");return r;});wrapped.reset();direct.reset();
 const auto&v=*held.video;const auto&w=*reference.video;check(v.width==w.width&&v.height==w.height&&v.planes==w.planes&&v.codec_timing==w.codec_timing&&time_equal(v.source_pts,w.source_pts)&&time_equal(v.timestamp,w.timestamp)&&v.sample_aspect_numerator==w.sample_aspect_numerator&&v.sample_aspect_denominator==w.sample_aspect_denominator&&v.color_range==w.color_range&&v.color_space==w.color_space&&v.color_primaries==w.color_primaries&&v.color_transfer==w.color_transfer,"first video bytes plus all timing/color/source identity retained after owner destruction");
 const auto&a=*held.audio;const auto&b=*reference.audio;check(a.channels==b.channels&&a.sample_rate==b.sample_rate&&a.channel_layout==b.channel_layout&&time_equal(a.source_pts,b.source_pts)&&time_equal(a.timestamp,b.timestamp),"first audio PCM/layout/timestamps retained after owner destruction");check(v.codec_timing.picture_source&&a.channel_layout.order==MovieAudioChannelOrder::unspecified&&a.channel_layout.channel_count==2,"actual LOGON source metadata not guessed stereo");
 std::printf("{\"source\":\"LOGON.SFD\",\"scope\":\"first_video_and_audio_only_two_source_owners\",\"video_sha256\":\"%s\",\"audio_planar_s16_sha256\":\"%s\",\"width\":%u,\"height\":%u,\"source_audio_rate\":%u,\"source_audio_planes\":%zu}\n",video_hash(v).c_str(),audio_hash(a).c_str(),v.width,v.height,a.sample_rate,a.channels.size());
#ifdef _WIN32
 Com apartment;sound::AudioDeviceConfig c;c.endpoint_id_utf8=argv[5];c.sample_rate=word(argv[6]);c.channels=word(argv[7]);c.channel_mask=word(argv[8]);c.buffer_duration_100ns=number(argv[9]);check(std::string_view(argv[10])=="exact"||std::string_view(argv[10])=="convert","explicit conversion policy");c.conversion=std::string_view(argv[10])=="exact"?sound::AudioConversion::exact_shared:sound::AudioConversion::engine_conversion;
 sound::AudioDeviceFailure df;auto device=sound::NativeAudioDevice::open(c,df);if(!device){std::fprintf(stderr,"named native open kind=%d operation=%s code=%d\n",int(df.kind),df.operation,df.native_code);return 1;}
 auto audio=own_movie_playback_audio(std::move(device),f);check(audio&&!device&&f.kind==MovieControlError::none,"actual OwnedAudio consumes native device");auto format=audio->format(f);check(format&&f.kind==MovieControlError::none&&*format==sound::AudioStreamFormat{c.sample_rate,c.channels,c.channel_mask},"retained actual client format");
 auto initial=audio->queue_status(f);check(initial&&!initial->started&&!initial->padding_frames&&!initial->submitted_frames&&initial->capacity_frames,"exclusive initial audio stream");check(audio->pause(true,f)&&audio->pause(true,f)&&audio->pause(false,f)&&audio->pause(false,f),"idempotent pause plus deferred empty start");auto deferred=audio->queue_status(f);check(deferred&&!deferred->started&&deferred->reset_epoch==initial->reset_epoch,"empty unpause does not start or reset");
 check(initial->capacity_frames<=SIZE_MAX/c.channels,"host zero allocation extent");std::vector<float> zeros(std::size_t(initial->capacity_frames)*c.channels,0.0f);check_zeros(zeros);auto accepted=audio->queue(zeros,f);check(accepted.status==MovieAudioSubmitStatus::accepted&&accepted.submission&&f.kind==MovieControlError::none,"actual adapter zeroPCM accepted");auto first=*accepted.submission;check(first.first_client_frame==0&&first.client_frames==initial->capacity_frames&&first.reset_epoch==initial->reset_epoch,"full capacity receipt exact");auto queued=audio->queue_status(f);check(queued&&!queued->started&&queued->writable_frames==0&&queued->submitted_frames==initial->capacity_frames,"queue acceptance alone never starts");
 std::vector<float> one(c.channels,0.0f);check_zeros(one);auto full=audio->queue(one,f);check(full.status==MovieAudioSubmitStatus::full&&!full.submission&&f.kind==MovieControlError::none,"adapter maps actual bufferfull to typed backpressure");check(audio->pause(false,f)&&audio->pause(false,f),"resume actual prequeued stream idempotent");auto started=audio->queue_status(f);check(started&&started->started,"positive padding permits actual Start");auto clock=audio->clock(f);check(clock&&clock->frequency&&clock->reset_epoch==initial->reset_epoch,"raw clock is separate deviceunits");check(audio->pause(true,f)&&audio->pause(true,f),"Stop without Reset idempotent");auto stopped=audio->queue_status(f);check(stopped&&!stopped->started&&stopped->reset_epoch==initial->reset_epoch&&stopped->submitted_frames==initial->capacity_frames,"pause preserves epoch and submitted ranges");
 check(audio->stop_and_reset(f),"explicit adapter stop_and_reset");auto reset=audio->queue_status(f);check(reset&&!reset->started&&!reset->submitted_frames&&!reset->padding_frames&&reset->reset_epoch==initial->reset_epoch+1,"explicit Reset flushes new epoch");check_zeros(one);auto next=audio->queue(one,f);check(next.status==MovieAudioSubmitStatus::accepted&&next.submission&&next.submission->first_client_frame==0&&next.submission->client_frames==1&&next.submission->reset_epoch==reset->reset_epoch,"reset epoch new receipt");
 MovieControlFailure wrong;bool foreign_success=true;std::thread t([&]{foreign_success=audio->format(wrong).has_value();});t.join();check(!foreign_success&&wrong.kind==MovieControlError::wrong_thread,"actual native adapter rejects foreign format query");
 std::printf("{\"audio\":\"actual_owned_adapter_positive_zero_only\",\"rate\":%u,\"channels\":%u,\"mask\":%u,\"capacity_frames\":%u,\"first\":",format->sample_rate,format->channels,format->channel_mask,initial->capacity_frames);receipt_json(first);std::printf(",\"after_reset\":");receipt_json(*next.submission);std::printf(",\"raw_clock_position\":%llu,\"raw_clock_frequency\":%llu,\"raw_clock_precise\":%s}\n",(unsigned long long)clock->position,(unsigned long long)clock->frequency,clock->precise?"true":"false");
 check(audio->stop_and_reset(f),"final zero-only cleanup");audio.reset();
#else
 throw std::runtime_error("actual audio adapter fixture requires Windows");
#endif
 std::puts("PASS actual native source and zero-only audio adapters; no movie playback or retail policy binding");check(!std::fflush(stdout)&&!std::ferror(stdout),"complete JSON and completion output");return 0;
}catch(const std::exception&e){std::fprintf(stderr,"FAIL adapter fixture: %s\n",e.what());return 1;}}
