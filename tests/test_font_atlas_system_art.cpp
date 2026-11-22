#include "mka/font_atlas.hpp"
#include <fstream>
#include <iterator>
#include <vector>
#include <cstdio>
#include <stdexcept>
using B=std::vector<unsigned char>;
void check(bool x,const char* m){if(!x)throw std::runtime_error(m);}
unsigned u(const B&b,std::size_t p){return b.at(p)|(unsigned(b.at(p+1))<<8)|(unsigned(b.at(p+2))<<16)|(unsigned(b.at(p+3))<<24);}
static constexpr unsigned columns[2][16][4]={
 {{0,36,8,44},{1,37,9,45},{2,38,10,46},{3,39,11,47},
  {4,32,12,40},{5,33,13,41},{6,34,14,42},{7,35,15,43},
  {16,52,24,60},{17,53,25,61},{18,54,26,62},{19,55,27,63},
  {20,48,28,56},{21,49,29,57},{22,50,30,58},{23,51,31,59}},
 {{4,32,12,40},{5,33,13,41},{6,34,14,42},{7,35,15,43},
  {0,36,8,44},{1,37,9,45},{2,38,10,46},{3,39,11,47},
  {20,48,28,56},{21,49,29,57},{22,50,30,58},{23,51,31,59},
  {16,52,24,60},{17,53,25,61},{18,54,26,62},{19,55,27,63}}};

int main(int argc,char**argv)try{check(argc==2,"actual loose gameart file fixture");std::ifstream f(argv[1],std::ios::binary);check(bool(f),"original loose fixture");B b((std::istreambuf_iterator<char>(f)),{});unsigned count=0;
for(auto section:{0x800u,0x61800u,0xc2800u,0x123800u,0x184800u})for(unsigned i=0;i<u(b,section+16);++i){auto rec=section+28+i*16;if(auto type=u(b,rec)&0x3fffffffu;type!=2&&type!=3)continue;auto p=section+u(b,rec+4),n=u(b,rec+8);B raw(b.begin()+p,b.begin()+p+n);const auto rw=5+raw.at(4)+8,s=rw+76,d=s+64;auto w=u(raw,5+raw[4]),h=u(raw,9+raw[4]);auto ph=0x100+u(raw,d+16),pal=0x100+u(raw,d+20),tw=u(raw,ph+32),th=u(raw,ph+36);
mka::FontAtlasFailure failure;auto decoded=mka::FontAtlas::decode(raw,failure);if(!decoded){std::fprintf(stderr,"ordinal%u %ux%u: %s %s\n",i,w,h,mka::font_atlas_error_name(failure.code),failure.detail.c_str());return 1;}check(decoded->width()==w&&decoded->height()==h,"owned logical dimensions");
B expected(w*h);std::vector<bool> seen(w*h);
for(unsigned cy=0;cy<th;++cy)for(unsigned cx=0;cx<tw;++cx)for(unsigned c=0;c<4;++c){auto bx=(cx%64)/8,by=(cy%32)/8,col=(cy%8)/2,word=(cy%2)*8+cx%8;auto pos=columns[col&1][word][c];auto x=(cx/64)*128+bx*16+pos%16,y=(cy/32)*64+by*16+col*4+pos/16;if(x>=w||y>=h)continue;auto dst=y*w+x;check(!seen[dst],"independent literal diagram unique");seen[dst]=true;expected[dst]=raw.at(ph+80+(cy*tw+cx)*4+c);}
for(bool x:seen)check(x,"all logical pixels independently covered");check(decoded->indices()==expected,"all original indices match independent forward diagram");
for(unsigned j=0;j<expected.size();++j){auto q=(expected[j]&~0x18u)|((expected[j]&8)<<1)|((expected[j]&16)>>1);for(unsigned c=0;c<4;++c)check(decoded->rgba_raw_alpha().at(j*4+c)==raw.at(pal+80+q*4+c),"stored palette channels/rawalpha");}
check(decoded->serialized()==raw,"original raw bytes retained");++count;}
check(count==105,"all21 textures in all5 locale sections");std::printf("PASS all105 actual system-art textures original indices/palette/logical extents; no GPU\n");return 0;}catch(const std::exception&e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
