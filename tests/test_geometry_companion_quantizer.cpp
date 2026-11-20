#include "mka/geometry_companion_quantizer.hpp"
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
void check(bool x,const char* what){if(!x)throw std::runtime_error(what);}
std::vector<std::uint32_t> inputs(const char* path){
 std::ifstream file(path);check(bool(file),"vectors.open");std::vector<std::uint32_t> result;std::string line;
 while(std::getline(file,line)){std::istringstream s(line);std::size_t id;std::string word,extra;
  check(bool(s>>id>>word)&&id==result.size()&&word.size()==8&&word.find_first_not_of("0123456789abcdef")==std::string::npos&&!(s>>extra),"vectors.row");
  result.push_back(static_cast<std::uint32_t>(std::stoul(word,nullptr,16)));}
 check(!file.bad()&&result.size()==4096,"vectors.cardinality");return result;
}
}
int main(int argc,char** argv){try{
 using mka::game::quantize_geometry_companion_component;
 if(argc==1){
  struct Literal{std::uint32_t word;unsigned byte;};
  const Literal literals[]={{0,0},{0x80000000,0},{1,0},{0x807fffff,0},{0x3f000000,63},{0xbf000000,193},
   {0x3f800000,127},{0xbf800000,129},{0x40000000,254},{0xc0000000,2},{0x40800000,252},
   {0xc0800000,4},{0x7f800000,255},{0xff800000,0},{0x7fffffff,255},{0xffffffff,0},
   {0x3f7fffff,126},{0xbf7fffff,130},{0x00800000,0}};
  for(const auto& x:literals)check(quantize_geometry_companion_component({x.word})==x.byte,"literal127/truncation/wrap/extended-domain");
 }else{check(argc==2,"usage quantizer [vectors.tsv]");const auto rows=inputs(argv[1]);
  for(std::size_t i=0;i<rows.size();++i)check(std::printf("%zu %08x %u\n",i,rows[i],unsigned(quantize_geometry_companion_component({rows[i]})))>0,"output.row");}
 check(std::puts("PASS native geometry companion127 quantization; project-source words not hardware proof")>=0&&std::fflush(stdout)==0,"output.flush");return 0;
 }catch(const std::exception& e){std::fprintf(stderr,"FAIL geometry companion %s\n",e.what());return 1;}}
