#include "mka/movie_service_clock.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>
using namespace mka::video;
struct Op { const char* kind;MovieServiceMaintenanceGates gates;bool admitted;std::int64_t value;MovieClockRational rational;const char* json; };
struct Group { const char* name;bool source_mode;std::int64_t mode;MovieServiceClockConfig config;const char* factory_json;std::vector<Op> operations; };
#include "generated/movie_service_clock_plan.inc"
static void check(bool ok,const char* text) { if(!ok) { std::fprintf(stderr,"FAIL: %s\n",text);std::exit(1); } }
static const char* boolean(bool x) { return x?"true":"false"; }
static void integer(std::int64_t x) { std::printf("%lld",static_cast<long long>(x)); }
static void snapshot(const NativeMovieServiceClock& owner) {
    const auto& s=owner.state();std::printf("{\"frequency\":");integer(s.frequency);std::printf(",\"step\":");integer(s.step);std::printf(",\"counter\":");integer(s.counter);std::printf(",\"retained\":");if(s.retained)integer(*s.retained);else std::printf("null");std::printf("}");
}
int main() {
    for(const auto& group:groups) {
        MovieServiceClockFailure f;std::optional<NativeMovieServiceClock> owner;
        if(group.source_mode) {auto config=movie_service_source_config(static_cast<MovieServiceSourceMode>(group.mode),f);if(config)owner=NativeMovieServiceClock::create(*config,f);}
        else owner=NativeMovieServiceClock::create(group.config,f);
        std::printf("{\"case\":\"%s\",\"op\":\"create\",\"input\":%s,\"ok\":%s,\"error\":\"%s\",\"snapshot\":",group.name,group.factory_json,boolean(owner.has_value()),movie_service_clock_error_name(f.kind));if(owner)snapshot(*owner);else std::printf("null");std::printf("}\n");if(!owner)continue;
        for(std::size_t ordinal=0;ordinal<group.operations.size();++ordinal) {
            const auto& op=group.operations[ordinal];bool ok=false;std::optional<MovieServiceMaintenance> maintenance;std::optional<MovieServiceClockRead> read;std::optional<MovieServiceRateProjection> projection;std::optional<bool> bit;
            if(!std::strcmp(op.kind,"maintenance")) {maintenance=owner->maintenance(op.gates,f);ok=maintenance.has_value();}
            else if(!std::strcmp(op.kind,"read")) {read=owner->read(op.admitted,f);ok=read.has_value();}
            else if(!std::strcmp(op.kind,"project_rate")) {projection=owner->project_rate(op.value,f);ok=projection.has_value();}
            else if(!std::strcmp(op.kind,"eligibility")) {bit=owner->eligible(op.rational,f);ok=bit.has_value();}
            else if(!std::strcmp(op.kind,"retained_less_than")) {bit=owner->retained_less_than(op.value,f);ok=bit.has_value();}
            else if(!std::strcmp(op.kind,"set_step"))ok=owner->set_step(op.value,f);
            else if(!std::strcmp(op.kind,"assign_retained"))ok=owner->assign_retained(op.value,f);
            else if(!std::strcmp(op.kind,"invalidate_retained"))ok=owner->invalidate_retained(f);
            else if(!std::strcmp(op.kind,"increment"))ok=owner->increment(op.rational,f);
            else if(!std::strcmp(op.kind,"action2_increment"))ok=owner->increment_rate(op.value,f);
            else check(false,"unknown fixed operation");
            std::printf("{\"case\":\"%s\",\"ordinal\":%zu,\"input\":%s,\"ok\":%s,\"error\":\"%s\",\"result\":",group.name,ordinal,op.json,boolean(ok),movie_service_clock_error_name(f.kind));
            if(maintenance)std::printf("{\"counter_advanced\":%s,\"retained_advanced\":%s}",boolean(maintenance->counter_advanced),boolean(maintenance->retained_advanced));
            else if(read) {std::printf("{\"available\":%s,\"value\":",boolean(read->value.has_value()));if(read->value) {std::printf("[");integer(read->value->numerator);std::printf(",");integer(read->value->denominator);std::printf("]");}else std::printf("null");std::printf("}");}
            else if(projection) {std::printf("{\"available\":%s,\"value\":",boolean(projection->value.has_value()));if(projection->value)integer(*projection->value);else std::printf("null");std::printf("}");}
            else if(bit.has_value())std::printf("%s",boolean(*bit));else std::printf("null");
            std::printf(",\"snapshot\":");snapshot(*owner);std::printf("}\n");
        }
    }
    // Ownership assertions are separate from the fixed-plan JSON trace.
    MovieServiceClockFailure f;auto source=NativeMovieServiceClock::create({59940,1000},f);check(source.has_value(),"lifetime create");check(source->increment({7,3},f),"lifetime increment");auto retained=source->read(true,f);check(retained&&retained->value,"owned read");auto copy=*retained->value;auto state=source->state();
    NativeMovieServiceClock moved(std::move(*source));check(moved.valid()&&!source->valid()&&moved.state()==state,"move construction");
    auto rejected=[&](bool success) {check(!success&&f.kind==MovieServiceClockError::invalid_owner,"moved operation rejects");};
    rejected(source->maintenance({true,true,true,true},f).has_value());rejected(source->read(false,f).has_value());rejected(source->set_step(1,f));rejected(source->assign_retained(0,f));rejected(source->invalidate_retained(f));rejected(source->increment({0,1},f));rejected(source->increment_rate(0,f));rejected(source->project_rate(0,f).has_value());rejected(source->eligible({0,1},f).has_value());rejected(source->retained_less_than(0,f).has_value());
    auto destination=NativeMovieServiceClock::create({1,0},f);check(destination.has_value(),"destination create");*destination=std::move(moved);check(destination->valid()&&!moved.valid()&&destination->state()==state,"move assignment");auto* self=&*destination;*destination=std::move(*self);check(destination->valid()&&destination->state()==state,"self move retained");source.reset();destination.reset();check(retained->value&&*retained->value==copy,"owned output survives destruction");
    check(std::fflush(stdout)==0&&!std::ferror(stdout),"complete trace output");std::fprintf(stderr,"PASS: fixed service clock trace and separate ownership assertions; no cadence/player\n");
}
