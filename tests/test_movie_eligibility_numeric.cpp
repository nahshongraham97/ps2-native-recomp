#include "mka/movie_eligibility_numeric.hpp"
#include "generated/movie_eligibility_numeric_plan.inc"
#include <bit>
#include <cstdio>
using namespace mka::video;
static const char* boolean(bool x) { return x?"true":"false"; }
static void metric(const char* name,double value) {
    std::printf("\"%s\":\"%016llx\"",name,static_cast<unsigned long long>(std::bit_cast<std::uint64_t>(value)));
}
int main() {
    for(const auto& input:numeric_inputs) {
        MovieEligibilityNumericFailure failure;
        const auto r=evaluate_movie_eligibility_numeric({input.clock_n,input.clock_d},{input.target_n,input.target_d},input.frequency,input.margin,input.tolerance,failure);
        std::printf("{\"case\":\"%s\",\"domain\":\"%s\",\"input\":%s,\"ok\":%s,\"error\":\"%s\",\"result\":",input.case_id,input.domain,input.json,boolean(r.has_value()),movie_eligibility_numeric_error_name(failure.kind));
        if(!r)std::printf("null");
        else {
            std::printf("{\"adjusted_clock_numerator\":%lld,",static_cast<long long>(r->adjusted_clock_numerator));
            metric("target_units_bits",r->target_units);std::printf(",");metric("clock_units_bits",r->clock_units);std::printf(",");metric("early_boundary_bits",r->early_boundary);std::printf(",");metric("late_boundary_bits",r->late_boundary);
            std::printf(",\"direct_eligible\":%s,\"early\":%s,\"late\":%s}",boolean(r->direct_eligible),boolean(r->early),boolean(r->late));
        }
        std::printf("}\n");
    }
    if(std::fflush(stdout)!=0||std::ferror(stdout)) {std::fprintf(stderr,"FAIL: complete numeric trace output\n");return 1;}
    std::fprintf(stderr,"PASS: numeric fixed-input trace emitted; comparison is external, no player proof\n");
}
