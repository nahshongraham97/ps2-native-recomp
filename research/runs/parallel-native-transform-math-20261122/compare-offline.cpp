#include "imported-fpu.reference.hpp"
#include "ps2float.h"
#include <cstdio>
#include <cstdint>
int main(){unsigned flags=0;unsigned a=0x3f800000,b=0x3fffffff;std::printf("known %08x %08x %08x %08x\n",a,b,ps2::fp::ee::mul(flags,a,b),unsigned(ps2f_mul(a,b)));
 unsigned seed=0x13579bdf,n=0;for(unsigned i=0;i<1000000;++i){seed=seed*1664525u+1013904223u;a=0x3f000000u|(seed&0x7fffffu);seed=seed*1664525u+1013904223u;b=0x3f000000u|(seed&0x7fffffu);flags=0;auto x=ps2::fp::ee::mul(flags,a,b);auto y=unsigned(ps2f_mul(a,b));if(x!=y&&n++<8)std::printf("dispute %08x %08x imported=%08x research=%08x\n",a,b,x,y);}std::printf("mul disputes %u/1000000\n",n);}
