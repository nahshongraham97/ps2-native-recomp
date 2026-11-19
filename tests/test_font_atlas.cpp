#include "mka/font_atlas.hpp"
#include "native_test_sha256.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <utility>
using B=std::vector<std::uint8_t>;
static void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
static void put(B& b,std::size_t p,std::uint32_t v){for(unsigned i=0;i<4;++i)b.at(p+i)=std::uint8_t(v>>(i*8));}
static std::string sha(const B& b){NativeTestSha256 h;h.add(b.data(),b.size());return h.finish();}
static void chunk(B& b,std::size_t p,unsigned t,unsigned n){put(b,p,t);put(b,p+4,n);put(b,p+8,0x1803ffff);}
static B fixture(){
 B b(0x8700);put(b,5,256);put(b,9,128);
 chunk(b,13,1,8);put(b,25,0x00325350);put(b,29,0x1101);
 chunk(b,0x21,2,4);chunk(b,0x31,2,4);chunk(b,0x41,1,0x8553);chunk(b,0x4d,1,64);
 const std::array<unsigned,16> f={256,128,8,0x22504,0xe1310000,0x20000005,0x80,0,0x4000,0x400004,0x4000,0x400004,0x8050,0x450,0x2800,0xfc0};
 for(unsigned i=0;i<16;++i)put(b,0x59+4*i,f[i]);
 chunk(b,0x99,1,0x84a0);put(b,0xa5,0x8600);put(b,0xa9,0x100);put(b,0xad,0x8180);
 for(auto p:{0x200u,0x8280u}){
  const bool pal=p==0x8280;
  const std::array<unsigned,20> h={3,0x10000000,14,0,0,0,0x51,0,pal?16u:128u,pal?16u:64u,0x52,0,0,0,0x53,0,pal?0x40u:0x800u,0x08000000,0,0};
  for(unsigned i=0;i<20;++i)put(b,p+4*i,h[i]);
 }
 return b;
}
// Independent literal byte correspondence from primary manual p174, NOT the
// production inverse address tables. Rows A..P, bytes low to high.
static constexpr unsigned columns[2][16][4]={
 {{0,36,8,44},{1,37,9,45},{2,38,10,46},{3,39,11,47},
  {4,32,12,40},{5,33,13,41},{6,34,14,42},{7,35,15,43},
  {16,52,24,60},{17,53,25,61},{18,54,26,62},{19,55,27,63},
  {20,48,28,56},{21,49,29,57},{22,50,30,58},{23,51,31,59}},
 {{4,32,12,40},{5,33,13,41},{6,34,14,42},{7,35,15,43},
  {0,36,8,44},{1,37,9,45},{2,38,10,46},{3,39,11,47},
  {20,48,28,56},{21,49,29,57},{22,50,30,58},{23,51,31,59},
  {16,52,24,60},{17,53,25,61},{18,54,26,62},{19,55,27,63}}};
static void synthetic(){
 auto bytes=fixture();std::array<bool,32768> visited{};B expected(32768);
 // Enumerate source CT32 scan-order words, map bytes to T8 scan-order pixels
 // by page, block coordinates and the explicit p174 column diagram.
 for(unsigned py=0;py<2;++py)for(unsigned px=0;px<2;++px)
 for(unsigned by=0;by<4;++by)for(unsigned bx=0;bx<8;++bx)
 for(unsigned col=0;col<4;++col)for(unsigned word=0;word<16;++word)for(unsigned c=0;c<4;++c){
  const auto sx=px*64+bx*8+word%8,sy=py*32+by*8+col*2+word/8;
  const auto source=(sy*128+sx)*4+c;
  const auto n=columns[col&1][word][c];
  const auto x=px*128+bx*16+n%16,y=py*64+by*16+col*4+n/16;
  const auto out=y*256+x;check(!visited[out],"reference map duplicate");visited[out]=true;
  const auto value=std::uint8_t((source*73+source/257+11)&255);
  bytes[0x250+source]=value;expected[out]=value;
 }
 for(bool v:visited)check(v,"reference map missing byte");
 // Store a deliberately colored palette by diagram coordinates: row carries
 // index bit3, x bit3 carries index bit4. All four channels distinguishable.
 for(unsigned y=0;y<16;++y)for(unsigned x=0;x<16;++x){
  unsigned index=(y/2)*32+(y%2)*8+(x/8)*16+x%8;
  auto p=0x82d0+4*(y*16+x);
  bytes[p]=std::uint8_t(index);bytes[p+1]=std::uint8_t(index^0x55);
  bytes[p+2]=std::uint8_t(255-index);bytes[p+3]=std::uint8_t(index/2);
 }
 mka::FontAtlasFailure f;auto a=mka::FontAtlas::decode(bytes,f);
 check(bool(a),"synthetic decode");check(a->indices()==expected,"all-byte independent permutation");
 check(a->serialized()==bytes,"raw source unchanged");
 for(unsigned i=0;i<32768;++i){auto v=expected[i];const auto& q=a->rgba_raw_alpha();
  check(q[4*i]==v && q[4*i+1]==(v^0x55) && q[4*i+2]==255-v && q[4*i+3]==v/2,"colored palette/channel/rawalpha");}
 auto copy=*a;auto moved=std::move(*a);a.reset();bytes.clear();
 check(copy.indices()==expected && moved.indices()==expected,"owned copy/move lifetime");
 const auto reject=[&](B b,mka::FontAtlasError e){auto v=mka::FontAtlas::decode(std::move(b),f);check(!v && f.code==e,"named malformed rejection");};
 for(auto n:{0u,4u,12u,0x58u,0x86ffu}){auto b=fixture();b.resize(n);reject(std::move(b),mka::FontAtlasError::truncated);}
 {auto b=fixture();put(b,5,512);reject(std::move(b),mka::FontAtlasError::unsupported_variant);}
 {auto b=fixture();put(b,0xa5,0xffffffff);reject(std::move(b),mka::FontAtlasError::invalid_extent);}
 {auto b=fixture();put(b,0xa9,0xffffffff);reject(std::move(b),mka::FontAtlasError::invalid_extent);}
 {auto b=fixture();put(b,0x45,0xffffffff);reject(std::move(b),mka::FontAtlasError::malformed_chunk);}
 {auto b=fixture();put(b,0x15,0);reject(std::move(b),mka::FontAtlasError::unsupported_variant);}
 {auto b=fixture();put(b,0x220,127);reject(std::move(b),mka::FontAtlasError::invalid_transfer);}
 {auto b=fixture();put(b,0x5d,64);reject(std::move(b),mka::FontAtlasError::unsupported_variant);}
 {auto b=fixture();b.push_back(0);reject(std::move(b),mka::FontAtlasError::invalid_extent);}
}
int main(int argc,char** argv){try{
 synthetic();
 if(argc==2){std::ifstream file(argv[1],std::ios::binary);check(bool(file),"open explicit atlas-entry fixture");
  B b((std::istreambuf_iterator<char>(file)),{});
  check(sha(b)=="085846ff5d595e179e118c2fcbe51f59f322f6c51fc871cac80354f19d1990bc","actual serialized atlas SHA");
  mka::FontAtlasFailure f;auto a=mka::FontAtlas::decode(std::move(b),f);
  if(!a)throw std::runtime_error(std::string(mka::font_atlas_error_name(f.code))+": "+f.detail);
  check(sha(a->indices())=="743f7f80cc79b2f48cf1a0da2aefa6f6a453cb7d5812c87d5cd059b0407fd240","source-derived index SHA");
  check(sha(a->rgba_raw_alpha())=="de76edc806f1d1924ba9fbabbd57d5490afd37c3044ec26823e628aec047c049","source-derived rawalpha RGBA SHA");
 }else check(argc==1,"usage: test_font_atlas [explicit-extracted-atlas-entry]");
 std::cout<<"PASS font atlas owned data/permutation; no render oracle\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
