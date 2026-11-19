#include "mka/movie_audio_progress.hpp"
#include <utility>
#include <limits>
#include <new>
namespace mka::video {
namespace {
bool fail(MovieAudioProgressFailure& f,MovieAudioProgressError k,const char* op){f={k,op};return false;}
bool config(const MovieAudioProgressConfig& c,MovieAudioProgressFailure& f){
    if(c.source_identity.empty()||!c.source_rate||!c.client_rate)return fail(f,MovieAudioProgressError::invalid_config,"explicit source identity/origin/rates");
    if(c.source_rate!=c.client_rate)return fail(f,MovieAudioProgressError::unsupported_rate,"equal source/client rates required");
    return true;
}
bool add(std::uint64_t a,std::uint64_t b){return b<=UINT64_MAX-a;}
}
NativeMovieAudioProgress::NativeMovieAudioProgress(MovieAudioProgressConfig c):config_(std::move(c)){}
std::unique_ptr<NativeMovieAudioProgress> NativeMovieAudioProgress::create(MovieAudioProgressConfig c,MovieAudioProgressFailure& f){f={};if(!config(c,f))return {};try{return std::unique_ptr<NativeMovieAudioProgress>(new NativeMovieAudioProgress(std::move(c)));}catch(const std::bad_alloc&){fail(f,MovieAudioProgressError::allocation,"progress owner allocation");return {};}}
NativeMovieAudioProgress::NativeMovieAudioProgress(NativeMovieAudioProgress&& o) noexcept:valid_(std::exchange(o.valid_,false)),paused_(o.paused_),config_(std::move(o.config_)),submitted_(o.submitted_),retired_(o.retired_){}
NativeMovieAudioProgress& NativeMovieAudioProgress::operator=(NativeMovieAudioProgress&& o) noexcept {if(this!=&o){valid_=std::exchange(o.valid_,false);paused_=o.paused_;config_=std::move(o.config_);submitted_=o.submitted_;retired_=o.retired_;}return *this;}
bool NativeMovieAudioProgress::accept(std::string_view identity,std::uint64_t rate,const MoviePlaybackAudioAcceptance& a,MovieAudioProgressFailure& f) noexcept {
    f={};if(!valid_)return fail(f,MovieAudioProgressError::invalid_owner,"moved progress owner");
    if(identity!=config_.source_identity)return fail(f,MovieAudioProgressError::foreign_source,"acceptance source identity");
    if(rate!=config_.source_rate)return fail(f,MovieAudioProgressError::source_rate_mismatch,"acceptance actual source rate");
    const auto& r=a.submission;
    if(r.reset_epoch!=config_.epoch)return fail(f,MovieAudioProgressError::epoch_mismatch,"receipt epoch");
    if(!r.client_frames||!a.source_frames||a.packet_client_frames!=a.source_frames||a.packet_client_offset>a.source_frames||r.client_frames>a.source_frames-a.packet_client_offset)return fail(f,MovieAudioProgressError::invalid_receipt,"equal-cardinality packet chunk");
    if(!add(a.source_first_frame,a.source_frames)||!add(a.source_first_frame,a.packet_client_offset)||!add(submitted_,r.client_frames)||!add(config_.source_origin,submitted_)||!add(config_.source_origin,submitted_+r.client_frames))return fail(f,MovieAudioProgressError::overflow,"source/client range end");
    if(r.first_client_frame!=submitted_||a.source_first_frame+a.packet_client_offset!=config_.source_origin+submitted_)return fail(f,MovieAudioProgressError::noncontiguous,"contiguous source/client chunks");
    submitted_+=r.client_frames;return true;
}
std::optional<MovieAudioProgressSnapshot> NativeMovieAudioProgress::snapshot(MovieAudioProgressFailure& f) const {
    f={};if(!valid_){fail(f,MovieAudioProgressError::invalid_owner,"moved progress owner");return {};}
    try{return MovieAudioProgressSnapshot{config_.source_identity,config_.epoch,submitted_,retired_,submitted_-retired_,config_.source_origin+retired_,config_.source_rate,paused_,false};}catch(const std::bad_alloc&){fail(f,MovieAudioProgressError::allocation,"owned progress snapshot");return {};}
}
std::optional<MovieAudioProgressSnapshot> NativeMovieAudioProgress::observe(const sound::AudioQueueStatus& s,MovieAudioProgressFailure& f){
    f={};if(!valid_){fail(f,MovieAudioProgressError::invalid_owner,"moved progress owner");return {};}
    if(s.reset_epoch!=config_.epoch){fail(f,MovieAudioProgressError::epoch_mismatch,"status epoch");return {};}
    if(!s.capacity_frames||s.padding_frames>s.capacity_frames||s.padding_frames>s.submitted_frames||s.writable_frames!=s.capacity_frames-s.padding_frames||s.submitted_frames!=submitted_||(paused_&&s.started)){fail(f,MovieAudioProgressError::invalid_status,"status capacity/ledger");return {};}
    const auto r=s.submitted_frames-s.padding_frames;
    if(r<retired_){fail(f,MovieAudioProgressError::regression,"retirement regression");return {};}
    // Allocate the owned output before committing the new retirement.
    auto result=snapshot(f);if(!result)return {};result->retired=r;result->pending=submitted_-r;result->source_boundary=config_.source_origin+r;retired_=r;return result;
}
bool NativeMovieAudioProgress::pause(bool p,MovieAudioProgressFailure& f) noexcept {f={};if(!valid_)return fail(f,MovieAudioProgressError::invalid_owner,"moved progress owner");paused_=p;return true;}
std::optional<MovieAudioProgressSnapshot> NativeMovieAudioProgress::reset(MovieAudioProgressConfig c,const sound::AudioQueueStatus& status,MovieAudioProgressFailure& f){
    f={};if(!valid_){fail(f,MovieAudioProgressError::invalid_owner,"moved progress owner");return {};}
    if(!config(c,f))return {};
    if(!paused_){fail(f,MovieAudioProgressError::invalid_config,"reset requires paused owner");return {};}
    if(config_.epoch==UINT64_MAX){fail(f,MovieAudioProgressError::overflow,"reset epoch");return {};}
    if(c.epoch!=config_.epoch+1){fail(f,MovieAudioProgressError::epoch_mismatch,"next reset epoch");return {};}
    if(c.source_identity!=config_.source_identity||c.source_rate!=config_.source_rate||c.client_rate!=config_.client_rate){fail(f,MovieAudioProgressError::invalid_config,"reset source/rate identity");return {};}
    if(status.reset_epoch!=c.epoch||status.submitted_frames||status.padding_frames||status.started||!status.capacity_frames||status.writable_frames!=status.capacity_frames){fail(f,MovieAudioProgressError::invalid_status,"confirmed empty stopped reset status");return {};}
    auto old=snapshot(f);if(!old)return {};old->cancelled=true;config_=std::move(c);submitted_=retired_=0;return old;
}
}
