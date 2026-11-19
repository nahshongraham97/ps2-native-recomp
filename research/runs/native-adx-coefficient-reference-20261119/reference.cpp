// OFFLINE business-reference fixture only. Never link into accepted runtime.
// Literal source sequence; no instruction decoder, guest state or memory.
#include "ps2/fpu.hpp"
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
using U=std::uint32_t;
namespace {
struct Reference {
    const char* name;
    U flags=0;
    static void require(bool yes,const char* message){if(!yes)throw std::runtime_error(message);}
    U trace(const char* pc,const char* operation,U a,U b,U acc,U result){
        std::printf("{\"case\":\"%s\",\"pc\":\"%s\",\"operation\":\"%s\",\"a\":\"%08x\",\"b\":\"%08x\",\"acc\":\"%08x\",\"bits\":\"%08x\",\"helper_flags\":\"%08x\"}\n",name,pc,operation,a,b,acc,result,flags);
        return result;
    }
    U number(const char* pc,std::int32_t value){return trace(pc,"cvt_s_w",std::bit_cast<U>(value),0,0,ps2::fp::cvt_s_w(std::bit_cast<U>(value)));}
    U mul(const char* pc,U a,U b){return trace(pc,"mul",a,b,0,ps2::fp::ee::mul(flags,a,b));}
    U add(const char* pc,U a,U b){return trace(pc,"add",a,b,0,ps2::fp::ee::add(flags,a,b));}
    U sub(const char* pc,U a,U b){return trace(pc,"sub",a,b,0,ps2::fp::ee::sub(flags,a,b));}
    U div(const char* pc,U a,U b){require((b&0x7fffffff)!=0,"unsupported zero divisor");return trace(pc,"div",a,b,0,ps2::fp::ee::div(flags,a,b));}
    U root(const char* pc,U a){
        require(!(a&0x80000000u)&&ps2::fp::exponent(a)>0&&ps2::fp::exponent(a)<255,"unsupported nonpositive/non-normal square-root domain");
        return trace(pc,"sqrt",a,0,0,ps2::fp::ee::sqrt(flags,a));
    }
    void self_equal(const char* pc,U value){
        // Exact preserved emitter CEq compares key(fs)==key(ft). Here fs==ft.
        // These fixed cases must remain finite/normal; fallback is not guessed.
        require(ps2::fp::exponent(value)>0&&ps2::fp::exponent(value)<255,"unsupported root result/fallback domain");
        const bool yes=ps2::fp::key(value)==ps2::fp::key(value);
        trace(pc,"self_equal_skip_fallback",value,value,0,yes?1u:0u);
        require(yes,"unsupported original sqrt fallback");
    }
    U cosine_small(U x){
        require((x&0x7fffffffu)<=0x3f490fd8u,"unsupported cosine reduction branch");
        require((x&0x7fffffffu)>0x31ffffffu&&(x&0x7fffffffu)<0x3e99999au,"unsupported tiny/large cosine branch");
        trace("00121D7C","direct_cos_kernel",x,0,0,1);
        const U z=mul("001202D8",x,x);
        U polynomial=mul("001202F4",z,0xad47d74eu);
        polynomial=add("00120304",polynomial,0x310f74f5u);
        polynomial=mul("00120310",z,polynomial);
        polynomial=add("0012031C",polynomial,0xb493f27bu);
        polynomial=mul("00120324",z,polynomial);
        polynomial=add("00120330",polynomial,0x37d00d00u);
        polynomial=mul("00120338",z,polynomial);
        polynomial=add("00120344",polynomial,0xbab60b60u);
        polynomial=mul("0012034C",z,polynomial);
        polynomial=add("00120354",polynomial,0x3d2aaaaau);
        polynomial=mul("0012035C",z,polynomial);
        const U acc=trace("00120364","mula",z,polynomial,0,ps2::fp::ee::mul(flags,z,polynomial));
        const U residual=trace("0012036C","msub",x,0,acc,ps2::fp::ee::msub(flags,acc,x,0));
        U half=mul("0012037C",z,0x3f000000u);
        half=sub("00120380",half,residual);
        return sub("0012038C",0x3f800000u,half);
    }
    void run(std::int32_t cutoff,std::int32_t rate){
        require(cutoff==500&&(rate==48000||rate==44100),"unsupported coefficient input pair");
        const U root2=root("007C8924",0x40000000u);
        self_equal("007C8954",root2);
        const U first_root=trace("007C895C","move_root2",root2,0,0,root2);
        U angle=number("007C8970",cutoff);
        const U rate_float=number("007C8984",rate);
        angle=mul("007C8988",angle,0x40c90fdau);
        angle=div("007C8994",angle,rate_float);
        const U cosine=cosine_small(angle);
        const U reused_root=trace("007C89A4","reuse_original_sqrt",root2,0,0,root2);
        self_equal("007C89A8",reused_root);
        const U a=sub("007C89B0",first_root,cosine); // branch delay: always executes
        const U b=sub("007C89C8",reused_root,0x3f800000u);
        const U minus=sub("007C89CC",a,b);
        const U plus=add("007C89D0",a,b);
        const U radicand=mul("007C89D4",plus,minus);
        const U square_root=root("007C89E0",radicand);
        self_equal("007C89E4",square_root);
        U coefficient=sub("007C89EC",a,square_root); // taken BC1TL delay
        coefficient=div("007C8A18",coefficient,b);
        const U negative=trace("007C8A28","neg",coefficient,0,0,coefficient^0x80000000u);
        U first=add("007C8A2C",coefficient,coefficient);
        U second=mul("007C8A30",negative,coefficient);
        first=mul("007C8A34",first,0x45800000u);
        second=mul("007C8A38",second,0x45800000u);
        const U first_word=trace("007C8A3C","cvt_w_s",first,0,0,ps2::fp::cvt_w_s(first));
        const U second_word=trace("007C8A44","cvt_w_s",second,0,0,ps2::fp::cvt_w_s(second));
        const auto c1=std::bit_cast<std::int32_t>(first_word),c2=std::bit_cast<std::int32_t>(second_word);
        require(c1>=INT16_MIN&&c1<=INT16_MAX&&c2>=INT16_MIN&&c2<=INT16_MAX,"unsupported signed16 coefficient store wrap");
        require(!flags,"unsupported exceptional helper observation");
        std::printf("{\"case\":\"%s\",\"kind\":\"coefficient_pair\",\"cutoff\":%d,\"rate\":%d,\"first_word\":\"%08x\",\"second_word\":\"%08x\",\"first_int16\":%d,\"second_int16\":%d,\"helper_flags\":\"%08x\"}\n",name,cutoff,rate,first_word,second_word,c1,c2,flags);
    }
};
}
int main(int argc,char**){try{
    Reference::require(argc==1,"usage: offline_adx_coefficient_reference (no arguments)");
    Reference actual{"actual_header_500_48000"};actual.run(500,48000);
    Reference constructor{"constructor_500_44100"};constructor.run(500,44100);
    std::puts("PASS offline literal ADX coefficient source-helper sequence; no binary hardware or decoder parity");
    Reference::require(!std::fflush(stdout)&&!std::ferror(stdout),"complete reference output");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"UNSUPPORTED offline ADX coefficient reference: %s\n",e.what());return 1;}}
