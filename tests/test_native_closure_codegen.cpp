#include "native_closure_codegen.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <new>
#include <stdexcept>
#include <string_view>
#include <utility>
namespace { bool fail_allocate=false;void check(bool value,const char* why){if(!value)throw std::runtime_error(why);} }
void* operator new(std::size_t n){if(fail_allocate)throw std::bad_alloc();if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
int main(int argc,char** argv){try{
 check(argc==2||argc==3,"usage: test_native_closure_codegen original_ELF [NEW_native_output_dir]");
 std::ifstream input(std::filesystem::u8path(argv[1]),std::ios::binary);check(bool(input),"original input opens");
 std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});check(!input.bad()&&!bytes.empty(),"original input complete");
 auto manifest=recomp::native_logon_manifest();recomp::NativeClosureFailure failure;
 auto result=recomp::emit_native_logon_closure(bytes,manifest,failure);check(result&&failure.kind==recomp::NativeClosureError::none,"exact original source accepted");
 check(result->header.find("class NativeLogonState final")!=std::string::npos&&result->implementation.find("return native_logon_select(f);")!=std::string::npos&&result->implementation.find("return native_logon_start(*request,f);")!=std::string::npos,"actual native caller chain emitted");
 check(result->implementation.find("service_.start(request.id,request.requested_view,settings_.language,4,f.service)")!=std::string::npos,"actual Service source fade count");
 check(result->implementation.find("poll_logon_skip(*lease->host,af)")!=std::string::npos&&result->implementation.find("validate_logon_bindings(f.input)")!=std::string::npos,"actual typed native action owner");
 for(const char* forbidden:{"ps2::","Cpu","c.r[","dispatch(","reinterpret_cast","0x0018","0x0033","original(c","register_hook"}){
  check(result->header.find(forbidden)==std::string::npos&&result->implementation.find(forbidden)==std::string::npos,"no runtime legacy ABI or address adapter");}
 auto expected=*result;
 auto rejected=[&](const recomp::NativeLogonManifest& m,recomp::NativeClosureError error){auto r=recomp::emit_native_logon_closure(bytes,m,failure);check(!r&&failure.kind==error,"named manifest rejection");};
 auto altered=manifest;altered.original_elf_sha256[0]='0';rejected(altered,recomp::NativeClosureError::source_identity);
 altered=manifest;altered.words[0].word^=1;rejected(altered,recomp::NativeClosureError::source_word);
 altered=manifest;std::swap(altered.words[0],altered.words[1]);rejected(altered,recomp::NativeClosureError::source_word);
 altered=manifest;altered.words.pop_back();rejected(altered,recomp::NativeClosureError::source_word);
 altered=manifest;altered.effects.pop_back();rejected(altered,recomp::NativeClosureError::recipe);
 altered=manifest;std::swap(altered.effects[0],altered.effects[1]);rejected(altered,recomp::NativeClosureError::recipe);
 altered=manifest;altered.effects.push_back(altered.effects[0]);rejected(altered,recomp::NativeClosureError::recipe);
 altered=manifest;altered.effects[0]=static_cast<recomp::NativeLogonEffect>(100);rejected(altered,recomp::NativeClosureError::recipe);
 bytes[0]^=1;check(!recomp::emit_native_logon_closure(bytes,manifest,failure)&&failure.kind==recomp::NativeClosureError::source_identity,"actual source byte mismatch");bytes[0]^=1;
 fail_allocate=true;auto failed=recomp::emit_native_logon_closure(bytes,manifest,failure);fail_allocate=false;
 check(!failed&&failure.kind==recomp::NativeClosureError::allocation,"real emitter allocation failure");
 auto again=recomp::emit_native_logon_closure(bytes,manifest,failure);check(again&&again->header==expected.header&&again->implementation==expected.implementation,"failed generation leaves owned output independent and successful emission repeatable");
 result.reset();check(!expected.header.empty()&&!expected.implementation.empty(),"owned output lifetime");
 if(argc==3){const auto dir=std::filesystem::u8path(argv[2]);check(std::filesystem::create_directory(dir),"exclusive new native output directory");
  for(const auto& pair:{std::pair{"native_logon.hpp",&expected.header},std::pair{"native_logon.cpp",&expected.implementation}}){
   std::ofstream output(dir/pair.first,std::ios::binary);check(bool(output),"native output opens");output.write(pair.second->data(),static_cast<std::streamsize>(pair.second->size()));output.close();check(bool(output),"native output complete");}}
 check(std::puts("PASS strict typed LOGON source recipe and owned native emission; runtime chain not executed")>=0&&std::fflush(stdout)==0,"checked output");return 0;
}catch(const std::exception& e){fail_allocate=false;std::fprintf(stderr,"FAIL native LOGON emission %s\n",e.what());return 1;}}
