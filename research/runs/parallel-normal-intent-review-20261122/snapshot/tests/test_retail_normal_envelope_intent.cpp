#include "mka/retail_normal_envelope_intent.hpp"
#include <cstdio>
#include <stdexcept>
using namespace mka::sound;
namespace {void check(bool v,const char* what){if(!v)throw std::runtime_error(what);}}
int main() try {
    RetailNormalEnvelopeFailure f;
    const auto setup=decode_retail_normal_envelope_setup(0x000f,0x1fc6,f);
    check(setup&&f.kind==RetailNormalEnvelopeError::none,"actual original normal setup");
    check(setup->attack==RetailEnvelopeCurve::linear_increase&&setup->attack_setting==0,"AR0 linear");
    check(setup->decay==RetailEnvelopeCurve::exponential_decrease&&setup->decay_setting==0,"DR0 exponential");
    check(setup->sustain_level_sixteenths==16&&setup->decay_has_one_sample_at_full_sustain,"SL15 full still has one decay sample");
    check(setup->sustain==RetailEnvelopeCurve::linear_increase&&setup->sustain_setting==127,"SR127 intent retains its own unresolved timing");
    check(setup->release==RetailEnvelopeCurve::linear_decrease&&setup->release_setting==6,"RR6 linear release");
    for(unsigned w=0;w<65536;++w) {
        check(bool(decode_retail_normal_envelope_setup(static_cast<std::uint16_t>(w),0x1fc6,f))==(w==15),"no guessed alternate first setup");
        check(bool(decode_retail_normal_envelope_setup(15,static_cast<std::uint16_t>(w),f))==(w==0x1fc6),"no guessed alternate second setup");
    }
    check(!decode_retail_normal_envelope_setup(0xf000,0xf000,f)&&f.kind==RetailNormalEnvelopeError::unsupported_setup,"pan-history sentinels are not envelope defaults");
    for(const std::uint16_t flags:{0,1,8,9,0x8000,0xffff}) {
        const auto p=retail_normal_start_intent(flags);
        check(p.key_on_after_configuration==((flags&8)==0)&&p.freeze_source_before_release&&p.retain_release_tail,"held source mask and freeze/release intent");
    }
    std::puts("PASS source-decoded normal setup/start intent only; exact native timing/profile realization remains REQUIRED");return 0;
} catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
