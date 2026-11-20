#include "mka/movie_service_acquisition.hpp"
#include <new>
#include <exception>
namespace mka::video {
namespace {
bool fail(MovieServiceAcquisitionFailure& f,MovieServiceAcquisitionError k,const char* op) noexcept {f.kind=k;f.operation=op;return false;}
struct Guard {bool& b;explicit Guard(bool& x):b(x){b=true;}~Guard(){b=false;}};
bool same(const RetailMovieRequest& a,const RetailMovieRequest& b) noexcept {
    return a.id==b.id&&a.logical_key==b.logical_key&&a.logical_extent.width==b.logical_extent.width&&
        a.logical_extent.height==b.logical_extent.height&&a.requested_view==b.requested_view&&
        a.selected_view==b.selected_view&&a.route==b.route&&a.route_extra==b.route_extra&&
        a.subtitle_language==b.subtitle_language&&a.subtitle_tag==b.subtitle_tag;
}
void translate(const MovieServiceAcquisitionFailure& a,MovieSessionFailure& f) noexcept {
    if(a.session.kind!=MovieSessionError::none){f=a.session;return;}
    f={};f.kind=a.kind==MovieServiceAcquisitionError::allocation?MovieSessionError::allocation:
        a.kind==MovieServiceAcquisitionError::wrong_thread?MovieSessionError::wrong_thread:MovieSessionError::control;
    f.operation=a.operation;
    f.control={MovieControlError::native_service,a.device.kind!=sound::AudioDeviceError::none?a.device.operation:a.operation,
        a.device.kind!=sound::AudioDeviceError::none?a.device.native_code:static_cast<std::int64_t>(a.kind)};
}
}
NativeMovieServiceAcquisition::NativeMovieServiceAcquisition(NativeMovieAssetStore s,std::filesystem::path l,
    std::shared_ptr<NativeMovieGpuBackend> g,NativeMovieColorTable c,std::vector<MovieServiceAcquisitionSetting> settings)
    :store_(std::move(s)),libraries_(std::move(l)),gpu_(std::move(g)),color_(std::move(c)),settings_(std::move(settings)){}
bool NativeMovieServiceAcquisition::access(MovieServiceAcquisitionFailure& f) const noexcept {
    f={};if(std::this_thread::get_id()!=owner_)return fail(f,MovieServiceAcquisitionError::wrong_thread,"movie acquisition owner thread");
    if(busy_)return fail(f,MovieServiceAcquisitionError::reentry,"movie acquisition reentry");return true;
}
std::shared_ptr<NativeMovieServiceAcquisition> NativeMovieServiceAcquisition::create(NativeMovieAssetStore store,
    std::filesystem::path libraries,std::shared_ptr<NativeMovieGpuBackend> gpu,NativeMovieColorTable color,
    std::vector<MovieServiceAcquisitionSetting> settings,MovieServiceAcquisitionFailure& f) noexcept {
    f={};if(!gpu||libraries.empty()||settings.empty()){fail(f,MovieServiceAcquisitionError::invalid_owner,"movie acquisition explicit resources");return {};}
    for(std::size_t i=0;i<settings.size();++i){const auto& s=settings[i];
        if(s.logical_key.empty()||(s.playback.audio!=MoviePlaybackAudioPresence::absent&&s.playback.audio!=MoviePlaybackAudioPresence::required)||
            (s.playback.audio==MoviePlaybackAudioPresence::required)!=bool(s.audio)){
            fail(f,MovieServiceAcquisitionError::invalid_setting,"movie acquisition explicit audio presence");return {};}
        for(std::size_t j=0;j<i;++j)if(settings[j].logical_key==s.logical_key){fail(f,MovieServiceAcquisitionError::invalid_setting,"movie acquisition duplicate exact key");return {};}
        if(s.audio){
            if(!sound::validate_audio_device_config(*s.audio,f.device)){fail(f,MovieServiceAcquisitionError::invalid_setting,"movie acquisition invalid native audio configuration");return {};}
            if(s.playback.client_sample_rate!=s.audio->sample_rate||s.playback.client_channels!=s.audio->channels||s.playback.client_channel_mask!=s.audio->channel_mask){fail(f,MovieServiceAcquisitionError::invalid_setting,"movie acquisition client configuration join");return {};}
        }else if(s.playback.client_sample_rate||s.playback.client_channels||s.playback.client_channel_mask){fail(f,MovieServiceAcquisitionError::invalid_setting,"movie acquisition absent audio client fields");return {};}
    }
    try{return std::shared_ptr<NativeMovieServiceAcquisition>(new NativeMovieServiceAcquisition(std::move(store),std::move(libraries),std::move(gpu),std::move(color),std::move(settings)));}
    catch(const std::bad_alloc&){fail(f,MovieServiceAcquisitionError::allocation,"movie acquisition allocation");return {};}
}
NativeMovieServiceAcquisition::~NativeMovieServiceAcquisition(){if(std::this_thread::get_id()!=owner_||busy_)std::terminate();}
MovieServiceSessionFactory NativeMovieServiceAcquisition::session_factory(MovieServiceAcquisitionFailure& f) noexcept {
    if(!access(f))return {};
    try {auto owned=shared_from_this();return [owned=std::move(owned)](const RetailMovieRequest& r,std::int64_t language,
        MovieSessionPolicyFactory policies,NativeMovieControlContext& control,MovieSessionFailure& why){
        MovieServiceAcquisitionFailure failure;auto session=owned->acquire(r,language,std::move(policies),control,failure);
        why={};if(!session)translate(failure,why);return session;
    };}catch(const std::bad_alloc&){fail(f,MovieServiceAcquisitionError::allocation,"movie acquisition factory allocation");return {};}
}
std::unique_ptr<NativeMoviePlaybackSession> NativeMovieServiceAcquisition::acquire(const RetailMovieRequest& request,
    std::int64_t language,MovieSessionPolicyFactory policies,NativeMovieControlContext& control,MovieServiceAcquisitionFailure& f) noexcept {
    if(!access(f))return {};Guard guard(busy_);
    try {
        auto resolved=RetailMovieCatalog{}.resolve(request.id,request.requested_view,language,f.catalog);
        if(!resolved||!same(request,*resolved)||request.route!=MovieRoute::blocking){fail(f,MovieServiceAcquisitionError::invalid_request,"movie acquisition semantic request mismatch");return {};}
        if(!policies){fail(f,MovieServiceAcquisitionError::invalid_owner,"movie acquisition required concrete policy factory");return {};}
        const MovieServiceAcquisitionSetting* selected=nullptr;for(const auto& s:settings_)if(s.logical_key==request.logical_key){selected=&s;break;}
        if(!selected){fail(f,MovieServiceAcquisitionError::missing_setting,"movie acquisition exact key settings absent");return {};}
        std::unique_ptr<sound::NativeAudioDevice> audio;
        if(selected->audio){audio=sound::NativeAudioDevice::open(*selected->audio,f.device);if(!audio){fail(f,MovieServiceAcquisitionError::device,"movie acquisition native device open");return {};}}
        auto session=NativeMoviePlaybackSession::open(store_,libraries_,request.id,request.requested_view,language,
            std::move(policies),std::move(audio),gpu_,color_,selected->playback,control,f.session);
        if(!session){fail(f,MovieServiceAcquisitionError::session,"movie acquisition native session open");return {};}
        return session;
    }catch(const std::bad_alloc&){fail(f,MovieServiceAcquisitionError::allocation,"movie acquisition request allocation");}
     catch(...){fail(f,MovieServiceAcquisitionError::session,"movie acquisition exception");}
    return {};
}
} // namespace mka::video
