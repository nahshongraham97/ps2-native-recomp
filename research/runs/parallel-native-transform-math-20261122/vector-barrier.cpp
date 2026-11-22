#include "imported-fpu.reference.hpp"
#include "ps2float.h"
#include <cstdio>
int main(){unsigned f=0;unsigned a=0x3f800000,b=0xb3800001;
std::printf("ordered-add candidate %08x %08x imported%08x research%08x\n",a,b,ps2::fp::ee::add(f,a,b),unsigned(ps2f_add(a,b)));
unsigned x=0x3f3d7186,right=0x3f5f1b2d;f=0;
a=ps2::fp::ee::mul(f,x,right);a=ps2::fp::ee::madd(f,a,0,0);a=ps2::fp::ee::add(f,0,a);a=ps2::fp::ee::madd(f,a,0,0);
auto q=ps2f_mul(x,right);q=ps2f_madd(unsigned(q),0,0,(q&PS2F_OF)!=0);q=ps2f_add(0,unsigned(q));q=ps2f_madd(unsigned(q),0,0,(q&PS2F_OF)!=0);
std::printf("vector input=(3f3d7186,0,0) right=(3f5f1b2d,0,0) up=at=0 output.x imported%08x research%08x\n",a,unsigned(q));}
