#include "ps2float.h"
#include <cstdio>
extern "C" unsigned ps2mul(unsigned,unsigned);
int main(){unsigned seed=0x2468ace1,n=0;for(unsigned i=0;i<1000000;++i){seed=seed*1664525u+1013904223u;unsigned a=seed;seed=seed*1664525u+1013904223u;unsigned b=seed;auto x=ps2mul(a,b);auto y=unsigned(ps2f_mul(a,b));if(x!=y&&n++<4)std::printf("primary dispute %08x %08x %08x %08x\n",a,b,x,y);}std::printf("TellowKrinkle-a5b2 primary-vs-parallel-f777 million allbit-pattern products mismatches%u\n",n);return n?1:0;}
