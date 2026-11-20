#include "mka/material_translation.hpp"
#include <bit>
namespace mka::game {
namespace {
// Compiled layer coordinate program: ordered stored old+0, rate*delta plus old,
// followed by sign-preserving unit remainder. All operands are game words.
std::uint32_t translated_coordinate(std::uint32_t old,std::uint32_t rate,std::uint32_t delta) noexcept {
 struct Term {std::uint32_t sign;int exponent;std::uint32_t magnitude;};
 constexpr std::uint32_t sign_bit=0x80000000u;
 // The initial ordered +0 discards signed zero/denormal old inputs.
 const auto oe=(old>>23)&255u;
 Term a{oe?(old&sign_bit):0u,static_cast<int>(oe),oe?((old&0x7fffffu)|0x800000u):0u};
 const auto re=(rate>>23)&255u,de=(delta>>23)&255u;
 Term b{(rate^delta)&sign_bit,0,0};
 if(re&&de){
  const auto rm=(rate&0x7fffffu)|0x800000u,dm=(delta&0x7fffffu)|0x800000u;
  std::uint64_t product=std::uint64_t(rm)*dm;
  product-=(dm>>1)&1u; // Ordered SECOND operand is the actual step delta.
  b.exponent=static_cast<int>(re)+static_cast<int>(de)-127;
  if(product>>47){product>>=24;++b.exponent;}else product>>=23;
  b.magnitude=static_cast<std::uint32_t>(product);
 }
 Term out=a;
 if(!b.magnitude){if(!a.magnitude)out={a.sign&b.sign,0,0};}
 else if(!a.magnitude)out=b;
 else {
  if(b.exponent>a.exponent||(b.exponent==a.exponent&&b.magnitude>a.magnitude)){const auto t=a;a=b;b=t;}
  auto large=a.magnitude<<1;const int gap=a.exponent-b.exponent;
  const auto small=gap>25?0u:(b.magnitude<<1)>>gap;
  int exponent=a.exponent;
  if(a.sign!=b.sign){large-=small;if(!large)out={0,0,0};else{const int shift=std::countl_zero(large)-7;large<<=shift;exponent-=shift;out={a.sign,exponent,large>>1};}}
  else{large+=small;if(large&0x2000000u){large>>=1;++exponent;}out={a.sign,exponent,large>>1};}
 }
 std::uint32_t stored;
 if(!out.magnitude||out.exponent<=0)stored=out.sign;
 else if(out.exponent>255)stored=out.sign|0x7fffffffu;
 else stored=out.sign|(static_cast<std::uint32_t>(out.exponent)<<23)|(out.magnitude&0x7fffffu);
 const auto exponent=(stored>>23)&255u,sign=stored&sign_bit;
 if(exponent<127)return stored;
 if(exponent>=150)return sign;
 const auto residual=((stored&0x7fffffu)|0x800000u)&((1u<<(150-exponent))-1u);
 if(!residual)return sign;
 const auto highest=31u-static_cast<unsigned>(std::countl_zero(residual));
 return sign|((exponent+highest-23u)<<23)|((residual<<(23u-highest))&0x7fffffu);
}
}
MaterialLayerSelection select_material_layers(const MaterialTranslationRates& r) noexcept {
 return {((r.first_x.bits|r.first_y.bits)&0x7f800000u)!=0,((r.second_x.bits|r.second_y.bits)&0x7f800000u)!=0};
}
MaterialTransformPair translate_material_layers(MaterialTransformPair p,const MaterialTranslationRates& r,MaterialStepDelta d,MaterialLayerSelection enabled) noexcept {
 if(enabled.first){p.first.translation.x.bits=translated_coordinate(p.first.translation.x.bits,r.first_x.bits,d.value.bits);p.first.translation.y.bits=translated_coordinate(p.first.translation.y.bits,r.first_y.bits,d.value.bits);p.first.flags&=0xfffdffffu;}
 if(enabled.second){p.second.translation.x.bits=translated_coordinate(p.second.translation.x.bits,r.second_x.bits,d.value.bits);p.second.translation.y.bits=translated_coordinate(p.second.translation.y.bits,r.second_y.bits,d.value.bits);p.second.flags&=0xfffdffffu;}
 return p;
}
}
