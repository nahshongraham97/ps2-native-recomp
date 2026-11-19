#include "mka/audio_device.hpp"
#include <cmath>
#include <climits>
#include <stdexcept>
#include <cstring>
#include <cstdio>
#include <exception>
#include <limits>
#include <new>
#include <thread>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <ks.h>
#include <ksmedia.h>
#endif
namespace mka::sound {
static_assert(sizeof(float)==4&&std::numeric_limits<float>::is_iec559,"WASAPI IEEE float32 host representation required");
namespace {
bool failure(AudioDeviceFailure& f,AudioDeviceError kind,const char* operation,std::int32_t code=0)noexcept{f={kind,code,operation};return false;}
unsigned count_bits(std::uint32_t n){unsigned count=0;while(n){n&=n-1;++count;}return count;}
}
#ifdef _WIN32
namespace {
template<class T>struct LocalCom {
    T* p=nullptr;
    LocalCom()=default;LocalCom(const LocalCom&)=delete;LocalCom& operator=(const LocalCom&)=delete;
    ~LocalCom(){if(p)p->Release();}
};
struct TaskMemory {
    void* p=nullptr;
    TaskMemory()=default;TaskMemory(const TaskMemory&)=delete;TaskMemory& operator=(const TaskMemory&)=delete;
    ~TaskMemory(){CoTaskMemFree(p);}
};
bool inspection_result(HRESULT hr,AudioDeviceError kind,const char* operation,AudioDeviceFailure& f){
    if(SUCCEEDED(hr))return true;
    if(hr==AUDCLNT_E_DEVICE_INVALIDATED||hr==AUDCLNT_E_RESOURCES_INVALIDATED)kind=AudioDeviceError::device_lost;
    if(hr==AUDCLNT_E_SERVICE_NOT_RUNNING)kind=AudioDeviceError::service_unavailable;
    return failure(f,kind,operation,hr);
}
bool endpoint_id(IMMDevice* endpoint,std::string& out,AudioDeviceFailure& f){
    if(!endpoint)return failure(f,AudioDeviceError::invalid_endpoint,"null endpoint in inventory");
    TaskMemory memory;LPWSTR text=nullptr;HRESULT hr=endpoint->GetId(&text);memory.p=text;
    if(!inspection_result(hr,AudioDeviceError::enumeration,"IMMDevice::GetId",f))return false;
    if(!text||!*text)return failure(f,AudioDeviceError::invalid_endpoint,"empty endpoint ID from MMDevice");
    int count=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,nullptr,0,nullptr,nullptr);
    if(!count)return failure(f,AudioDeviceError::invalid_endpoint,"WideCharToMultiByte endpoint ID",GetLastError());
    std::string converted(std::size_t(count),'\0');
    if(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,converted.data(),count,nullptr,nullptr)!=count)return failure(f,AudioDeviceError::invalid_endpoint,"endpoint ID UTF8 conversion",GetLastError());
    converted.pop_back();out=std::move(converted);return true;
}
void inspect_mix(IMMDevice* endpoint,AudioEndpointInfo& info){
    LocalCom<IAudioClient> client;
    if(!inspection_result(endpoint->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(&client.p)),AudioDeviceError::endpoint_open,"IMMDevice::Activate inspection only",info.mix_failure))return;
    TaskMemory memory;WAVEFORMATEX* format=nullptr;HRESULT hr=client.p->GetMixFormat(&format);memory.p=format;
    if(!inspection_result(hr,AudioDeviceError::format_unsupported,"IAudioClient::GetMixFormat",info.mix_failure))return;
    if(!format){failure(info.mix_failure,AudioDeviceError::format_unsupported,"null shared mix format");return;}
    auto& mix=info.mix;mix.sample_rate=format->nSamplesPerSec;mix.channels=format->nChannels;mix.container_bits=format->wBitsPerSample;
    // WAVEFORMATEX alone has no channel mask. Never guess mono/stereo.
    if(format->wFormatTag!=WAVE_FORMAT_EXTENSIBLE||format->cbSize<sizeof(WAVEFORMATEXTENSIBLE)-sizeof(WAVEFORMATEX)){
        failure(info.mix_failure,AudioDeviceError::format_unsupported,"shared mix format lacks verified extensible speaker mask");return;
    }
    const auto* extended=reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);
    mix.channel_mask=extended->dwChannelMask;mix.valid_bits=extended->Samples.wValidBitsPerSample;
    const GUID floating={STATIC_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT};const GUID pcm={STATIC_KSDATAFORMAT_SUBTYPE_PCM};
    if(IsEqualGUID(extended->SubFormat,floating))mix.encoding=AudioMixEncoding::ieee_float;
    else if(IsEqualGUID(extended->SubFormat,pcm))mix.encoding=AudioMixEncoding::pcm;
    else {failure(info.mix_failure,AudioDeviceError::format_unsupported,"unknown shared mix subformat");return;}
    if(!mix.sample_rate||!mix.channels||!mix.channel_mask||(mix.channel_mask&~0x3ffffu)||count_bits(mix.channel_mask)!=mix.channels||!mix.container_bits||mix.container_bits%8||!mix.valid_bits||mix.valid_bits>mix.container_bits||
        std::uint64_t(mix.channels)*(mix.container_bits/8)!=format->nBlockAlign||std::uint64_t(mix.sample_rate)*format->nBlockAlign!=format->nAvgBytesPerSec){
        failure(info.mix_failure,AudioDeviceError::format_unsupported,"inconsistent shared mix format/layout");return;
    }
    if(mix.encoding==AudioMixEncoding::ieee_float&&((mix.container_bits!=32&&mix.container_bits!=64)||mix.valid_bits!=mix.container_bits)){
        failure(info.mix_failure,AudioDeviceError::format_unsupported,"unsupported shared floating point encoding");return;
    }
}
}
#endif
std::optional<AudioEndpointInventory> inspect_audio_endpoints(AudioEndpointRole role,AudioDeviceFailure& f){
    f={};if(role!=AudioEndpointRole::console&&role!=AudioEndpointRole::multimedia&&role!=AudioEndpointRole::communications){failure(f,AudioDeviceError::invalid_role,"explicit endpoint discovery role required");return {};}
#ifdef _WIN32
    try{
        APTTYPE apartment;APTTYPEQUALIFIER qualifier;HRESULT hr=CoGetApartmentType(&apartment,&qualifier);
        if(!inspection_result(hr,AudioDeviceError::com_not_initialized,"CoGetApartmentType discovery",f))return {};
        if(apartment!=APTTYPE_STA&&apartment!=APTTYPE_MAINSTA){failure(f,AudioDeviceError::unsupported_apartment,"discovery requires caller STA COM");return {};}
        LocalCom<IMMDeviceEnumerator> enumerator;
        if(!inspection_result(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,__uuidof(IMMDeviceEnumerator),reinterpret_cast<void**>(&enumerator.p)),AudioDeviceError::enumeration,"CoCreateInstance discovery enumerator",f))return {};
        AudioEndpointInventory inventory;inventory.requested_role=role;
        const ERole native_role=role==AudioEndpointRole::console?eConsole:role==AudioEndpointRole::multimedia?eMultimedia:eCommunications;
        LocalCom<IMMDevice> default_device;hr=enumerator.p->GetDefaultAudioEndpoint(eRender,native_role,&default_device.p);
        if(FAILED(hr))inspection_result(hr,AudioDeviceError::endpoint_unavailable,"GetDefaultAudioEndpoint requested role",inventory.default_failure);
        else if(!endpoint_id(default_device.p,inventory.default_endpoint_id_utf8,f))return {};
        LocalCom<IMMDeviceCollection> collection;
        if(!inspection_result(enumerator.p->EnumAudioEndpoints(eRender,DEVICE_STATE_ACTIVE,&collection.p),AudioDeviceError::enumeration,"EnumAudioEndpoints active rendering",f))return {};
        UINT count=0;if(!inspection_result(collection.p->GetCount(&count),AudioDeviceError::enumeration,"IMMDeviceCollection::GetCount",f))return {};
        for(UINT i=0;i<count;++i){
            LocalCom<IMMDevice> endpoint;if(!inspection_result(collection.p->Item(i,&endpoint.p),AudioDeviceError::enumeration,"IMMDeviceCollection::Item",f))return {};
            AudioEndpointInfo info;if(!endpoint_id(endpoint.p,info.endpoint_id_utf8,f))return {};
            info.default_for_role=!inventory.default_endpoint_id_utf8.empty()&&info.endpoint_id_utf8==inventory.default_endpoint_id_utf8;
            inspect_mix(endpoint.p,info);inventory.active_render_endpoints.push_back(std::move(info));
        }
        return inventory;
    }catch(const std::bad_alloc&){failure(f,AudioDeviceError::allocation,"native endpoint inventory allocation");}
     catch(const std::length_error&){failure(f,AudioDeviceError::host_capacity,"native endpoint inventory capacity");}
#else
    failure(f,AudioDeviceError::unsupported_host,"WASAPI discovery requires Windows");
#endif
    return {};
}
bool validate_audio_device_config(const AudioDeviceConfig& c,AudioDeviceFailure& f)noexcept{
    f={};if(c.endpoint_id_utf8.empty()||c.endpoint_id_utf8.find('\0')!=std::string::npos)return failure(f,AudioDeviceError::invalid_endpoint,"explicit endpoint ID required");
    // WAVEFORMATEXTENSIBLE known speaker-position bits, no inferred layout.
    if(!c.sample_rate||!c.channels||!c.channel_mask||(c.channel_mask&~0x3ffffu)||count_bits(c.channel_mask)!=c.channels)return failure(f,AudioDeviceError::invalid_format,"explicit float32 rate/channels/speaker mask");
    if(c.channels>UINT16_MAX/sizeof(float)||std::uint64_t(c.sample_rate)*c.channels*sizeof(float)>UINT32_MAX)return failure(f,AudioDeviceError::host_capacity,"WAVEFORMATEXTENSIBLE byte-rate/block-align representation");
    if(!c.buffer_duration_100ns||c.buffer_duration_100ns>std::uint64_t(INT64_MAX))return failure(f,AudioDeviceError::invalid_duration,"explicit positive REFERENCE_TIME buffer duration");
    if(c.conversion!=AudioConversion::exact_shared&&c.conversion!=AudioConversion::engine_conversion)return failure(f,AudioDeviceError::invalid_conversion,"explicit shared-format conversion policy");
    return true;
}
bool validate_audio_pcm(const float* pcm,std::size_t count,std::uint32_t channels,std::uint32_t& frames,AudioDeviceFailure& f)noexcept{
    f={};frames=0;if(!pcm||!channels||!count||count%channels)return failure(f,AudioDeviceError::invalid_pcm,"nonempty complete interleaved PCM frames required");
    if(count/channels>UINT32_MAX||count>SIZE_MAX/sizeof(float))return failure(f,AudioDeviceError::host_capacity,"WASAPI frame-count/host byte extent");
    for(std::size_t i=0;i<count;++i)if(!std::isfinite(pcm[i]))return failure(f,AudioDeviceError::invalid_pcm,"nonfinite interleaved PCM");
    frames=static_cast<std::uint32_t>(count/channels);return true;
}
struct NativeAudioDevice::Impl {
    std::thread::id owner=std::this_thread::get_id();
    std::uint32_t channels=0,capacity=0;std::uint64_t submitted=0,epoch=0;
    bool started=false,closed=false,failed=false;
#ifdef _WIN32
    IMMDeviceEnumerator* enumerator=nullptr;IMMDevice* endpoint=nullptr;IAudioClient* client=nullptr;
    IAudioRenderClient* render=nullptr;IAudioClock* clock=nullptr;
    void release()noexcept{if(clock){clock->Release();clock=nullptr;}if(render){render->Release();render=nullptr;}if(client){client->Release();client=nullptr;}if(endpoint){endpoint->Release();endpoint=nullptr;}if(enumerator){enumerator->Release();enumerator=nullptr;}closed=true;}
    bool result(HRESULT hr,AudioDeviceError kind,const char* op,AudioDeviceFailure& f,bool terminal=true){
        if(SUCCEEDED(hr))return true;
        if(hr==AUDCLNT_E_DEVICE_INVALIDATED||hr==AUDCLNT_E_RESOURCES_INVALIDATED)kind=AudioDeviceError::device_lost;
        if(hr==AUDCLNT_E_SERVICE_NOT_RUNNING)kind=AudioDeviceError::service_unavailable;
        if(terminal)failed=true;return failure(f,kind,op,hr);
    }
#endif
    ~Impl(){
#ifdef _WIN32
        if(!closed&&(enumerator||endpoint||client||render||clock)){
            if(std::this_thread::get_id()!=owner){std::fputs("[native-audio] fatal: COM audio owner destroyed on foreign thread; explicit owner-thread close required\n",stderr);std::terminate();}
            if(client&&started){const HRESULT hr=client->Stop();if(FAILED(hr))std::fprintf(stderr,"[native-audio] IAudioClient::Stop during destruction failed HRESULT=0x%08X; releasing owned interfaces\n",unsigned(hr));}release();
        }
#endif
    }
    bool usable(AudioDeviceFailure& f,bool allow_failed=false){f={};if(std::this_thread::get_id()!=owner)return failure(f,AudioDeviceError::wrong_thread,"WASAPI owner thread required");if(closed)return failure(f,AudioDeviceError::closed,"native audio device closed");if(failed&&!allow_failed)return failure(f,AudioDeviceError::failed_owner,"native audio device previously failed");return true;}
};
NativeAudioDevice::NativeAudioDevice(std::unique_ptr<Impl> p):impl_(std::move(p)){}
NativeAudioDevice::~NativeAudioDevice()=default;
NativeAudioDevice::NativeAudioDevice(NativeAudioDevice&&)noexcept=default;
NativeAudioDevice& NativeAudioDevice::operator=(NativeAudioDevice&&)noexcept=default;
std::unique_ptr<NativeAudioDevice> NativeAudioDevice::open(const AudioDeviceConfig& c,AudioDeviceFailure& f){
    if(!validate_audio_device_config(c,f))return {};
#ifdef _WIN32
    try{
        APTTYPE apartment;APTTYPEQUALIFIER qualifier;HRESULT hr=CoGetApartmentType(&apartment,&qualifier);
        if(FAILED(hr)){failure(f,AudioDeviceError::com_not_initialized,"CoGetApartmentType",hr);return {};}
        if(apartment!=APTTYPE_STA&&apartment!=APTTYPE_MAINSTA){failure(f,AudioDeviceError::unsupported_apartment,"caller STA COM required for IAudioClient first use");return {};}
        if(c.endpoint_id_utf8.size()>INT_MAX){failure(f,AudioDeviceError::host_capacity,"UTF8 endpoint length");return {};}
        int length=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,c.endpoint_id_utf8.data(),int(c.endpoint_id_utf8.size()),nullptr,0);
        if(!length){failure(f,AudioDeviceError::invalid_endpoint,"MultiByteToWideChar endpoint UTF8",GetLastError());return {};}
        std::wstring id(length,L'\0');if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,c.endpoint_id_utf8.data(),int(c.endpoint_id_utf8.size()),id.data(),length)!=length){failure(f,AudioDeviceError::invalid_endpoint,"UTF8 endpoint conversion",GetLastError());return {};}
        auto s=std::make_unique<Impl>();s->channels=c.channels;
        if(!s->result(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,__uuidof(IMMDeviceEnumerator),reinterpret_cast<void**>(&s->enumerator)),AudioDeviceError::endpoint_open,"CoCreateInstance MMDeviceEnumerator",f))return {};
        if(!s->result(s->enumerator->GetDevice(id.c_str(),&s->endpoint),AudioDeviceError::endpoint_open,"IMMDeviceEnumerator::GetDevice explicit ID",f))return {};
        IMMEndpoint* endpoint_info=nullptr;
        if(!s->result(s->endpoint->QueryInterface(__uuidof(IMMEndpoint),reinterpret_cast<void**>(&endpoint_info)),AudioDeviceError::endpoint_open,"IMMDevice::QueryInterface IMMEndpoint",f))return {};
        EDataFlow flow=eAll;hr=endpoint_info->GetDataFlow(&flow);endpoint_info->Release();
        if(!s->result(hr,AudioDeviceError::endpoint_open,"IMMEndpoint::GetDataFlow",f))return {};
        if(flow!=eRender){failure(f,AudioDeviceError::invalid_endpoint,"explicit endpoint must be a rendering device");return {};}
        DWORD state=0;if(!s->result(s->endpoint->GetState(&state),AudioDeviceError::endpoint_open,"IMMDevice::GetState",f))return {};
        if(state!=DEVICE_STATE_ACTIVE){failure(f,AudioDeviceError::endpoint_unavailable,"explicit endpoint is not active");return {};}
        if(!s->result(s->endpoint->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(&s->client)),AudioDeviceError::endpoint_open,"IMMDevice::Activate IAudioClient",f))return {};
        WAVEFORMATEXTENSIBLE format{};format.Format.wFormatTag=WAVE_FORMAT_EXTENSIBLE;format.Format.nChannels=WORD(c.channels);format.Format.nSamplesPerSec=c.sample_rate;
        format.Format.wBitsPerSample=32;format.Format.nBlockAlign=WORD(c.channels*sizeof(float));format.Format.nAvgBytesPerSec=c.sample_rate*format.Format.nBlockAlign;
        format.Format.cbSize=sizeof(WAVEFORMATEXTENSIBLE)-sizeof(WAVEFORMATEX);format.Samples.wValidBitsPerSample=32;format.dwChannelMask=c.channel_mask;const GUID ieee_float={STATIC_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT};format.SubFormat=ieee_float;
        DWORD flags=AUDCLNT_STREAMFLAGS_NOPERSIST;
        if(c.conversion==AudioConversion::exact_shared){
            WAVEFORMATEX* closest=nullptr;hr=s->client->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED,&format.Format,&closest);if(closest)CoTaskMemFree(closest);
            if(hr!=S_OK){s->result(hr,AudioDeviceError::format_unsupported,"IAudioClient::IsFormatSupported exact float format",f);if(SUCCEEDED(hr))failure(f,AudioDeviceError::format_unsupported,"closest format substitution forbidden",hr);return {};}
        }else flags|=AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM|AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
        if(!s->result(s->client->Initialize(AUDCLNT_SHAREMODE_SHARED,flags,REFERENCE_TIME(c.buffer_duration_100ns),0,&format.Format,nullptr),AudioDeviceError::initialize,"IAudioClient::Initialize shared float32",f))return {};
        if(!s->result(s->client->GetBufferSize(&s->capacity),AudioDeviceError::buffer_query,"IAudioClient::GetBufferSize",f))return {};
        if(!s->capacity){failure(f,AudioDeviceError::buffer_query,"zero WASAPI capacity");return {};}
        if(!s->result(s->client->GetService(__uuidof(IAudioRenderClient),reinterpret_cast<void**>(&s->render)),AudioDeviceError::service,"IAudioClient::GetService IAudioRenderClient",f))return {};
        if(!s->result(s->client->GetService(__uuidof(IAudioClock),reinterpret_cast<void**>(&s->clock)),AudioDeviceError::service,"IAudioClient::GetService IAudioClock",f))return {};
        return std::unique_ptr<NativeAudioDevice>(new NativeAudioDevice(std::move(s)));
    }catch(const std::bad_alloc&){failure(f,AudioDeviceError::allocation,"native WASAPI ownership allocation");}
     catch(const std::length_error&){failure(f,AudioDeviceError::host_capacity,"native WASAPI string capacity");}
#else
    failure(f,AudioDeviceError::unsupported_host,"WASAPI requires Windows");
#endif
    return {};
}
std::optional<AudioQueueStatus> NativeAudioDevice::queue_status(AudioDeviceFailure& f){
    f={};if(!impl_){failure(f,AudioDeviceError::invalid_owner,"moved native audio device");return {};}
    auto& s=*impl_;if(!s.usable(f))return {};
#ifdef _WIN32
    UINT32 padding=0;if(!s.result(s.client->GetCurrentPadding(&padding),AudioDeviceError::buffer_query,"IAudioClient::GetCurrentPadding",f))return {};
    if(padding>s.capacity){s.failed=true;failure(f,AudioDeviceError::buffer_query,"padding exceeds actual endpoint capacity");return {};}
    return AudioQueueStatus{s.capacity,padding,s.capacity-padding,s.submitted,s.epoch,s.started};
#else
    return {};
#endif
}
bool NativeAudioDevice::queue(const float* pcm,std::size_t count,AudioDeviceFailure& f){
    f={};if(!impl_)return failure(f,AudioDeviceError::invalid_owner,"moved native audio device");auto& s=*impl_;if(!s.usable(f))return false;
    std::uint32_t frames;if(!validate_audio_pcm(pcm,count,s.channels,frames,f))return false;
    if(frames>UINT64_MAX-s.submitted)return failure(f,AudioDeviceError::host_capacity,"submitted frame counter overflow");
    auto status=queue_status(f);if(!status)return false;if(frames>status->writable_frames)return failure(f,AudioDeviceError::buffer_full,"requested PCM exceeds writable endpoint frames");
#ifdef _WIN32
    BYTE* target=nullptr;if(!s.result(s.render->GetBuffer(frames,&target),AudioDeviceError::buffer_acquire,"IAudioRenderClient::GetBuffer",f))return false;
    if(!target){HRESULT released=s.render->ReleaseBuffer(0,0);s.failed=true;return failure(f,AudioDeviceError::buffer_acquire,"nonnull render packet required",FAILED(released)?released:E_POINTER);}
    std::memcpy(target,pcm,count*sizeof(float));
    if(!s.result(s.render->ReleaseBuffer(frames,0),AudioDeviceError::buffer_release,"IAudioRenderClient::ReleaseBuffer",f))return false;
    s.submitted+=frames;return true;
#else
    return false;
#endif
}
bool NativeAudioDevice::start(AudioDeviceFailure& f){
    f={};if(!impl_)return failure(f,AudioDeviceError::invalid_owner,"moved native audio device");auto& s=*impl_;if(!s.usable(f))return false;
    if(s.started)return failure(f,AudioDeviceError::invalid_state,"stream already started");
    auto status=queue_status(f);if(!status)return false;if(!status->padding_frames)return failure(f,AudioDeviceError::invalid_state,"explicit prequeue required before Start");
#ifdef _WIN32
    if(!s.result(s.client->Start(),AudioDeviceError::start,"IAudioClient::Start",f))return false;s.started=true;return true;
#else
    return false;
#endif
}
bool NativeAudioDevice::stop(AudioDeviceFailure& f){
    f={};if(!impl_)return failure(f,AudioDeviceError::invalid_owner,"moved native audio device");auto& s=*impl_;if(!s.usable(f))return false;if(!s.started)return failure(f,AudioDeviceError::invalid_state,"stream not started");
#ifdef _WIN32
    if(!s.result(s.client->Stop(),AudioDeviceError::stop,"IAudioClient::Stop",f))return false;s.started=false;return true;
#else
    return false;
#endif
}
bool NativeAudioDevice::reset(AudioDeviceFailure& f){
    f={};if(!impl_)return failure(f,AudioDeviceError::invalid_owner,"moved native audio device");auto& s=*impl_;if(!s.usable(f))return false;if(s.started)return failure(f,AudioDeviceError::invalid_state,"Reset requires stopped stream");if(s.epoch==UINT64_MAX)return failure(f,AudioDeviceError::host_capacity,"reset epoch overflow");
#ifdef _WIN32
    if(!s.result(s.client->Reset(),AudioDeviceError::reset,"IAudioClient::Reset",f))return false;s.submitted=0;++s.epoch;return true;
#else
    return false;
#endif
}
std::optional<AudioClockReading> NativeAudioDevice::clock(AudioDeviceFailure& f){
    f={};if(!impl_){failure(f,AudioDeviceError::invalid_owner,"moved native audio device");return {};}auto& s=*impl_;if(!s.usable(f))return {};
#ifdef _WIN32
    UINT64 position=0,frequency=0,qpc=0;if(!s.result(s.clock->GetFrequency(&frequency),AudioDeviceError::clock,"IAudioClock::GetFrequency",f))return {};
    HRESULT hr=s.clock->GetPosition(&position,&qpc);if(!s.result(hr,AudioDeviceError::clock,"IAudioClock::GetPosition",f))return {};
    if(!frequency){s.failed=true;failure(f,AudioDeviceError::clock,"zero device clock frequency");return {};}
    return AudioClockReading{position,frequency,qpc,s.epoch,hr==S_OK};
#else
    return {};
#endif
}
bool NativeAudioDevice::close(AudioDeviceFailure& f){
    f={};if(!impl_)return failure(f,AudioDeviceError::invalid_owner,"moved native audio device");auto& s=*impl_;if(!s.usable(f,true))return false;
#ifdef _WIN32
    HRESULT stopped=S_OK;if(s.started)stopped=s.client->Stop();s.started=false;s.release();
    return s.result(stopped,AudioDeviceError::stop,"IAudioClient::Stop during close (resources released)",f);
#else
    return false;
#endif
}
} // namespace mka::sound
