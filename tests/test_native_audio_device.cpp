// Default mode is pure validation: no COM, endpoint access, device or sound.
// --silent is opt-in ONLY and queues exactly zero float samples; no effects.
#include "mka/audio_device.hpp"
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>
#include <string>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <objbase.h>
#endif
namespace {
using namespace mka::sound;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::uint64_t number(const char* s){std::size_t used=0;auto n=std::stoull(s,&used,0);check(used==std::string(s).size(),"complete numeric CLI value");return n;}
std::uint32_t word(const char* s){auto n=number(s);check(n<=UINT32_MAX,"CLI u32 bound");return std::uint32_t(n);}
void pure(bool verbose=true){
    AudioDeviceFailure f;AudioDeviceConfig c{"explicit-test-only-endpoint",48000,2,3,1000000,AudioConversion::exact_shared};
    check(validate_audio_device_config(c,f),"explicit source format");auto bad=c;bad.sample_rate=0;check(!validate_audio_device_config(bad,f)&&f.kind==AudioDeviceError::invalid_format,"zero rate never defaulted");
    bad=c;bad.channels=1;check(!validate_audio_device_config(bad,f)&&f.kind==AudioDeviceError::invalid_format,"layout/count mismatch");
    bad=c;bad.channel_mask=0;check(!validate_audio_device_config(bad,f)&&f.kind==AudioDeviceError::invalid_format,"layout must be explicit");
    bad=c;bad.channel_mask=0x80000003;check(!validate_audio_device_config(bad,f),"unknown speaker bits");
    bad=c;bad.sample_rate=UINT32_MAX;check(!validate_audio_device_config(bad,f)&&f.kind==AudioDeviceError::host_capacity,"native format byte-rate overflow");
    bad=c;bad.buffer_duration_100ns=UINT64_MAX;check(!validate_audio_device_config(bad,f)&&f.kind==AudioDeviceError::invalid_duration,"signed reference time bound");
    bad=c;bad.buffer_duration_100ns=0;check(!validate_audio_device_config(bad,f),"no default buffer duration");
    bad=c;bad.endpoint_id_utf8.clear();check(!validate_audio_device_config(bad,f)&&f.kind==AudioDeviceError::invalid_endpoint,"no hidden endpoint selection");
    bad=c;bad.endpoint_id_utf8=std::string("a\0b",3);check(!validate_audio_device_config(bad,f),"embedded NUL endpoint rejected");
    bad=c;bad.conversion=AudioConversion::unspecified;check(!validate_audio_device_config(bad,f)&&f.kind==AudioDeviceError::invalid_conversion,"no implicit conversion policy");
    // Large rates/channel counts are not console caps; only native format representation applies.
    auto wide=c;wide.sample_rate=192000;wide.channels=18;wide.channel_mask=0x3ffff;check(validate_audio_device_config(wide,f),"all18 known host speaker positions accepted");
    float zero[4]{};std::uint32_t frames=99;check(validate_audio_pcm(zero,4,2,frames,f)&&frames==2,"frame vs sample count");
    check(!validate_audio_pcm(zero,3,2,frames,f)&&frames==0&&f.kind==AudioDeviceError::invalid_pcm,"partial interleaved frame rejected");
    check(!validate_audio_pcm(nullptr,4,2,frames,f)&&frames==0,"null PCM rejected");check(!validate_audio_pcm(zero,0,2,frames,f),"zero-length request is not fake submitted sound");
    float invalid[2]{0,std::numeric_limits<float>::quiet_NaN()};check(!validate_audio_pcm(invalid,2,2,frames,f)&&frames==0,"nonfinite PCM rejected transactionally");
    invalid[1]=std::numeric_limits<float>::infinity();check(!validate_audio_pcm(invalid,2,2,frames,f),"infinite PCM rejected");
    if(SIZE_MAX/2>UINT32_MAX)check(!validate_audio_pcm(zero,std::size_t(UINT32_MAX)+1,1,frames,f)&&f.kind==AudioDeviceError::host_capacity,"u32 frame overflow rejected before pointer dereference");
    check(!inspect_audio_endpoints(AudioEndpointRole::unspecified,f)&&f.kind==AudioDeviceError::invalid_role,"no discovery role default or endpoint access");
    check(!inspect_audio_endpoints(static_cast<AudioEndpointRole>(99),f)&&f.kind==AudioDeviceError::invalid_role,"unknown discovery role rejected before COM");
    if(verbose)std::puts("PASS pure format/PCM validation only; no device opened or audio generated");
}
void json_string(const std::string& text){
    std::putchar('"');for(unsigned char c:text){if(c=='"'||c=='\\'){std::putchar('\\');std::putchar(c);}else if(c<32)std::printf("\\u%04x",unsigned(c));else std::putchar(c);}std::putchar('"');
}
void json_failure(const AudioDeviceFailure& f){std::printf("{\"kind\":%d,\"native_code\":%d,\"operation\":",int(f.kind),int(f.native_code));json_string(f.operation);std::putchar('}');}
#ifdef _WIN32
struct Com {Com(){check(SUCCEEDED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)),"fixture explicit STA COM");}~Com(){CoUninitialize();}};
#endif
}
int main(int argc,char** argv){try{
    using namespace mka::sound;
    const bool inspect_mode=argc==3&&std::string(argv[1])=="--inspect";
    pure(!inspect_mode);if(argc==1)return 0;
    if(inspect_mode){
        const std::string role_name=argv[2];AudioEndpointRole role=AudioEndpointRole::unspecified;
        if(role_name=="console")role=AudioEndpointRole::console;
        if(role_name=="multimedia")role=AudioEndpointRole::multimedia;
        if(role_name=="communications")role=AudioEndpointRole::communications;
        check(role!=AudioEndpointRole::unspecified,"inspect requires explicit console|multimedia|communications role");
#ifdef _WIN32
        Com apartment;AudioDeviceFailure f;auto inventory=inspect_audio_endpoints(role,f);
        if(!inventory){std::printf("{\"discovery_failure\":");json_failure(f);std::puts("}");return 1;}
        std::printf("{\"mode\":\"inspect_only_no_stream\",\"requested_role\":");json_string(role_name);
        std::printf(",\"default_endpoint_id_utf8\":");json_string(inventory->default_endpoint_id_utf8);
        std::printf(",\"default_failure\":");json_failure(inventory->default_failure);std::printf(",\"active_render_endpoints\":[");
        bool first=true;for(const auto& endpoint:inventory->active_render_endpoints){if(!first)std::putchar(',');first=false;
            std::printf("{\"endpoint_id_utf8\":");json_string(endpoint.endpoint_id_utf8);
            const auto& m=endpoint.mix;std::printf(",\"default_for_role\":%s,\"sample_rate\":%u,\"channels\":%u,\"channel_mask\":%u,\"container_bits\":%u,\"valid_bits\":%u,\"encoding\":",endpoint.default_for_role?"true":"false",m.sample_rate,m.channels,m.channel_mask,m.container_bits,m.valid_bits);
            json_string(m.encoding==AudioMixEncoding::ieee_float?"ieee_float":m.encoding==AudioMixEncoding::pcm?"pcm":"unknown");
            std::printf(",\"mix_failure\":");json_failure(endpoint.mix_failure);std::putchar('}');
        }std::puts("]}");return 0;
#else
        throw std::runtime_error("endpoint inspection requires Windows");
#endif
    }
    check(argc==8&&std::string(argv[1])=="--silent","usage: test_native_audio_device [--inspect console|multimedia|communications] OR [--silent endpointID rate channels mask duration100ns exact|convert]");
#ifdef _WIN32
    using namespace mka::sound;Com apartment;AudioDeviceFailure f;AudioDeviceConfig c;
    c.endpoint_id_utf8=argv[2];c.sample_rate=word(argv[3]);c.channels=word(argv[4]);c.channel_mask=word(argv[5]);c.buffer_duration_100ns=number(argv[6]);
    check(std::string(argv[7])=="exact"||std::string(argv[7])=="convert","explicit CLI conversion");c.conversion=std::string(argv[7])=="exact"?AudioConversion::exact_shared:AudioConversion::engine_conversion;
    auto device=NativeAudioDevice::open(c,f);if(!device){std::fprintf(stderr,"open failure kind=%d HRESULT=%08X operation=%s\n",int(f.kind),unsigned(f.native_code),f.operation);return 1;}
    auto status=device->queue_status(f);check(status&&status->capacity_frames&&status->padding_frames==0&&status->submitted_frames==0,"new endpoint queue exact initial state");
    check(!device->start(f)&&f.kind==AudioDeviceError::invalid_state,"unprimed start named");
    // Every value is positive zero. No waveform, effects, test tones or game assets.
    check(std::uint64_t(status->capacity_frames)*c.channels<=SIZE_MAX/sizeof(float),"silent fixture allocation extent");
    std::vector<float> silence(std::size_t(status->capacity_frames)*c.channels,0.0f);
    for(float value:silence)check(std::bit_cast<std::uint32_t>(value)==0,"all queued float samples are positive zero");
    check(status->capacity_frames>=2,"queried host buffer must fit two nonempty receipt test submissions");
    const std::uint32_t first_frames=status->capacity_frames/2;
    const auto first_receipt=device->queue_with_receipt(silence.data(),std::size_t(first_frames)*c.channels,f);
    check(first_receipt&&f.kind==AudioDeviceError::none&&first_receipt->first_client_frame==0&&
          first_receipt->client_frames==first_frames&&first_receipt->reset_epoch==0,"first exact silent submission receipt");
    const auto second_receipt=device->queue_with_receipt(silence.data()+std::size_t(first_frames)*c.channels,
          std::size_t(status->capacity_frames-first_frames)*c.channels,f);
    check(second_receipt&&f.kind==AudioDeviceError::none&&second_receipt->first_client_frame==first_frames&&
          second_receipt->client_frames==status->capacity_frames-first_frames&&second_receipt->reset_epoch==0,
          "second receipt contiguous through actual queried host capacity");
    auto full=device->queue_status(f);check(full&&full->submitted_frames==status->capacity_frames&&full->padding_frames==status->capacity_frames,"submitted vs padding explicit");
    check(!device->queue_with_receipt(silence.data(),c.channels,f)&&f.kind==AudioDeviceError::buffer_full,"full queue has no receipt or overwrite");
    full=device->queue_status(f);check(full&&full->submitted_frames==status->capacity_frames,"failed queue does not add submitted frames");
    NativeAudioDevice moved(std::move(*device));check(!device->queue_status(f)&&f.kind==AudioDeviceError::invalid_owner,"moved device cannot access COM");
    check(!device->queue_with_receipt(nullptr,0,f)&&f.kind==AudioDeviceError::invalid_owner,
          "moved owner has no receipt before PCM validation");
    AudioDeviceError wrong=AudioDeviceError::none,wrong_queue=AudioDeviceError::none;
    std::thread foreign([&]{AudioDeviceFailure local;
        auto receipt=moved.queue_with_receipt(nullptr,0,local);
        wrong_queue=receipt?AudioDeviceError::none:local.kind;
        wrong=moved.close(local)?AudioDeviceError::none:local.kind;});foreign.join();
    check(wrong_queue==AudioDeviceError::wrong_thread,"foreign submission has no receipt before PCM validation");
    check(wrong==AudioDeviceError::wrong_thread,"foreign close retains resource for owner retry");
    full=moved.queue_status(f);check(full&&full->submitted_frames==status->capacity_frames&&full->reset_epoch==0,
          "failed owner/thread/full submissions preserve sequence");
    check(moved.start(f),"caller explicit silent start");check(!moved.start(f)&&f.kind==AudioDeviceError::invalid_state,"duplicate start is not silent success");
    auto clock=moved.clock(f);check(clock&&clock->frequency&&clock->reset_epoch==0,"raw device clock ratio not submitted-as-played");
    auto advanced=*clock;bool progressed=false;
    for(unsigned poll=0;poll<100;++poll){
        Sleep(2);auto observed=moved.clock(f);check(bool(observed),"read actual running device clock");
        check(observed->frequency==clock->frequency&&observed->position>=advanced.position,"same-epoch raw device clock is monotonic");
        advanced=*observed;if(advanced.position>clock->position){progressed=true;break;}
    }
    check(progressed,"native rendering clock advances without equating submitted frames to playback");
    check(!moved.reset(f)&&f.kind==AudioDeviceError::invalid_state,"running reset rejected");
    check(moved.stop(f),"explicit stop");check(!moved.stop(f)&&f.kind==AudioDeviceError::invalid_state,"duplicate stop named");
    const auto stopped=moved.queue_status(f);check(stopped&&stopped->submitted_frames==status->capacity_frames&&
          stopped->reset_epoch==0&&!stopped->started,"Stop retains submitted sequence and epoch");
    check(moved.reset(f),"explicit flush/reset");
    auto reset=moved.queue_status(f);check(reset&&reset->padding_frames==0&&reset->submitted_frames==0&&reset->reset_epoch==1,"reset separates epoch from submitted counters");
    const auto reset_receipt=moved.queue_with_receipt(silence.data(),c.channels,f);
    check(reset_receipt&&reset_receipt->first_client_frame==0&&reset_receipt->client_frames==1&&
          reset_receipt->reset_epoch==1,"Reset starts frame zero in new receipt epoch");
    check(moved.queue(silence.data(),c.channels,f),"legacy bool queue shares receipt submission implementation");
    const auto shared=moved.queue_status(f);check(shared&&shared->submitted_frames==2&&shared->reset_epoch==1,
          "bool and receipt queues advance one shared client sequence");
    check(moved.close(f),"owner closes COM audio services");
    check(!moved.queue_with_receipt(nullptr,0,f)&&f.kind==AudioDeviceError::closed,
          "closed owner has no receipt before PCM validation");check(!moved.clock(f)&&f.kind==AudioDeviceError::closed,"closed owner cannot report fictional clock");
    // Three actual returned receipts plus the fourth accepted bool submission's
    // shared counter; do not invent a receipt value from the bool-only API.
    std::printf("{\"kind\":\"audio_submission_receipts\",\"actual_capacity_frames\":%u,"
                "\"epoch0_first\":{\"first_client_frame\":%llu,\"client_frames\":%llu,\"reset_epoch\":%llu},"
                "\"epoch0_second\":{\"first_client_frame\":%llu,\"client_frames\":%llu,\"reset_epoch\":%llu},"
                "\"epoch1_first\":{\"first_client_frame\":%llu,\"client_frames\":%llu,\"reset_epoch\":%llu},"
                "\"after_bool_queue\":{\"submitted_frames\":%llu,\"reset_epoch\":%llu},"
                "\"accepted_submissions\":4,\"all_queued_samples_positive_zero\":true,"
                "\"scope\":\"host_buffer_submission_only_no_heard_time_or_drain_claim\"}\n",
        status->capacity_frames,
        (unsigned long long)first_receipt->first_client_frame,(unsigned long long)first_receipt->client_frames,(unsigned long long)first_receipt->reset_epoch,
        (unsigned long long)second_receipt->first_client_frame,(unsigned long long)second_receipt->client_frames,(unsigned long long)second_receipt->reset_epoch,
        (unsigned long long)reset_receipt->first_client_frame,(unsigned long long)reset_receipt->client_frames,(unsigned long long)reset_receipt->reset_epoch,
        (unsigned long long)shared->submitted_frames,(unsigned long long)shared->reset_epoch);
    check(std::fflush(stdout)==0&&!std::ferror(stdout),"submission receipt measurement output");
    std::printf("PASS opt-in silent-only WASAPI lifecycle; initialrawposition=%llu progressedrawposition=%llu frequency=%llu QPC100ns=%llu precise=%d; no audible/game/movie-drain claim\n",(unsigned long long)clock->position,(unsigned long long)advanced.position,(unsigned long long)advanced.frequency,(unsigned long long)advanced.qpc_100ns,int(advanced.precise));return 0;
#else
    throw std::runtime_error("silent WASAPI fixture requires Windows");
#endif
}catch(const std::exception& e){std::fprintf(stderr,"FAIL native audio device: %s\n",e.what());return 1;}}
