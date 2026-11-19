#include "mka/movie_clock_filter.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>
using namespace mka::video;
struct Op { const char* kind;MovieClockRational audio,timer,increment;int value;const char* json; };
struct Group { const char* name;MovieClockFilterConfig config;std::vector<Op> operations; };
#include "generated/movie_clock_filter_plan.inc"
static void check(bool b,const char* message) { if(!b) { std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1); } }
static const char* boolean(bool v) { return v?"true":"false"; }
static void integer(std::int64_t v) { std::printf("%lld",static_cast<long long>(v)); }
static void pair(MovieClockRational r) { std::printf("[");integer(r.numerator);std::printf(",");integer(r.denominator);std::printf("]"); }
static void snapshot(const NativeMovieClockFilter& owner) {
    const auto& c=owner.config();const auto& s=owner.state();const auto& v=s.statistics;
    std::printf("{\"config\":{\"enabled\":%s,\"paused\":%s,\"mode\":%d,\"history_size\":%zu,\"quantum\":",boolean(c.enabled),boolean(c.paused),static_cast<int>(c.mode),c.history_size);
    pair(c.quantum);std::printf(",\"large_error\":");pair(c.large_error);std::printf(",\"first_offset\":");pair(c.first_offset);std::printf(",\"restart_offset\":");pair(c.restart_offset);
    std::printf("},\"state\":{\"active\":%s,\"anchor\":",boolean(s.active));
    if(s.anchor)pair({s.anchor->timer,s.anchor->audio});else std::printf("null");
    std::printf(",\"floor\":");integer(s.audio_floor);std::printf(",\"previous_audio\":");pair(s.previous_audio);std::printf(",\"previous_timer\":");pair(s.previous_timer);std::printf(",\"previous_output\":");pair(s.previous_output);
    std::printf(",\"history\":[");bool first=true;for(auto x:owner.history()) {if(!first)std::printf(",");first=false;integer(x);}
    std::printf("],\"cursor\":%zu,\"mean_before\":",owner.history_cursor());integer(s.mean_before);std::printf(",\"mean_after\":");integer(s.mean_after);
    std::printf(",\"resets\":%llu,\"large_resets\":%llu,\"inactive_reanchors\":%llu,\"positive_adjustments\":%llu,\"negative_adjustments\":%llu",static_cast<unsigned long long>(v.resets),static_cast<unsigned long long>(v.large_resets),static_cast<unsigned long long>(v.inactive_reanchors),static_cast<unsigned long long>(v.positive_adjustments),static_cast<unsigned long long>(v.negative_adjustments));
    std::printf(",\"inactive_min\":");integer(v.inactive_min);std::printf(",\"inactive_max\":");integer(v.inactive_max);std::printf(",\"active_min\":");integer(v.active_min);std::printf(",\"active_max\":");integer(v.active_max);std::printf("}}");
}
static void result(const MovieClockFilterUpdate& r,const MovieClockFilterState& before,const MovieClockFilterConfig& c) {
    std::printf("{\"clamped_audio\":");pair(r.clamped_audio);std::printf(",\"output\":");pair(r.output);std::printf(",\"observations\":[");bool first=true;
    auto flag=[&](bool condition,const char* text) {if(condition) {std::printf("%s\"%s\"",first?"":",",text);first=false;}};
    const auto& o=r.observations;
    flag(o.bypass,"bypass");
    if(!o.bypass) {
        flag(c.paused,"paused");flag(o.first_anchor,"first_offset");flag(o.restart_anchor,"restart_offset");
        flag(!c.paused&&!before.active&&r.clamped_audio.numerator<=before.audio_floor&&c.mode==MovieClockFilterMode::mode0,"mode0_activation_without_reanchor");
        flag(o.inactive_reanchor,"inactive_reanchor");flag(o.large_reset,"large_reset");flag(o.inserted,"insert");flag(o.corrected,"correction");flag(o.output_clamped,"last_pair_clamp");
    }
    std::printf("]}");
}
int main() {
    for(const auto& group:groups) {
        MovieClockFilterFailure failure;auto owner=NativeMovieClockFilter::create(group.config,failure);
        std::printf("{\"case\":\"%s\",\"op\":\"create\",\"ok\":%s,\"error\":\"%s\",\"snapshot\":",group.name,boolean(owner.has_value()),movie_clock_filter_error_name(failure.kind));
        if(owner)snapshot(*owner);else std::printf("null");std::printf("}\n");if(!owner)continue;
        for(std::size_t ordinal=0;ordinal<group.operations.size();++ordinal) {
            const auto& op=group.operations[ordinal];const auto before=owner->state();const auto config=owner->config();bool ok=false,changed=false;std::optional<MovieClockFilterUpdate> value;
            if(!std::strcmp(op.kind,"update")) {value=owner->update(op.audio,op.timer,failure);ok=value.has_value();}
            else if(!std::strcmp(op.kind,"set_enabled"))ok=owner->set_enabled(op.value!=0,failure);
            else if(!std::strcmp(op.kind,"set_paused"))ok=owner->set_paused(op.value!=0,failure);
            else if(!std::strcmp(op.kind,"set_mode"))ok=owner->set_mode(static_cast<MovieClockFilterMode>(op.value),failure);
            else if(!std::strcmp(op.kind,"clear_history"))ok=owner->clear_history(failure);
            else if(!std::strcmp(op.kind,"add_previous_output"))ok=owner->add_previous_output(op.increment,changed,failure);
            else check(false,"unknown fixed input operation");
            std::printf("{\"case\":\"%s\",\"ordinal\":%zu,\"input\":%s,\"ok\":%s,\"error\":\"%s\",\"result\":",group.name,ordinal,op.json,boolean(ok),movie_clock_filter_error_name(failure.kind));
            if(value)result(*value,before,config);else std::printf("null");std::printf(",\"snapshot\":");snapshot(*owner);std::printf("}\n");
        }
    }
    // Separate API lifetime assertions; these do not supply oracle expectations.
    MovieClockFilterFailure failure;auto owner=NativeMovieClockFilter::create(MovieClockFilterConfig::source_defaults(),failure);check(owner.has_value(),"lifetime create");
    auto retained=owner->update({100,1000},{0,1000},failure);check(retained.has_value(),"retained update");const auto retained_copy=*retained;const auto before=owner->state();
    NativeMovieClockFilter moved(std::move(*owner));check(moved.valid()&&!owner->valid()&&moved.state()==before,"move construction");
    check(!owner->update({1,1},{1,1},failure)&&failure.kind==MovieClockFilterError::invalid_owner,"moved update");
    check(!owner->clear_history(failure)&&failure.kind==MovieClockFilterError::invalid_owner,"moved clear");
    check(!owner->set_enabled(true,failure)&&failure.kind==MovieClockFilterError::invalid_owner,"moved enabled");
    check(!owner->set_paused(false,failure)&&failure.kind==MovieClockFilterError::invalid_owner,"moved paused");
    check(!owner->set_mode(MovieClockFilterMode::mode0,failure)&&failure.kind==MovieClockFilterError::invalid_owner,"moved mode");
    bool changed=true;check(!owner->add_previous_output({1,1},changed,failure)&&failure.kind==MovieClockFilterError::invalid_owner,"moved increment");
    auto destination=NativeMovieClockFilter::create(MovieClockFilterConfig::source_defaults(),failure);check(destination.has_value(),"destination create");*destination=std::move(moved);check(destination->valid()&&!moved.valid()&&destination->state()==before,"move assignment");
    auto* self=&*destination;*destination=std::move(*self);check(destination->valid()&&destination->state()==before,"self move");owner.reset();destination.reset();
    check(retained->clamped_audio==retained_copy.clamped_audio&&retained->output==retained_copy.output,"retained result lifetime");
    check(std::fflush(stdout)==0&&!std::ferror(stdout),"complete output");std::fprintf(stderr,"PASS: fixed filter input trace and separate lifetime assertions; no clock/player binding\n");
}
