// OFFLINE ONLY. This translation unit must never enter accepted native libraries.
#include "ps2/fpu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {

using Row=std::array<std::uint32_t,10>;
std::vector<Row> read_rows(const char* path){std::ifstream in(path);if(!in)throw std::runtime_error("vectors.open");std::vector<Row> rows;std::string line;while(std::getline(in,line)){std::istringstream s(line);std::size_t id;if(!(s>>id)||id!=rows.size())throw std::runtime_error("vectors.id");Row r;for(auto& w:r){std::string v;if(!(s>>v)||v.size()!=8||v.find_first_not_of("0123456789abcdef")!=std::string::npos)throw std::runtime_error("vectors.word");w=static_cast<std::uint32_t>(std::stoul(v,nullptr,16));}std::string extra;if(s>>extra||r[9]>3)throw std::runtime_error("vectors.shape");rows.push_back(r);}if(in.bad()||rows.empty())throw std::runtime_error("vectors.read");return rows;}
std::array<std::uint32_t,32> initial(const Row& r){std::array<std::uint32_t,32> w;for(unsigned i=0;i<32;++i)w[i]=0x12340000u+i;w[9]=r[0];w[10]=r[1];w[25]=r[2];w[26]=r[3];w[12]=0xffffffffu;w[28]=0x80020001u;return w;}
void emit(std::size_t id,const std::array<std::uint32_t,32>& w,unsigned selection){if(std::printf("%zu",id)<0)throw std::runtime_error("output.write");for(auto v:w)if(std::printf(" %08x",v)<0)throw std::runtime_error("output.write");if(std::printf(" %08x\n",selection)<0)throw std::runtime_error("output.write");}

// Specialized actual11E508 dividend-by-one reduction, independent of candidate.
std::uint32_t remainder_one(std::uint32_t word){const auto sign=word&0x80000000u;int e=int((word>>23)&255u)-127;if(e<0)return word;std::uint32_t n=(word&0x7fffffu)|0x800000u;while(e>0){if(n>=0x800000u)n-=0x800000u;if(!n)return sign;n<<=1;--e;}if(n>=0x800000u)n-=0x800000u;if(!n)return sign;while(n<0x800000u){n<<=1;--e;}return sign|(std::uint32_t(e+127)<<23)|(n&0x7fffffu);}
std::uint32_t coordinate(std::uint32_t old,std::uint32_t rate,std::uint32_t delta){std::uint32_t flags=0;const auto stored_old=ps2::fp::ee::add(flags,0,old);const auto stored=ps2::fp::ee::madd(flags,stored_old,rate,delta);return remainder_one(stored);}
}
int main(int argc,char** argv){try{if(argc!=2)throw std::runtime_error("usage source_oracle vectors.tsv");auto rows=read_rows(argv[1]);for(std::size_t i=0;i<rows.size();++i){const auto& r=rows[i];auto w=initial(r);for(unsigned layer=0;layer<2;++layer)if(r[9]&(1u<<layer)){const auto b=layer*16;w[b+9]=coordinate(r[layer*2],r[4+layer*2],r[8]);w[b+10]=coordinate(r[layer*2+1],r[5+layer*2],r[8]);w[b+12]&=0xfffdffffu;}const unsigned selected=(ps2::fp::key(r[4])!=0||ps2::fp::key(r[5])!=0)|unsigned(ps2::fp::key(r[6])!=0||ps2::fp::key(r[7])!=0)<<1;emit(i,w,selected);}if(std::puts("PASS offline material translation project-source helper oracle; not runtime or hardware proof")<0||std::fflush(stdout)!=0)throw std::runtime_error("output.flush");return 0;}catch(const std::exception& e){std::fprintf(stderr,"FAIL material source oracle %s\n",e.what());return 1;}}
