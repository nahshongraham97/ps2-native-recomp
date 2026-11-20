#include "mka/material_translation.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace mka::game;
namespace {
void check(bool b){if(!b)throw std::runtime_error("material translation literal");}

using Row=std::array<std::uint32_t,10>;
std::vector<Row> read_rows(const char* path){std::ifstream in(path);if(!in)throw std::runtime_error("vectors.open");std::vector<Row> rows;std::string line;while(std::getline(in,line)){std::istringstream s(line);std::size_t id;if(!(s>>id)||id!=rows.size())throw std::runtime_error("vectors.id");Row r;for(auto& w:r){std::string v;if(!(s>>v)||v.size()!=8||v.find_first_not_of("0123456789abcdef")!=std::string::npos)throw std::runtime_error("vectors.word");w=static_cast<std::uint32_t>(std::stoul(v,nullptr,16));}std::string extra;if(s>>extra||r[9]>3)throw std::runtime_error("vectors.shape");rows.push_back(r);}if(in.bad()||rows.empty())throw std::runtime_error("vectors.read");return rows;}
std::array<std::uint32_t,32> initial(const Row& r){std::array<std::uint32_t,32> w;for(unsigned i=0;i<32;++i)w[i]=0x12340000u+i;w[9]=r[0];w[10]=r[1];w[25]=r[2];w[26]=r[3];w[12]=0xffffffffu;w[28]=0x80020001u;return w;}
void emit(std::size_t id,const std::array<std::uint32_t,32>& w,unsigned selection){if(std::printf("%zu",id)<0)throw std::runtime_error("output.write");for(auto v:w)if(std::printf(" %08x",v)<0)throw std::runtime_error("output.write");if(std::printf(" %08x\n",selection)<0)throw std::runtime_error("output.write");}

NativeMaterialMatrix matrix(const std::array<std::uint32_t,32>& w,unsigned b){return {{ {w[b]}, {w[b+1]}, {w[b+2]} },{{w[b+3]},{w[b+4]},{w[b+5]}},{{w[b+6]},{w[b+7]},{w[b+8]}},{{w[b+9]},{w[b+10]},{w[b+11]}},w[b+12],{w[b+13],w[b+14],w[b+15]}};}
void flatten(std::array<std::uint32_t,32>& w,unsigned b,const NativeMaterialMatrix& m){unsigned at=b;for(auto v:{m.right,m.up,m.at,m.translation})for(auto x:{v.x,v.y,v.z})w[at++]=x.bits;w[at++]=m.flags;for(auto x:m.opaque_padding)w[at++]=x;}
std::uint32_t one(std::uint32_t old,std::uint32_t rate,std::uint32_t delta){Row row{};row[0]=old;auto w=initial(row);MaterialTransformPair p{matrix(w,0),matrix(w,16)};MaterialTranslationRates r{{rate},{0},{0},{0}};return translate_material_layers(p,r,{{delta}},{true,false}).first.translation.x.bits;}
}
int main(int argc,char** argv){try{
 check(one(0,0x3f800000,0x3f800002)==0x34000000);check(one(0,0x3f800002,0x3f800000)==0x34800000);
 check(one(0x3fa00000,0,0)==0x3e800000);check(one(0xbfa00000,0,0)==0xbe800000);
 check(one(0,0xbf800000,0x3f800000)==0x80000000);check(one(0x80000000,0,0)==0);check(one(1,0,0)==0);
 check(one(0,0x7fffffff,0x7fffffff)==0);check(one(0,0xffffffff,0x7fffffff)==0x80000000);
 MaterialTranslationRates denorm{{1},{0x807fffff},{0},{0x80000000}};check(select_material_layers(denorm)==MaterialLayerSelection{false,false});
 Row base{};auto words=initial(base);MaterialTransformPair p{matrix(words,0),matrix(words,16)};MaterialTranslationRates rates{{0x3f800000},{0xbf800000},{0},{0}};
 check(translate_material_layers(p,rates,{{0x3f800000}},{false,false})==p);auto changed=translate_material_layers(p,rates,{{0x3f800000}},{true,false});auto expected=p;expected.first.translation.x.bits=0;expected.first.translation.y.bits=0x80000000;expected.first.flags&=0xfffdffffu;check(changed==expected);
 if(argc==2){auto rows=read_rows(argv[1]);for(std::size_t i=0;i<rows.size();++i){const auto& r=rows[i];auto w=initial(r);MaterialTranslationRates rates{{r[4]},{r[5]},{r[6]},{r[7]}};auto selected=select_material_layers(rates);auto out=translate_material_layers({matrix(w,0),matrix(w,16)},rates,{{r[8]}},{bool(r[9]&1),bool(r[9]&2)});flatten(w,0,out.first);flatten(w,16,out.second);emit(i,w,unsigned(selected.first)|unsigned(selected.second)<<1);}}
 else check(argc==1);
 check(std::puts("PASS native material translation source-word kernel; no hardware parity or publication")>=0&&std::fflush(stdout)==0);return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL material translation %s\n",e.what());return 1;}}
