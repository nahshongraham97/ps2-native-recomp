#include "native_retail_startup.hpp"
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <new>
#include <stdexcept>
using namespace mka::retail;
namespace {
bool fail_allocation=false;
void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
}
void* operator new(std::size_t n){if(fail_allocation)throw std::bad_alloc();if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
int main(int argc,char** argv){try {
    check(argc==2,"exclusive fixture root argument");
    const auto root=std::filesystem::u8path(argv[1]);
    check(std::filesystem::create_directory(root),"exclusive native fixture root");
    check(std::filesystem::create_directory(root/"MODULES"),"actual-style resource directory");
    {std::ofstream out(root/"MODULES"/"DNAS310.IMG",std::ios::binary);out<<"owned-native-startup-fixture";out.close();check(bool(out),"fixture resource complete");}
    mka::content::ContentVolumeFailure cf;
    auto volume=mka::content::NativeContentVolume::open(root,cf);
    check(bool(volume),"actual native content volume");
    NativeRetailStartupState state(std::move(*volume),{"cdrom0:\\SLUS_214.10"});
    StartupFailure f;
    check(!state.memory_owners,"native memory group is not fabricated before its main phase");
    fail_allocation=true;const bool initialized=initialize_retail_memory_owners(state,f);fail_allocation=false;
    check(!initialized&&f.kind==StartupError::memory_owners&&!state.memory_owners,
        "real failed owner creation leaves startup state unpublished");
    check(initialize_retail_memory_owners(state,f)&&state.memory_owners,"actual owner group retained by generated startup state");
    const auto* owners=state.memory_owners.get();
    check(!initialize_retail_memory_owners(state,f)&&f.kind==StartupError::already_initialized&&state.memory_owners.get()==owners,
        "duplicate initialization cannot replace live native owner identities");
    RetailMemoryOwnerFailure mf;
    check(!state.memory_owners->wave_allocator(mf)&&mf.kind==RetailMemoryOwnerError::wave_unbound,
        "created identities do not fabricate missing retail Wave/overlay readiness");
    check(state.resource_prefix.empty()&&state.boot_directory.empty(),"native startup values independently initialized");
    check(initialize_retail_resource_prefix(state,f)&&state.resource_prefix=="cdrom0:","actual original content prefix in owned string");
    check(derive_retail_boot_directory(state,f)&&state.boot_directory=="cdrom0:\\","actual argv0 lexical leaf preserves device and trailing separator");
    auto file=open_retail_resource(state,"cdrom0:\\MODULES\\DNAS310.IMG;1",f);
    check(file&&file->logical_key()=="MODULES/DNAS310.IMG","actual source-style key opens native bytes without device provider");
    std::array<char,64> bytes{};auto read=file->read(bytes.data(),bytes.size(),cf);
    check(!read.error&&std::string_view(bytes.data(),read.bytes)=="owned-native-startup-fixture","retail startup content owner uses retained native handle");
    for(const auto& pair:std::vector<std::pair<std::string,std::string>>{
        {"relative/path/game","relative/path/"},{"host0:/folder/game","host0:/folder/"},
        {"/game","/"},{"folder\\sub/game","folder\\sub/"},{"folder/","folder/"}}){
        state.boot_arguments={pair.first};
        check(derive_retail_boot_directory(state,f)&&state.boot_directory==pair.second,"source lexical path without canonical/CWD substitution");
    }
    const std::string large(8192,'a');
    state.boot_arguments.assign(128,"argument");state.boot_arguments[0]=large+"/game";
    check(derive_retail_boot_directory(state,f)&&state.boot_directory==large+"/","no guest argument count or path capacity");
    const auto retained=state.boot_directory;
    state.boot_arguments.clear();
    check(!derive_retail_boot_directory(state,f)&&f.kind==StartupError::missing_boot_argument&&state.boot_directory==retained,"missing argv fails without damaging prior state");
    state.boot_arguments={"game"};
    check(!derive_retail_boot_directory(state,f)&&f.kind==StartupError::missing_directory_separator,"undefined source no-separator case is explicit error");
    state.boot_arguments={std::string("bad\0/path",9)};
    check(!derive_retail_boot_directory(state,f)&&f.kind==StartupError::malformed_boot_argument,"embedded terminator rejects owned malformed argument");
    check(!open_retail_resource(state,"cdrom1:\\MODULES\\DNAS310.IMG;1",f)&&f.kind==StartupError::content,"native file error propagated, no provider fallback");
    std::puts("PASS generated retail startup owns actual arguments/content/memory group; failed initialization is unpublished and full entry link remains blocked");
    return std::fflush(stdout)||std::ferror(stdout)?1:0;
}catch(const std::exception& e){fail_allocation=false;std::fprintf(stderr,"FAIL retail startup: %s\n",e.what());return 1;}}
