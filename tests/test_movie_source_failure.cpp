// CPU/filesystem integration failure fixture. No media output or game/devices.
#include "mka/movie_decode.hpp"
#include "native_test_sha256.hpp"
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mka::video;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
std::filesystem::path path(const char* s){std::string text(s);return std::filesystem::path(std::u8string(text.begin(),text.end()));}
std::string sha(const std::filesystem::path& p){std::ifstream in(p,std::ios::binary);check(bool(in),"hash source open");NativeTestSha256 h;std::array<char,131072>b{};while(in){in.read(b.data(),b.size());h.add(b.data(),std::size_t(in.gcount()));}check(in.eof(),"hash source read");return h.finish();}
struct OwnedTemp {
 std::filesystem::path parent,root;
 OwnedTemp(){parent=std::filesystem::canonical(std::filesystem::temp_directory_path());root=parent/("mka-movie-source-failure-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));check(std::filesystem::create_directory(root),"exclusive temporary directory");root=std::filesystem::canonical(root);check(root.is_absolute()&&root.parent_path()==parent,"resolved owned boundary");}
 ~OwnedTemp(){std::error_code ec;auto actual=std::filesystem::canonical(root,ec);if(ec||actual!=root||!actual.is_absolute()||actual.parent_path()!=parent){std::fprintf(stderr,"cleanup refused changed temporary boundary\n");return;}std::filesystem::remove_all(actual,ec);if(ec)std::fprintf(stderr,"temporary cleanup failed host=%d\n",ec.value());}
};
std::unique_ptr<NativeMovieDecoder> opened(const std::filesystem::path& libs,const std::filesystem::path& file,bool catalog,MovieDecodeFailure& f){
 if(!catalog)return NativeMovieDecoder::open(libs,file,f);
 auto u=file.filename().u8string();std::string rel(u.begin(),u.end());MovieAssetFailure a;auto s=NativeMovieAssetStore::create(file.parent_path(),{{"explicit-source",rel}},a);check(bool(s),"temporary explicit catalog");auto asset=s->open("explicit-source",a);check(bool(asset),"temporary catalog opened asset");return NativeMovieDecoder::open(libs,std::move(*asset),f);
}
int main(int argc,char**argv){try{
 check(argc==3,"usage: test_native_movie_source_failure DLL_DIRECTORY EXPLICIT_LOGON_SFD");
 const auto libs=path(argv[1]),source=std::filesystem::canonical(path(argv[2]));
 const auto original_sha=sha(source);check(original_sha=="cf49fb014d51f20fee7727a01e845cc7303be63fe0af2902fb3eadf371a843af","pinned LOGON input SHA");
 OwnedTemp temp;const auto bad=temp.root/"unsupported.sfd";
 {std::ofstream out(bad,std::ios::binary);const std::array<unsigned char,12>b{0,0,1,0xba,0x44,0,0,0,0,0,0,0};out.write(reinterpret_cast<const char*>(b.data()),b.size());check(bool(out),"unsupported pack fixture");}
 for(bool catalog:{false,true}){
  MovieDecodeFailure f;check(!opened(libs,bad,catalog,f)&&f.kind==MovieDecodeError::source_index&&f.source_failure.kind==MoviePesError::unsupported_header,"both opens reject unsupported MPEG2 pack before codec path");
  std::cout<<"invalid_pes route="<<(catalog?"catalog":"path")<<" error=source_index pes=unsupported_header\n";
  const auto copy=temp.root/(catalog?"owned.sfd":"path.sfd");check(std::filesystem::copy_file(source,copy),"exclusive LOGON copy");check(sha(copy)==original_sha,"exact temporary LOGON bytes");
  auto decoder=opened(libs,copy,catalog,f);if(!decoder){std::cerr<<"open error kind="<<int(f.kind)<<" source="<<int(f.source_failure.kind)<<" operation="<<f.operation<<'\n';throw std::runtime_error("temporary LOGON decoder open");}
  std::filesystem::resize_file(copy,0);std::uint64_t buffered_frames=0;
  for(;;){auto next=decoder->next(f);if(next.status==MovieReadStatus::frame){++buffered_frames;continue;}check(next.status==MovieReadStatus::error,"truncated same-open source cannot end successfully");break;}
  std::cout<<"truncated route="<<(catalog?"catalog":"path")<<" buffered_frames="<<buffered_frames<<" kind="<<int(f.kind)<<" pes="<<int(f.source_failure.kind)<<" operation="<<f.operation<<'\n';
  check(f.kind==MovieDecodeError::file_io&&f.source_failure.kind==MoviePesError::truncated&&f.operation=="av_read_frame owned reader","named source truncation terminal error");
  check(decoder->next(f).status==MovieReadStatus::error&&f.kind==MovieDecodeError::failed_owner,"failed decoder cannot replay/fallback after source failure");decoder.reset();
 }
 check(sha(source)==original_sha,"original asset unchanged");
 std::cout<<"PASS native decoder both-open source rejection and terminal truncation; no output/race-authentication claim\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
