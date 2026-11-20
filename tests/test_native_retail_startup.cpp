#include "native_retail_startup.hpp"
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
using namespace mka::retail;
namespace {
void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
}
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
    std::puts("PASS generated retail startup leaves use owned arguments and actual native volume; full entry link remains blocked");
    return std::fflush(stdout)||std::ferror(stdout)?1:0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL retail startup: %s\n",e.what());return 1;}}
