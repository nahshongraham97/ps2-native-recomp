#include "mka/native_user_paths.hpp"
#include <cstdio>
#include <fstream>
#include <chrono>
#include <stdexcept>
using namespace mka::platform;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main(){try {
    NativeUserPaths real; UserPathFailure f;
#ifdef _WIN32
    check(NativeUserPaths::resolve(real,f),"read native known folder");
    check(real.logs().is_absolute() && real.logs().filename()=="logs","known folder shape");
    // No create_logs or writes against this actual profile path.
#endif
    const auto temp_parent=std::filesystem::canonical(std::filesystem::temp_directory_path());
    auto temp=temp_parent / ("mka-user-paths-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    check(std::filesystem::create_directory(temp),"exclusive temporary root");
    const auto owned_temp=std::filesystem::canonical(temp);
    check(owned_temp.is_absolute() && owned_temp.parent_path()==temp_parent && owned_temp==temp,"owned cleanup boundary");
    struct Cleanup {std::filesystem::path p,parent;~Cleanup(){
        std::error_code e;const auto actual=std::filesystem::canonical(p,e);
        if(e || !actual.is_absolute() || actual!=p || actual.parent_path()!=parent || std::filesystem::is_symlink(p,e)) {
            std::fprintf(stderr,"native-user-paths cleanup refused: changed boundary\n");return;
        }
        std::filesystem::remove_all(actual,e);
        if(e)std::fprintf(stderr,"native-user-paths cleanup failed: %d\n",e.value());
    }} cleanup{owned_temp,temp_parent};
    auto root=temp / std::filesystem::path(L"Unicode-\u00e9-\u65e5");std::filesystem::create_directory(root);
    NativeUserPaths p;check(NativeUserPaths::from_local_app_data(root,p,f),"explicit Unicode root");
    check(!std::filesystem::exists(p.application()),"resolve creates nothing");
    check(p.application().filename()=="MortalKombatArmageddon","project component");
    check(p.create_logs(f) && p.create_logs(f),"create and existing logs");
    auto unicode_log=p.logs()/std::filesystem::path(L"census-\u65e5.txt");
    std::error_code open_error;
    auto* native=open_native_file(unicode_log,NativeFileMode::write_binary,open_error);check(native!=nullptr,"same Unicode census open helper");
    check(std::fprintf(native,"[census] unicode fixture\n")>0,"Unicode census write");check(std::fclose(native)==0,"Unicode census close");
    native=open_native_file(unicode_log,NativeFileMode::read_binary,open_error);check(native!=nullptr,"Unicode census reopen");
    char census[80]{};check(std::fgets(census,sizeof census,native)!=nullptr,"Unicode census read");check(std::fclose(native)==0,"Unicode census read close");
    check(std::string(census)=="[census] unicode fixture\n","Unicode census bytes unchanged");
    check(!open_native_file({},NativeFileMode::read_binary,open_error) && open_error==std::errc::invalid_argument,"empty native file rejected");
    check(!open_native_file(unicode_log,static_cast<NativeFileMode>(99),open_error) && open_error==std::errc::invalid_argument,"invalid native file mode");
    auto nul_path=unicode_log.native();nul_path.push_back(0);nul_path+=L"suffix";
    check(!open_native_file(std::filesystem::path(nul_path),NativeFileMode::read_binary,open_error) && open_error==std::errc::invalid_argument,"embedded NUL rejected");
    check(!open_native_file(p.logs()/"absent",NativeFileMode::read_binary,open_error) && bool(open_error),"missing native file error");
    auto log=p.logs()/"test.log";{std::ofstream o(log);o<<"native log";check(bool(o),"temp write");}
    std::ifstream in(log);std::string text;std::getline(in,text);check(text=="native log","temp read");in.close();
    NativeUserPaths bad=p;
    check(!NativeUserPaths::from_local_app_data("relative",bad,f) && bad.logs().empty() && f.kind==UserPathError::invalid_root,"relative clears stale output");
    check(!NativeUserPaths::from_local_app_data(temp/"missing",bad,f),"missing root");
    check(!NativeUserPaths::from_local_app_data(log,bad,f),"file root");
    check(!NativeUserPaths::from_local_app_data(root/"..",bad,f),"traversal root");
    auto blocked=temp/"blocked";std::filesystem::create_directory(blocked);{std::ofstream o(blocked/"MortalKombatArmageddon");o<<"file";}
    check(NativeUserPaths::from_local_app_data(blocked,bad,f),"resolve blocked child without create");
    check(!bad.create_logs(f) && f.kind==UserPathError::create_logs,"named creation failure");
    NativeUserPaths empty;check(!empty.create_logs(f),"unresolved rejected");
    std::puts("native-user-paths PASS: read-only knownfolder; temporary Unicode/create/IO/errors");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"native-user-paths FAIL: %s\n",e.what());return 1;}}
