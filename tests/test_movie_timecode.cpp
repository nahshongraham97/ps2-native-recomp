#include "mka/movie_timecode.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <initializer_list>
#include <bit>
#include <cfenv>
using namespace mka::video;
void check(bool v,const char* s){if(!v){std::fprintf(stderr,"[timecode-fail] %s\n",s);std::exit(1);}}
MovieTimecodeRational convert(MovieTimecodeDescriptor d){MovieTimecodeFailure f;auto r=convert_movie_timecode(d,f);check(bool(r),"conversion succeeds");return *r;}
void independent_wide_oracle(){
#if defined(__SIZEOF_INT128__)
    // Compiler128 arithmetic is independent of production's signed-magnitude
    // limb product/accumulator. Coefficients below expand original helpers.
    std::uint64_t state=0x82f16a04dc39b75eull;
    const auto random=[&](){state^=state<<13;state^=state>>7;state^=state<<17;return state;};
    constexpr std::int64_t denominator[]={0,23976,24000,25000,29970,30000,50000,59940,60000};
    for(unsigned mode=0;mode<2;++mode)for(unsigned i=0;i<10000;++i){
        MovieTimecodeDescriptor d;d.frame_rate_code=std::uint8_t(1+random()%8);d.drop_frame=(random()&1)!=0;
        auto signed64=[&](){return std::bit_cast<std::int64_t>(random());};
        auto signed32=[&](){return std::int64_t(std::bit_cast<std::int32_t>(std::uint32_t(random())));};
        if(!mode){d.hours=signed64();d.minutes=signed64();d.seconds=signed64();d.pictures=signed64();d.temporal_reference=signed64();d.half_frame_adjustment=signed64();}
        else{d.hours=signed64()/128;d.minutes=-60*d.hours;d.seconds=signed32();d.pictures=signed32();d.temporal_reference=signed32();d.half_frame_adjustment=signed32();}
        const auto code=d.frame_rate_code;
        const std::int64_t nominal=code==1?24000:code==4?30000:code==7?60000:denominator[code];
        const bool drop=d.drop_frame&&(code==1||code==4||code==7);
        const std::int64_t h=drop?(code==1?86292000:code==4?107892000:215892000):3600*nominal;
        const std::int64_t m=drop?(code==1?1438000:code==4?1798000:3598000):60*nominal;
        const __int128 expected=__int128(d.hours)*h+__int128(d.minutes)*m+
            __int128(d.minutes/10)*(drop?2000:0)+__int128(d.seconds)*nominal+
            __int128(d.pictures)*1000+__int128(d.temporal_reference)*1000+__int128(d.half_frame_adjustment)*500;
        const bool fits=expected>=std::numeric_limits<std::int64_t>::min()&&expected<=std::numeric_limits<std::int64_t>::max();
        MovieTimecodeFailure f;auto got=convert_movie_timecode(d,f);
        check(bool(got)==fits,"wide oracle result/overflow classification");
        if(fits)check(got->numerator==static_cast<std::int64_t>(expected)&&got->denominator==denominator[code]&&f.kind==MovieTimecodeError::none,"independent128 exact result");
        else check(f.kind==MovieTimecodeError::overflow,"independent128 exact overflow error");
    }
    std::puts("[timecode-wide-oracle] full_domain=10000 wide_cancellation=10000 compiler128=PASS");
#else
    check(false,"this verification target requires an independent compiler128 integer oracle");
#endif
}
void position_timecode_source_cases(){
    MovieTimecodeFailure f;
    auto pos=[&](std::uint64_t t,std::uint8_t rate,bool drop,std::uint16_t ref){
        auto d=convert_movie_position_timecode(t,rate,drop,ref,f);check(bool(d),"explicit position conversion");return *d;};
    auto expect=[&](std::uint64_t t,std::uint8_t rate,bool drop,std::int64_t h,std::int64_t m,std::int64_t sec,std::int64_t pic){
        const auto d=pos(t,rate,drop,0);check(d.hours==h&&d.minutes==m&&d.seconds==sec&&d.pictures==pic&&
          d.frame_rate_code==rate&&d.drop_frame==drop&&d.temporal_reference==0&&d.half_frame_adjustment==0,
          "literal original inverse-table boundary");};
    // Independent literal source-table boundaries; notably minute60 exists just
    // BEFORE the original inverse hour divisor. Do not normalize these fields.
    expect(1350601,4,true,0,0,59,29);expect(1351351,4,true,0,1,0,2);
    expect(1371622,4,true,0,1,0,29);expect(1372372,4,true,0,1,1,0);
    expect(13498498,4,true,0,9,59,28);expect(13499249,4,true,0,10,0,0);
    expect(80998498,4,true,0,60,0,4);expect(80999249,4,true,1,0,0,0);
    expect(1350976,7,true,0,0,59,59);expect(1351351,7,true,0,1,0,4);
    expect(1371997,7,true,0,1,0,59);expect(1372372,7,true,0,1,1,0);
    expect(13499249,7,true,0,9,59,58);expect(13499625,7,true,0,10,0,0);
    expect(80999249,7,true,0,60,0,4);expect(80999625,7,true,1,0,0,0);
    // Actual preexisting PES census tail timestamps (raw33>>2), with explicit
    // fixture drop=false/ref=0. These do not assert retail branch admission.
    expect(113250,5,false,0,0,5,1);   // LOGON raw453000
    expect(674195,4,false,0,0,29,28); // TITLEN raw2696781
    expect(5885916,4,false,0,4,21,10);// OPENINGN raw23543664
    expect(22500,5,false,0,0,1,0);expect(1350000,5,false,0,1,0,0);
    expect(81000000,5,false,1,0,0,0);
    // Source only special-cases drop at29970/59940; flag on code5 uses the
    // ordinary decomposition while preserving the caller's explicit flag.
    expect(1350000,5,true,0,1,0,0);
    const auto before=pos(187,5,false,0),after=pos(188,5,false,0);
    check(before.pictures==0&&before.half_frame_adjustment==0&&after.pictures==0&&after.half_frame_adjustment==1,
          "source double plus-half floor and half-frame parity");
    const auto second_before=pos(562,5,false,0),second_after=pos(563,5,false,0);
    check(second_before.pictures==0&&second_before.half_frame_adjustment==1&&second_after.pictures==1&&second_after.half_frame_adjustment==0,
          "second half-frame rounding boundary");
    const auto adjusted=pos(22500,5,false,5);check(adjusted.seconds==0&&adjusted.pictures==25&&adjusted.temporal_reference==5,
          "subtract temporal reference before decomposition; retain explicit field");
    const auto clamped=pos(0,5,false,1023);check(clamped.hours==0&&clamped.pictures==0&&clamped.temporal_reference==1023,
          "negative adjusted frame count clamps at source zero");
    check(!convert_movie_position_timecode(std::nullopt,5,false,0,f)&&f.kind==MovieTimecodeError::missing_timestamp,"missing timestamp is not zero");
    check(!convert_movie_position_timecode(0,0,false,0,f)&&f.kind==MovieTimecodeError::unsupported_rate,"position unsupported rate");
    check(!convert_movie_position_timecode(0,5,false,1024,f)&&f.kind==MovieTimecodeError::invalid_temporal_reference,"serialized ten-bit temporal reference");
    const auto wide=pos(std::numeric_limits<std::uint64_t>::max(),8,false,0);
    check(wide.hours>std::numeric_limits<std::int32_t>::max(),"native position descriptor has no original word/hour cap");
    check(!convert_movie_timecode(wide,f)&&f.kind==MovieTimecodeError::overflow,"genuine native rational output overflow remains named");
    const auto original_round=std::fegetround();check(std::fesetround(FE_DOWNWARD)==0,"set test rounding mode");
    check(!convert_movie_position_timecode(188,5,false,0,f)&&f.kind==MovieTimecodeError::unsupported_fp_environment,"no rounding-environment-dependent result");
    check(std::fesetround(original_round)==0,"restore test rounding mode");
}
int main(){
    position_timecode_source_cases();
    // Original instruction constants: drop24 hour86292/minute1438;
    // drop30 hour107892/minute1798;drop60 hour215892/minute3598 (source uses two, not four).
    check(convert({1,true,1,1,1,1,2,1})==MovieTimecodeRational{87757500,23976},"original24drop constants+half");
    check(convert({4,true,1,1,1,1,2,1})==MovieTimecodeRational{109723500,29970},"original30drop constants+half");
    check(convert({7,true,1,1,1,1,2,1})==MovieTimecodeRational{219553500,59940},"original60drop constants+half");
    check(convert({4,true,0,10,0,0,0,0})==MovieTimecodeRational{17982000,29970},"tenth minute restoration");
    check(convert({4,false,0,1,0,0,0,0})==MovieTimecodeRational{1800000,29970},"drop0 retains retail29970 denominator");
    constexpr unsigned rate[]={0,23976,24000,25000,29970,30000,50000,59940,60000};
    for(unsigned c=1;c<=8;++c){const auto nominal=(c==1?24000:c==4?30000:c==7?60000:rate[c]);
        for(int h=-3;h<=3;++h)for(int m=-12;m<=12;++m){
            MovieTimecodeDescriptor d{static_cast<std::uint8_t>(c),false,h,m,7,11,1023,-2};
            const auto expected=std::int64_t(h)*3600*nominal+std::int64_t(m)*60*nominal+7*nominal+(11+1023)*1000-1000;
            check(convert(d)==MovieTimecodeRational{expected,rate[c]},"generic reference expansion");
            d.drop_frame=true;if(c==2||c==3||c==5||c==6||c==8)check(convert(d).numerator==expected,"integer codes ignore drop branch");
        }
    }
    check(convert({4,true,0,-11,0,0,0,0})==MovieTimecodeRational{-19780000,29970},"negative minutes division truncateszero");
    // Independently expanded original branch constants, without the production
    // coefficient construction or wide accumulator.
    for(unsigned c:{1u,4u,7u})for(int m=0;m<60;++m){
        const std::int64_t hour=c==1?86292:c==4?107892:215892;
        const std::int64_t minute=c==1?1438:c==4?1798:3598;
        const std::int64_t second=c==1?24:c==4?30:60;
        const auto expected=(hour*2+minute*m+2*(m/10)+second*59+21+1023)*1000-500;
        check(convert({static_cast<std::uint8_t>(c),true,2,m,59,21,1023,-1})==MovieTimecodeRational{expected,rate[c]},"drop reference constants all minute boundaries");
    }
    const auto large=std::int64_t(7000000000ll);
    check(convert({2,false,0,0,0,large,0,0})==MovieTimecodeRational{large*1000,24000},"native input above32bit");
    check(convert({8,false,100000000000000ll,-6000000000000000ll,0,0,0,0}).numerator==0,"wide intermediate exact cancellation");
    MovieTimecodeFailure f;
    check(!convert_movie_timecode({8,false,std::numeric_limits<std::int64_t>::max(),0,0,0,0,0},f)&&f.kind==MovieTimecodeError::overflow,"positive64 overflow");
    check(!convert_movie_timecode({8,false,std::numeric_limits<std::int64_t>::min(),0,0,0,0,0},f)&&f.kind==MovieTimecodeError::overflow,"negative64 overflow");
    check(!convert_movie_timecode({0},f)&&f.kind==MovieTimecodeError::unsupported_rate,"sentinel rate0 unsupported");
    check(!convert_movie_timecode({9},f)&&f.kind==MovieTimecodeError::unsupported_rate,"reserved rate unsupported");
    Mpeg2PictureTiming p;p.sequence.frame_rate_code=4;p.temporal_reference=17;
    check(!convert_movie_picture_timecode(p,f)&&f.kind==MovieTimecodeError::missing_gop,"no guessed GOP origin");
    p.gop=Mpeg2GopTiming{};p.gop->hours=1;p.gop->minutes=2;p.gop->seconds=3;p.gop->pictures=4;
    auto r=convert_movie_picture_timecode(p,f);check(r&&*r==convert({4,false,1,2,3,4,17,0}),"source descriptor mapping");
    p.sequence.extension_n=3;p.sequence.extension_d=31;p.top_field_first=true;p.repeat_first_field=true;p.progressive_frame=false;
    check(convert_movie_picture_timecode(p,f)==r,"unread source flags and extension ratios do not substitute rate");
    independent_wide_oracle();
    std::puts("[timecode-pass] original rate/drop branches and checked native64 arithmetic");return 0;
}
