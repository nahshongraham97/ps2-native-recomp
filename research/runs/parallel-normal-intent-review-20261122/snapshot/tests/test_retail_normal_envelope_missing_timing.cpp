#include "mka/retail_normal_envelope_required.hpp"
int main(){mka::sound::RetailNormalEnvelopeFailure f;auto intent=mka::sound::decode_retail_normal_envelope_setup(15,0x1fc6,f);return mka::sound::normal_envelope_required::realize_retail_normal_envelope_profile(*intent,48000,f)?0:1;}
