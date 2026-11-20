// OFFLINE SOURCE HELPER ONLY. Never link into accepted native libraries.
#include "ps2/fpu.hpp"
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
 check(argc==2,"usage offline_geometry_source vectors.tsv");const auto rows=inputs(argv[1]);
 for(std::size_t i=0;i<rows.size();++i){std::uint32_t flags=0;const auto stored=ps2::fp::ee::mul(flags,0x42fe0000u,rows[i]);
  const auto out=ps2::fp::cvt_w_s(stored)&255u;
  check(std::printf("%zu %08x %u\n",i,rows[i],unsigned(out))>0,"output.row");}
 check(std::puts("PASS offline geometry companion project-source oracle; not runtime or hardware proof")>=0&&std::fflush(stdout)==0,"output.flush");return 0;
 }catch(const std::exception& e){std::fprintf(stderr,"FAIL offline geometry source %s\n",e.what());return 1;}}
