#include "mka/movie_clock_math.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
using namespace mka::video;
static void check(bool v,const char* label) {
    if(!v) { std::fprintf(stderr,"FAIL: %s\n",label);std::exit(1); }
}
static void row(const char* operation,std::initializer_list<std::int64_t> inputs,
                std::optional<std::int64_t> result,MovieClockMathFailure f) {
    std::printf("{\"operation\":\"%s\",\"inputs\":[",operation);
    bool first=true;for(auto v:inputs) { std::printf("%s%lld",first?"":",",static_cast<long long>(v));first=false; }
    std::printf("],\"result\":");
    if(result) std::printf("%lld",static_cast<long long>(*result));else std::printf("null");
    std::printf(",\"error\":\"%s\"}\n",movie_clock_math_error_name(f.kind));
}
int main() {
    constexpr auto lo=std::numeric_limits<std::int64_t>::min();
    constexpr auto hi=std::numeric_limits<std::int64_t>::max();
    MovieClockMathFailure f;
    auto scale=[&](auto a,auto b,auto den) { auto r=scale_movie_clock(a,b,den,f);row("scale",{a,b,den},r,f);return r; };
    auto extrapolate=[&](auto a,auto ad,auto now,auto then,auto td) {
        auto r=extrapolate_movie_clock(a,ad,now,then,td,f);row("extrapolate",{a,ad,now,then,td},r,f);return r;
    };
    check(scale(-7,3,2)==-10,"negative truncation");
    check(scale(7,3,-2)==-10,"negative divisor");
    check(scale(-7,-3,-2)==-10,"three signs");
    check(scale(lo,lo,lo)==lo,"wide product representable negative result");
    check(scale(hi,hi,hi)==hi,"wide product representable positive result");
    check(scale(lo,0,-1)==0,"zero result extreme divisor");
    check(!scale(lo,-1,1)&&f.kind==MovieClockMathError::overflow,"final positive overflow");
    check(!scale(lo,lo,1)&&f.kind==MovieClockMathError::overflow,"huge quotient overflow");
    check(!scale(0,0,0)&&f.kind==MovieClockMathError::zero_divisor,"zero divisor even zero numerator");
    check(extrapolate(lo,1,hi,lo,1)==hi,"wide delta and quotient cancel anchor");
    check(extrapolate(hi,1,lo,hi,1)==lo,"negative wide delta cancels anchor");
    check(extrapolate(0,1,hi,lo,3)==6148914691236517205LL,"wide delta divides to representable result");
    check(extrapolate(5,3,-7,0,2)==-5,"truncate before anchor add");
    check(extrapolate(-7,hi,hi,hi,1)==-7,"zero delta");
    check(!extrapolate(hi,hi,hi,lo,1)&&f.kind==MovieClockMathError::overflow,"largest positive intermediate");
    check(!extrapolate(lo,hi,lo,hi,1)&&f.kind==MovieClockMathError::overflow,"largest negative intermediate");
    check(!extrapolate(0,1,0,0,0)&&f.kind==MovieClockMathError::zero_divisor,"timer zero denominator");
    check(!extrapolate(0,0,0,0,1)&&f.kind==MovieClockMathError::invalid_denominator,"audio zero denominator");
    check(!extrapolate(0,-1,0,0,1)&&f.kind==MovieClockMathError::invalid_denominator,"audio negative denominator");
    check(!extrapolate(0,1,0,0,-1)&&f.kind==MovieClockMathError::invalid_denominator,"timer negative denominator");
    for(std::int64_t i=-100;i<=100;++i) {
        scale(i*1234567,i*97+3,(i%17)?i%17:1);
        extrapolate(i*37,(i+101)*13,i*654321,-i*76543,(i+101)*7);
    }
    check(std::fflush(stdout)==0&&!std::ferror(stdout),"complete trace output");
    std::fprintf(stderr,"PASS: checked native rational literals and trace; no clock/player binding\n");
}
