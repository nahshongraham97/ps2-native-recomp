#include "mka/movie_frame_eligibility.hpp"
#include <cstdio>
#include <stdexcept>
using namespace mka::video;
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(){try{
    MovieFrameEligibilityFailure f;
    auto direct=NativeMovieFrameEligibility::create({59940,1000,0,1,MovieFrameEligibilityMode::direct},f);
    check(bool(direct),"direct create");auto initial=*direct->snapshot(f);
    check(initial.repeated_late==100&&initial.previous_late==-1&&initial.previous_middle==-1&&!initial.previous_middle_result,"source literal initial state");
    auto equal=direct->evaluate({10,10000},{10,10000},1,f);check(equal&&equal->eligible&&equal->state==initial,"direct equality immutable");
    check(!direct->evaluate({0,0},{0,1},1,f)&&f.kind==MovieFrameEligibilityError::numeric&&*direct->snapshot(f)==initial,"numeric error atomic");
    auto adaptive=NativeMovieFrameEligibility::create({59940,1000,0,2,MovieFrameEligibilityMode::adaptive},f);
    auto first=adaptive->evaluate({0,10000},{1,10000},1,f);check(first&&!first->eligible&&first->branch==MovieFrameEligibilityBranch::middle&&first->state.repeated_late==0,"initial middle strict direct false");
    auto late=adaptive->evaluate({10,10000},{2,10000},1,f);check(late&&late->eligible&&late->state.repeated_late==1,"one new late");
    auto duplicate=adaptive->evaluate({10,10000},{2,10000},1,f);check(duplicate&&duplicate->branch==MovieFrameEligibilityBranch::late_duplicate&&duplicate->state==late->state,"duplicate late no counter write");
    auto middle_duplicate=adaptive->evaluate({10,10000},{1,10000},1,f);check(middle_duplicate&&middle_duplicate->branch==MovieFrameEligibilityBranch::late_duplicate&&middle_duplicate->state==late->state,"middle cache duplicate late");
    auto middle=adaptive->evaluate({3,10000},{3,10000},1,f);check(middle&&!middle->eligible&&middle->state.repeated_late==0,"threshold one preserves cached false at count one");
    int factor=0;
    for(auto cfg:{MovieFrameEligibilityConfig{59940,1000,0,2,MovieFrameEligibilityMode::adaptive},MovieFrameEligibilityConfig{60000,1000,0,2,MovieFrameEligibilityMode::adaptive},MovieFrameEligibilityConfig{59940,999,0,2,MovieFrameEligibilityMode::adaptive}}){
        const auto rate=factor++==0?3:1;auto p=NativeMovieFrameEligibility::create(cfg,f);check(bool(p),"isolated factor owner");auto seed=p->evaluate({0,10000},{1,10000},rate,f);check(seed&&!seed->eligible&&seed->branch==MovieFrameEligibilityBranch::middle&&seed->state.repeated_late==0,"isolated factor middle setup");auto one_late=p->evaluate({10,10000},{2,10000},rate,f);check(one_late&&one_late->branch==MovieFrameEligibilityBranch::late_new&&one_late->state.repeated_late==1,"isolated factor late setup");auto r=p->evaluate({3,10000},{3,10000},rate,f);check(r&&r->eligible,"metadata3 or alternate frequency/step threshold zero");
    }
    auto minus_one=NativeMovieFrameEligibility::create({59940,1000,0,1,MovieFrameEligibilityMode::adaptive},f);auto sentinel_duplicate=minus_one->evaluate({10,10000},{-1,10000},1,f);check(sentinel_duplicate&&sentinel_duplicate->eligible&&sentinel_duplicate->branch==MovieFrameEligibilityBranch::late_duplicate&&sentinel_duplicate->state.repeated_late==100,"source initialized minus-one cache participates in duplicate rule");
    auto mutated=*adaptive->snapshot(f);check(!adaptive->evaluate({1,0},{3,10000},1,f)&&f.kind==MovieFrameEligibilityError::numeric&&*adaptive->snapshot(f)==mutated,"adaptive numeric error after mutation preserves complete state");
    auto negative=NativeMovieFrameEligibility::create({59940,1000,0,-1,MovieFrameEligibilityMode::adaptive},f);auto both=negative->evaluate({0,10000},{0,10000},1,f);check(both&&both->metrics.early&&both->metrics.late&&!both->eligible&&both->branch==MovieFrameEligibilityBranch::early&&both->state.repeated_late==100,"early first when negative tolerance overlaps");
    NativeMovieFrameEligibility moved(std::move(*adaptive));check(!adaptive->snapshot(f)&&f.kind==MovieFrameEligibilityError::invalid_owner,"moved snapshot rejects");check(!adaptive->evaluate({0,1},{0,1},1,f),"moved evaluate rejects");moved=std::move(moved);check(*moved.snapshot(f)==middle->state,"selfmove preserves");adaptive.reset();check(middle->state.previous_middle==3,"owned result lifetime");
    check(!NativeMovieFrameEligibility::create({0,1,0,0,MovieFrameEligibilityMode::direct},f)&&f.kind==MovieFrameEligibilityError::invalid_config,"invalid frequency");
    std::puts("PASS pure ordinary frame eligibility; no clock/target producer or player binding");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
