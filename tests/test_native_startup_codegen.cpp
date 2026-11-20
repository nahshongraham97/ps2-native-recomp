#include "native_startup_codegen.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <new>
#include <stdexcept>
namespace {
bool fail_allocation=false;
void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
}
void* operator new(std::size_t n){if(fail_allocation)throw std::bad_alloc();if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
int main(int argc,char** argv){try {
    check(argc==2,"original ELF argument");
    std::ifstream input(std::filesystem::u8path(argv[1]),std::ios::binary);
    check(bool(input),"original ELF opens");
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
    check(!input.bad(),"original ELF complete");
    auto manifest=recomp::native_startup_manifest();
    check(manifest.words.size()==209&&manifest.steps.size()==6,"bounded original CRT/main recipe");
    recomp::NativeClosureFailure f;
    auto result=recomp::emit_native_startup_closure(bytes,manifest,f);
    check(result&&f.kind==recomp::NativeClosureError::none,"actual original recipe emits");
    const auto expected=*result;
    auto rejected=[&](const auto& m,recomp::NativeClosureError error){
        check(!recomp::emit_native_startup_closure(bytes,m,f)&&f.kind==error,"named altered recipe refusal");
    };
    auto altered=manifest;altered.words.pop_back();rejected(altered,recomp::NativeClosureError::source_word);
    altered=manifest;altered.words[0].word^=1;rejected(altered,recomp::NativeClosureError::source_word);
    altered=manifest;std::swap(altered.words[0],altered.words[1]);rejected(altered,recomp::NativeClosureError::source_word);
    altered=manifest;altered.steps.erase(altered.steps.begin());rejected(altered,recomp::NativeClosureError::recipe);
    altered=manifest;std::swap(altered.steps[2],altered.steps[3]);rejected(altered,recomp::NativeClosureError::recipe);
    altered=manifest;altered.original_elf_sha256[0]='0';rejected(altered,recomp::NativeClosureError::source_identity);
    bytes.back()^=1;rejected(manifest,recomp::NativeClosureError::source_identity);bytes.back()^=1;
    fail_allocation=true;auto failed=recomp::emit_native_startup_closure(bytes,manifest,f);fail_allocation=false;
    check(!failed&&f.kind==recomp::NativeClosureError::allocation,"real emitter allocation failure");
    result=recomp::emit_native_startup_closure(bytes,manifest,f);
    check(result&&result->header==expected.header&&result->implementation==expected.implementation&&
        result->entry==expected.entry,"failure preserves repeatable independent output");
    for(auto* source:{&result->header,&result->implementation,&result->entry})
        for(auto* forbidden:{"ps2::","Cpu&","g_ram","g_spr","reinterpret_cast","dispatch(","0x001","0x002","syscall("})
            check(source->find(forbidden)==source->npos,"no runtime guest ABI/address/dispatch in generated source");
    std::puts("PASS native retail startup source identity, ordered prerequisites and owned emission; no game boot");
    return std::fflush(stdout)||std::ferror(stdout)?1:0;
}catch(const std::exception& e){fail_allocation=false;std::fprintf(stderr,"FAIL startup emitter: %s\n",e.what());return 1;}}
