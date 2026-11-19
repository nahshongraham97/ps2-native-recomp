#include "mka/movie_frame_eligibility.hpp"
#include <new>
#include <utility>
namespace mka::video {
std::unique_ptr<NativeMovieFrameEligibility> NativeMovieFrameEligibility::create(MovieFrameEligibilityConfig c,MovieFrameEligibilityFailure& f) noexcept {
    f={};
    if(c.frequency<=0||(c.mode!=MovieFrameEligibilityMode::direct&&c.mode!=MovieFrameEligibilityMode::adaptive)){f.kind=MovieFrameEligibilityError::invalid_config;return {};}
    try{return std::unique_ptr<NativeMovieFrameEligibility>(new NativeMovieFrameEligibility(c));}
    catch(const std::bad_alloc&){f.kind=MovieFrameEligibilityError::allocation;return {};}
}
NativeMovieFrameEligibility::NativeMovieFrameEligibility(NativeMovieFrameEligibility&& o) noexcept:config_(o.config_),state_(o.state_),valid_(std::exchange(o.valid_,false)){}
NativeMovieFrameEligibility& NativeMovieFrameEligibility::operator=(NativeMovieFrameEligibility&& o) noexcept {if(this!=&o){config_=o.config_;state_=o.state_;valid_=std::exchange(o.valid_,false);}return *this;}
std::optional<MovieFrameEligibilityState> NativeMovieFrameEligibility::snapshot(MovieFrameEligibilityFailure& f) const noexcept {f={};if(!valid_){f.kind=MovieFrameEligibilityError::invalid_owner;return {};}return state_;}
std::optional<MovieFrameEligibilityResult> NativeMovieFrameEligibility::evaluate(MovieClockRational clock,MovieTimecodeRational target,std::int64_t rate,MovieFrameEligibilityFailure& f) noexcept {
    f={};if(!valid_){f.kind=MovieFrameEligibilityError::invalid_owner;return {};}
    auto metrics=evaluate_movie_eligibility_numeric(clock,target,config_.frequency,config_.margin,config_.tolerance,f.numeric);
    if(!metrics){f.kind=MovieFrameEligibilityError::numeric;return {};}
    auto next=state_;
    MovieFrameEligibilityResult result{false,MovieFrameEligibilityBranch::direct,*metrics,next};
    if(config_.mode==MovieFrameEligibilityMode::direct)result.eligible=metrics->direct_eligible;
    else if(metrics->early){result.branch=MovieFrameEligibilityBranch::early;}
    else if(metrics->late){
        result.eligible=true;
        if(metrics->target_units==next.previous_middle||metrics->target_units==next.previous_late)result.branch=MovieFrameEligibilityBranch::late_duplicate;
        else {
            if(next.repeated_late==UINT64_MAX){f.kind=MovieFrameEligibilityError::overflow;return {};}
            next.previous_late=metrics->target_units;++next.repeated_late;result.branch=MovieFrameEligibilityBranch::late_new;
        }
    }else{
        const std::uint64_t threshold=config_.frequency==59940&&rate<3&&config_.step==1000?1:0;
        result.eligible=next.repeated_late>threshold?metrics->direct_eligible:next.previous_middle_result;
        next.repeated_late=0;next.previous_middle=metrics->target_units;next.previous_middle_result=result.eligible;result.branch=MovieFrameEligibilityBranch::middle;
    }
    result.state=next;state_=next;return result;
}
}
