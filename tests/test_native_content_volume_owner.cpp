#include "mka/content_volume.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <winioctl.h>
#endif
using namespace mka::content;
namespace {
void check(bool b,const char* why){if(!b){std::fprintf(stderr,"FAIL: %s\n",why);std::abort();}}
void write(const std::filesystem::path& p,const char* s){std::ofstream f(p,std::ios::binary|std::ios::trunc);f<<s;check(bool(f),"owned fixture write");}
std::string read(NativeContentFile& file){char bytes[64]{};ContentVolumeFailure f;auto r=file.read(bytes,sizeof(bytes),f);check(!r.error&&f.kind==ContentVolumeError::none,"actual opened handle read");return {bytes,r.bytes};}
#ifdef _WIN32
void sparse(const std::filesystem::path& p,std::uint64_t length){auto h=CreateFileW(p.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);check(h!=INVALID_HANDLE_VALUE,"exclusive sparse file");DWORD count;check(DeviceIoControl(h,FSCTL_SET_SPARSE,nullptr,0,nullptr,0,&count,nullptr)!=0,"owned sparse capability");LARGE_INTEGER n;n.QuadPart=static_cast<LONGLONG>(length-1);check(SetFilePointerEx(h,n,nullptr,FILE_BEGIN)!=0,"owned sparse64 seek");const char marker='Z';check(WriteFile(h,&marker,1,&count,nullptr)&&count==1,"owned sparse tail write");check(CloseHandle(h)!=0,"owned sparse close");}
#endif
}
int main(int argc,char** argv){
 if(argc!=2){std::fprintf(stderr,"usage: test_native_content_volume_owner <NEW owned directory>\n");return 2;}
 auto diagnostics=std::filesystem::absolute(std::filesystem::path(argv[1]));check(std::filesystem::create_directory(diagnostics),"exclusive new diagnostics");auto root=diagnostics/"root",outside=diagnostics/"outside";check(std::filesystem::create_directory(root)&&std::filesystem::create_directory(outside)&&std::filesystem::create_directory(root/"MODULES"),"owned roots");write(root/"MODULES"/"MSL.IRX","module");write(root/"MODULES"/"DNAS310.IMG","startup bytes");write(root/"LOADING.RAW","loading bytes");write(outside/"SECRET.BIN","outside");ContentVolumeFailure f;auto volume=NativeContentVolume::open(root,f);
#ifndef _WIN32
 check(!volume&&f.kind==ContentVolumeError::unsupported_host,"opened handle identity explicitly unsupported here");std::puts("PASS_NATIVE_CONTENT_VOLUME_UNSUPPORTED_HOST: Windows containment cases not executed");return 0;
#else
 check(bool(volume),"actual volume without catalog or disc metadata");
 {auto loading=volume->open_resource("cdrom0:\\LOADING.RAW;1",f);check(loading&&loading->logical_key()=="LOADING.RAW"&&read(*loading)=="loading bytes","actual source LOADING.RAW provider syntax");}
 {auto startup=volume->open_resource("cdrom0:\\MODULES\\DNAS310.IMG;1",f);check(startup&&startup->logical_key()=="MODULES/DNAS310.IMG"&&read(*startup)=="startup bytes","exact source220C00 startup provider key");}
 for(auto name:{"MODULES/MSL.IRX","/MODULES/MSL.IRX","cdrom0:\\MODULES\\MSL.IRX","CdRoM0:/MODULES/MSL.IRX"}){auto file=volume->open_resource(name,f);check(file&&file->logical_key()=="MODULES/MSL.IRX"&&read(*file)=="module","source-known retail key syntax");}
 write(root/std::filesystem::u8path("na\xc3\xafve.bin"),"utf8");{auto file=volume->open_resource("na\xc3\xafve.bin",f);check(file&&read(*file)=="utf8","UTF8 host filename preserved");}
 {auto a=volume->open_resource("MODULES/MSL.IRX",f),b=volume->open_resource("MODULES/MSL.IRX",f);auto other=NativeContentVolume::open(root,f);auto foreign=other->open_resource("MODULES/MSL.IRX",f);check(a&&b&&foreign&&a->same_volume(*b)&&!a->same_volume(*foreign),"independent volume identities");auto moved=std::move(*a);check(!a->valid()&&!a->length(f)&&f.kind==ContentVolumeError::moved_file&&read(moved)=="module","move-only opened file contract");}
 write(root/"replacement.bin","first");{auto file=volume->open_resource("replacement.bin",f);check(file&&read(*file)=="first","initial generic resource");}write(root/"replacement.bin","second");{auto file=volume->open_resource("replacement.bin",f);check(file&&read(*file)=="second","later open observes actual asset replacement without catalog");}
 for(auto name:{"",".","..","MODULES/../MSL.IRX","MODULES/./MSL.IRX","MODULES//MSL.IRX","MODULES/MSL.IRX/","//MODULES/MSL.IRX","cdrom1:/MODULES/MSL.IRX","host0:MSL.IRX","C:/MSL.IRX","MODULES/MSL.IRX;2","MODULES/MSL.IRX;01","MODULES/MSL.IRX;1;1","MODULES;1/MSL.IRX","MODULES/MSL.IRX:stream","MODULES/CON","MODULES/NUL.txt","MODULES/COM1","MODULES/LPT9.log","MODULES/trailing.","MODULES/trailing ","MODULES/a?b"})check(!volume->open_resource(name,f)&&f.kind!=ContentVolumeError::none,"unsafe key rejected");
 const std::string nul("MODULES/MSL\0.IRX",16);check(!volume->open_resource(nul,f)&&f.kind==ContentVolumeError::invalid_key,"embedded NUL rejected");check(!volume->open_resource(std::string("\xc0\xaf",2),f)&&f.kind==ContentVolumeError::invalid_utf8,"overlong UTF8 rejected");check(!volume->open_resource(std::string("\xed\xa0\x80",3),f)&&f.kind==ContentVolumeError::invalid_utf8,"UTF8 surrogate rejected");check(!volume->open_resource("missing.bin",f)&&f.kind==ContentVolumeError::open_failed,"missing file named");check(!volume->open_resource("MODULES",f)&&f.kind==ContentVolumeError::not_regular_file,"directory is not file");
 std::error_code ec;std::filesystem::create_directory_symlink(outside,root/"escape",ec);if(!ec)check(!volume->open_resource("escape/SECRET.BIN",f)&&f.kind==ContentVolumeError::root_escape,"actual symlink target cannot escape");else std::printf("SKIP symlink creation: %d\n",ec.value());
 constexpr std::uint64_t size=(std::uint64_t(1)<<32)+123;sparse(root/"large.bin",size);{auto file=volume->open_resource("large.bin",f);check(file&&file->length(f)==size&&file->seek(-1,SeekOrigin::end,f)&&file->position(f)==size-1,"64bit opened length and cursor");char tail=0;check(file->read(&tail,1,f).bytes==1&&tail=='Z'&&file->position(f)==size,"sparse tail bytes");check(!file->seek(0,static_cast<SeekOrigin>(99),f)&&f.kind==ContentVolumeError::seek_failed,"invalid seek named");}
 auto retained=volume->open_resource("MODULES/MSL.IRX",f);auto moved_volume=std::move(*volume);check(!volume->open_resource("MODULES/MSL.IRX",f)&&f.kind==ContentVolumeError::moved_volume,"moved volume cannot open");volume.reset();check(retained&&read(*retained)=="module","file owns volume beyond public move/destruction");
 auto rename_root=diagnostics/"rename-root",renamed=diagnostics/"renamed";check(std::filesystem::create_directory(rename_root),"owned rename root");write(rename_root/"file.bin","old");auto stable=NativeContentVolume::open(rename_root,f);check(bool(stable),"stable actual root handle");std::error_code rename_error;std::filesystem::rename(rename_root,renamed,rename_error);check(bool(rename_error)&&std::filesystem::exists(rename_root),"retained root denies rename while volume owns it");auto anchored=stable->open_resource("file.bin",f);check(anchored&&read(*anchored)=="old","failed rename preserves original opened root");stable.reset();rename_error.clear();std::filesystem::rename(rename_root,renamed,rename_error);check(bool(rename_error),"opened file retains volume root anchor after public volume dies");anchored.reset();std::filesystem::rename(rename_root,renamed);check(std::filesystem::create_directory(rename_root),"final owner cleanup permits rename and replacement");write(rename_root/"file.bin","new");auto fresh=NativeContentVolume::open(rename_root,f);auto file=fresh->open_resource("file.bin",f);check(file&&read(*file)=="new","new explicit volume owns replacement only after old anchor disposal");
 std::puts("PASS_NATIVE_CONTENT_VOLUME_OWNER: opened identity/retail keys/containment/moves/root replacement/sparse64 files; no startup/schema claim");return std::fflush(stdout)||std::ferror(stdout)?1:0;
#endif
}
