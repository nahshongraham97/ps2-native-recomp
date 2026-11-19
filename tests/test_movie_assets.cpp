// Expose the SDK's Windows10 case-sensitivity enum in this test only. Actual
// OS/permission support is queried below; no machine policy is changed.
#ifdef _WIN32
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0a00
#undef NTDDI_VERSION
#define NTDDI_VERSION 0x0a000007
#endif
#include "mka/movie_assets.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <winioctl.h>
#endif
using namespace mka::video;
namespace {
template<class Char> std::string utf8_bytes(const std::basic_string<Char>& bytes){return std::string(reinterpret_cast<const char*>(bytes.data()),bytes.size()*sizeof(Char));}
template<class Char,std::size_t N> std::string utf8_bytes(const Char (&bytes)[N]){static_assert(sizeof(Char)==1);return std::string(reinterpret_cast<const char*>(bytes),N-1);}
void check(bool v,const char* m){if(!v){std::fprintf(stderr,"FAIL: %s\n",m);std::exit(1);}}
void write(const std::filesystem::path& p,const std::string& text){std::ofstream f(p,std::ios::binary);f.write(text.data(),text.size());check(bool(f),"ownedfixturewrite");}
std::vector<unsigned char> all(NativeMovieAsset& a,MovieAssetFailure& f){
 check(a.seek(0,mka::content::SeekOrigin::begin,f),"seekbegin");std::vector<unsigned char> result;unsigned char buffer[4096];
 for(;;){auto r=a.read(buffer,sizeof(buffer),f);check(!r.error&&f.kind==MovieAssetError::none,"NativeFilebytes");result.insert(result.end(),buffer,buffer+r.bytes);if(r.eof)break;check(r.bytes!=0,"nozeroprogresssuccess");}return result;
}
#ifdef _WIN32
void case_sensitive_containment(const std::filesystem::path& out){
 const auto parent=out/"exclusive-case-sensitive";check(std::filesystem::create_directory(parent),"exclusivecasefixture");
 const auto handle=CreateFileW(parent.c_str(),FILE_READ_ATTRIBUTES|FILE_WRITE_ATTRIBUTES,
  FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,nullptr);
 check(handle!=INVALID_HANDLE_VALUE,"opencasefixtureonly");
 FILE_CASE_SENSITIVE_INFO requested{FILE_CS_FLAG_CASE_SENSITIVE_DIR};
 if(!SetFileInformationByHandle(handle,FileCaseSensitiveInfo,&requested,sizeof(requested))){
  const auto error=GetLastError();check(CloseHandle(handle),"closeunsupportedcasefixture");
  std::printf("NOT_EXERCISED case-sensitive directory containment: SetFileInformationByHandle error=%lu\n",error);return;
 }
 FILE_CASE_SENSITIVE_INFO observed{};check(GetFileInformationByHandleEx(handle,FileCaseSensitiveInfo,&observed,sizeof(observed))&&
  (observed.Flags&FILE_CS_FLAG_CASE_SENSITIVE_DIR),"verifyfixturecaseflag");check(CloseHandle(handle),"closecasefixturehandle");
 const auto lower=parent/"root",upper=parent/"ROOT";check(std::filesystem::create_directory(lower)&&std::filesystem::create_directory(upper),"distinctcaseonlyrootsactuallyexist");
 write(lower/"clip.sfd","lower-root");write(upper/"clip.sfd","upper-root");
 MovieAssetFailure f;auto a=NativeMovieAssetStore::create(lower,{{"clip.sfd","clip.sfd"}},f),b=NativeMovieAssetStore::create(upper,{{"clip.sfd","clip.sfd"}},f);
 check(a&&b,"caseonlyrootscapturedfromactualdirectoryhandles");auto af=a->open("CLIP.SFD",f),bf=b->open("CLIP.SFD",f);check(af&&bf,"distinctcaseonlyrootassets");
 const auto av=all(*af,f),bv=all(*bf,f);check(std::string(av.begin(),av.end())=="lower-root"&&std::string(bv.begin(),bv.end())=="upper-root","caseonlyrootidentitiesneverfold");
 std::puts("EXERCISED case-sensitive directory roots with distinct exact handle identities");
 std::error_code ec;std::filesystem::create_symlink(std::filesystem::absolute(upper/"clip.sfd"),lower/"escape.sfd",ec);
 if(ec){std::printf("NOT_EXERCISED case-sensitive sibling escape rejection: symlink error=%d %s\n",ec.value(),ec.message().c_str());return;}
 check(!NativeMovieAssetStore::create(lower,{{"escape.sfd","escape.sfd"}},f)&&f.kind==MovieAssetError::root_escape,"actualcaseonlysiblingtargetisoutsideregisteredroot");
 std::puts("EXERCISED exact-handle containment rejects case-only sibling symlink target");
}
void opened_path(const std::filesystem::path& request,const std::filesystem::path& expected){
 mka::content::NativeFile native;std::error_code error;std::filesystem::path observed="stale-path";
 check(!native.opened_path(observed,error)&&observed.empty()&&error==std::errc::bad_file_descriptor,"closedhandlequeryclearsoutput");
 check(native.open(request,error),"openactualqueryfixture");
 check(native.seek(1,mka::content::SeekOrigin::begin,error),"positionbeforehandlequery");
 check(native.opened_path(observed,error)&&!error&&observed.is_absolute(),"actualopenedhandlefinalpath");
 check(observed.native().rfind(L"\\\\?\\",0)==0,"extendedDOSnamespacepreservednotreopened");
 check(std::filesystem::equivalent(observed,expected,error)&&!error,"independentfileidentitycomparison");
 check(observed.filename()==expected.filename(),"handlefinalnamereflectstargetnotrequestalias");
 std::uint64_t position=0;check(native.position(position,error)&&position==1,"handlequerydoesnotmovecursor");
 auto moved=std::move(native);observed="stale-path";
 check(!native.opened_path(observed,error)&&observed.empty()&&error==std::errc::bad_file_descriptor,"movedhandlequerynamedandclear");
 check(moved.opened_path(observed,error)&&std::filesystem::equivalent(observed,expected,error)&&!error,"queryfollowsmovedactualhandle");
 moved.close();observed="stale-path";check(!moved.opened_path(observed,error)&&observed.empty()&&error==std::errc::bad_file_descriptor,"closedmovedhandlequeryclear");
 check(moved.open(request,error)&&!moved.open(std::filesystem::path{},error),"failedreopenclosesprevioushandle");observed="stale-path";
 check(!moved.opened_path(observed,error)&&observed.empty()&&error==std::errc::bad_file_descriptor,"failedreopenqueryhasnostalepath");
}
#endif
}
int main(int argc,char** argv){
 if(argc!=3){std::fprintf(stderr,"usage: test_movie_assets <pinnedLOGON.sfd> <NEW_owned_fixture_directory>\n");return 2;}
 const auto source=std::filesystem::u8path(argv[1]),out=std::filesystem::u8path(argv[2]);check(std::filesystem::create_directory(out),"exclusiveownedfixtures");MovieAssetFailure f;
#ifdef _WIN32
 opened_path(source,source);
 std::filesystem::path actual_root;std::error_code directory_error;
 check(mka::content::NativeFile::directory_path(source.parent_path(),actual_root,directory_error)&&
  std::filesystem::equivalent(actual_root,source.parent_path(),directory_error)&&!directory_error,"actualdirectoryhandlepath");
 actual_root="stale-path";check(!mka::content::NativeFile::directory_path(source,actual_root,directory_error)&&actual_root.empty()&&directory_error==std::errc::not_a_directory,"filecannotmasqueradeasdirectoryhandle");
 actual_root="stale-path";check(!mka::content::NativeFile::directory_path(out/"absent-directory",actual_root,directory_error)&&actual_root.empty()&&directory_error,"missingdirectoryqueryclearsoutput");
 case_sensitive_containment(out);
#endif
 const std::vector<MovieAssetCatalogEntry> catalog{{"LOGON.SFD",utf8_bytes(source.filename().u8string())}};
 auto store=NativeMovieAssetStore::create(source.parent_path(),catalog,f);check(bool(store),"explicitactualMOVIEPS2rootcatalog");auto file=store->open("logon.sfd",f);check(file&&file->logical_key()=="LOGON.SFD"&&file->associated_path(),"ASCIIcasecataloglookup");
 auto bytes=all(*file,f);check(bytes.size()==3321856,"exactpinnedLOGONextent");std::uint64_t fnv=14695981039346656037ull;for(auto b:bytes)fnv=(fnv^b)*1099511628211ull;check(fnv==0x5367995b434ec1a0ull,"allactualLOGONbytesknownsourcefingerprint");
 std::ifstream independent(source,std::ios::binary);std::vector<unsigned char> reference((std::istreambuf_iterator<char>(independent)),{});check(reference==bytes,"independentfullfilebytecomparison");
 check(file->seek(123,mka::content::SeekOrigin::begin,f)&&file->position(f)==123,"bytepositionnotsector");check(file->length(f)==3321856&&file->position(f)==123,"lengthpreservescursor");check(file->seek(-23,mka::content::SeekOrigin::current,f)&&file->position(f)==100,"relativebyteseek");check(file->seek(-1,mka::content::SeekOrigin::end,f),"endbyteseek");unsigned char last;auto read=file->read(&last,1,f);check(read.bytes==1&&!read.error&&last==bytes.back(),"exactlastbyte");read=file->read(&last,1,f);check(read.bytes==0&&read.eof&&!read.error,"explicitEOF");check(!file->seek(-1,mka::content::SeekOrigin::begin,f)&&f.kind==MovieAssetError::file_seek,"invalidseeknamed");
 auto copy=*store;auto moved=std::move(*store);check(!store->open("LOGON.SFD",f)&&f.kind==MovieAssetError::moved_store,"movedstore");store.reset();check(copy.open("LOGON.SFD",f)&&moved.open("LOGON.SFD",f),"immutablecopy/movestoreownership");auto owned=std::move(*file);check(!file->valid()&&!file->associated_path()&&file->logical_key().empty(),"movedassetmetadataunbound");check(file->read(&last,1,f).error&&f.kind==MovieAssetError::moved_asset,"movedassetreadnamed");check(owned.seek(0,mka::content::SeekOrigin::begin,f)&&owned.read(&last,1,f).bytes==1&&last==0,"retainedopenedNativeFile");
 const auto root=out/"root";check(std::filesystem::create_directory(root),"fixtureroot");check(std::filesystem::create_directories(root/std::filesystem::u8path(utf8_bytes(u8"KRYPT/nested\u00e9"))),"nestedUTF8directory");write(root/std::filesystem::u8path(utf8_bytes(u8"KRYPT/nested\u00e9/Cl\u00edp.sfd")),"krypt-unicode-data");write(root/"logon-copy.sfd","different-context");write(root/"clip;edition.sfd","semicolon-name");
 std::vector<MovieAssetCatalogEntry> fixtures{{utf8_bytes(u8"KRYPT/nested\u00e9/cl\u00edp.sfd"),utf8_bytes(u8"KRYPT/nested\u00e9/Cl\u00edp.sfd")},{"LOGON.SFD","logon-copy.sfd"},{"clip;edition.sfd","clip;edition.sfd"},{"clip;1.sfd","clip;edition.sfd"}};
 auto other=NativeMovieAssetStore::create(root,fixtures,f);check(bool(other),"explicitKRYPTnestedUTF8catalog");auto semicolon=other->open("CLIP;EDITION.SFD",f);check(semicolon&&all(*semicolon,f)==std::vector<unsigned char>({'s','e','m','i','c','o','l','o','n','-','n','a','m','e'}),"registerednonISOsemicolonname");check(bool(other->open("clip;1.sfd",f)),"nonterminalISOliketextallowed");check(!other->open("clip.sfd;1",f)&&f.kind==MovieAssetError::iso_provider_suffix,"terminalISOprovidersuffixrejected");auto nested=other->open(utf8_bytes(u8"KrYpT\\NeStEd\u00e9\\CL\u00edP.SFD"),f);check(bool(nested),"separatorASCIIcaseUnicodepreserved");auto text=all(*nested,f);check(std::string(text.begin(),text.end())=="krypt-unicode-data","exactnestedasset");check(!other->open(utf8_bytes(u8"KRYPT/nested\u00c9/CL\u00edP.SFD"),f)&&f.kind==MovieAssetError::unknown_key,"noinventedUnicodecasefold");auto alternate=other->open("LOGON.SFD",f);check(bool(alternate)&&all(*alternate,f)!=bytes,"contextsneverretargetbyglobalname");other.reset();check(all(*nested,f)==text&&all(*alternate,f)!=bytes,"filesoutlivetheirstore");
#ifdef _WIN32
 opened_path(root/std::filesystem::u8path(utf8_bytes(u8"KRYPT/nested\u00e9/Cl\u00edp.sfd")),root/std::filesystem::u8path(utf8_bytes(u8"KRYPT/nested\u00e9/Cl\u00edp.sfd")));
 write(root/"MixedCaseMovie.SFD","case-alias-data");
 const auto alias_root=out/"RoOt",alias_path=alias_root/"mIxEdCaSeMoViE.sFd";
 if(std::filesystem::exists(alias_path)){
  auto case_store=NativeMovieAssetStore::create(alias_root,{{"actual.sfd","mIxEdCaSeMoViE.sFd"}},f);
  check(bool(case_store),"validWindowscataloghost/rootcasealiascapturesactualhandlepath");
  auto case_asset=case_store->open("ACTUAL.SFD",f);check(bool(case_asset),"actualhandleidentitydoesnotcompareagainstcallerpathcase");
  const auto case_bytes=all(*case_asset,f);check(std::string(case_bytes.begin(),case_bytes.end())=="case-alias-data","casealiasopensregisteredactualbytes");
  opened_path(alias_path,root/"MixedCaseMovie.SFD");
  std::puts("EXERCISED Windows catalog relative/root case aliases using actual opened-handle spelling");
 }else std::puts("NOT_EXERCISED ordinary Windows case-alias fixture: this directory does not expose the alternate case; no per-directory policy changed");
 const auto absolute=std::filesystem::absolute(root);const auto extended=absolute.native().rfind(L"\\\\?\\",0)==0?absolute:std::filesystem::path(L"\\\\?\\"+absolute.native());
 const auto longdir=extended/std::wstring(100,L'x')/std::wstring(100,L'y');check(std::filesystem::create_directories(longdir),"ownedlongDOSnamespacefixture");
 const auto longfile=longdir/std::filesystem::u8path(utf8_bytes(u8"long-\u00e9.sfd"));write(longfile,"long-path-data");check(longfile.native().size()>260,"realextendedlongpath");opened_path(longfile,longfile);
 auto longstore=NativeMovieAssetStore::create(longdir,{{"long.sfd",utf8_bytes(u8"long-\u00e9.sfd")}},f);check(bool(longstore),"extendedlongrootcatalog");auto longasset=longstore->open("LONG.SFD",f);check(bool(longasset)&&all(*longasset,f)==std::vector<unsigned char>({'l','o','n','g','-','p','a','t','h','-','d','a','t','a'}),"actualopenedlongpathassociation");
#endif
 for(const std::string bad:std::vector<std::string>{"", "/LOGON.SFD", "\\LOGON.SFD", "C:/LOGON.SFD", "../LOGON.SFD", "KRYPT/../LOGON.SFD", "./LOGON.SFD", "LOGON.SFD;1", "KRYPT//LOGON.SFD", "LOGON.SFD.", "LOGON.SFD ","LOGON.SFD:stream",std::string("a\0b",3)})check(!copy.open(bad,f)&&f.kind!=MovieAssetError::none,"invalidnamesneverfallback");
 for(const std::string bad:{std::string("\xc0\xaf",2),std::string("\xed\xa0\x80",3),std::string("\xf4\x90\x80\x80",4),std::string("\xc3",1)})check(!copy.open(bad,f)&&f.kind==MovieAssetError::invalid_utf8,"invalidUTF8named");
 check(!copy.open("TITLEN.SFD",f)&&f.kind==MovieAssetError::unknown_key,"existingbutunregisteredisnotsearched");check(!NativeMovieAssetStore::create(root,{{"abc.sfd","logon-copy.sfd"},{"ABC.SFD","logon-copy.sfd"}},f)&&f.kind==MovieAssetError::duplicate_key,"ASCIIcasealiasesrejected");check(!NativeMovieAssetStore::create(root,{{"a/b.sfd","logon-copy.sfd"},{"A\\B.SFD","logon-copy.sfd"}},f)&&f.kind==MovieAssetError::duplicate_key,"separatoraliasesrejected");
 check(!NativeMovieAssetStore::create(out/"missing",{},f)&&f.kind==MovieAssetError::invalid_root,"missingroot");check(!NativeMovieAssetStore::create(root,{{"file.sfd","missing.sfd"}},f)&&f.kind==MovieAssetError::invalid_host_path,"missingcatalogtarget");check(!NativeMovieAssetStore::create(root,{{"file.sfd","../escape.sfd"}},f)&&f.kind==MovieAssetError::traversal,"hosttraversal");check(!NativeMovieAssetStore::create(root,{{"file.sfd","KRYPT"}},f)&&f.kind==MovieAssetError::not_regular_file,"directorynotmovie");
 // No read-size/seek cap inherited from a console: real sparse NativeFile extent.
 const auto sparse=root/"large.sfd";
#ifdef _WIN32
 {HANDLE h=CreateFileW(sparse.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);check(h!=INVALID_HANDLE_VALUE,"exclusive64bitfixture");DWORD returned=0;check(DeviceIoControl(h,FSCTL_SET_SPARSE,nullptr,0,nullptr,0,&returned,nullptr),"marktrulysparsebeforeextension");LARGE_INTEGER position;position.QuadPart=(std::uint64_t(1)<<32)+16;check(SetFilePointerEx(h,position,nullptr,FILE_BEGIN),"sparse64bitseek");DWORD written=0;const char marker='z';check(WriteFile(h,&marker,1,&written,nullptr)&&written==1&&CloseHandle(h),"writesparsemarkeronly");}
#else
 {std::ofstream f(sparse,std::ios::binary);f.seekp((std::uint64_t(1)<<32)+16);f.put('z');check(bool(f),"64bitsparsefixture");}
#endif
 auto largeStore=NativeMovieAssetStore::create(root,{{"large.sfd","large.sfd"}},f);check(bool(largeStore),"largecatalog");auto large=largeStore->open("LARGE.SFD",f);check(large&&large->length(f)==(std::uint64_t(1)<<32)+17&&large->seek(-1,mka::content::SeekOrigin::end,f)&&large->read(&last,1,f).bytes==1&&last=='z',"beyond4GiBhostfilenoPS2cap");
#ifdef _WIN32
 opened_path(sparse,sparse);
#endif
 write(out/"outside.sfd","outside");std::error_code symlink_error;std::filesystem::create_symlink(std::filesystem::absolute(out/"outside.sfd"),root/"escape-link.sfd",symlink_error);if(!symlink_error){check(!NativeMovieAssetStore::create(root,{{"escape.sfd","escape-link.sfd"}},f)&&f.kind==MovieAssetError::root_escape,"resolvedsymlinkcannotescaperoot");
#ifdef _WIN32
 opened_path(root/"escape-link.sfd",out/"outside.sfd");
#endif
 }else std::printf("NOT_EXERCISED symlink escape fixture: %s\n",symlink_error.message().c_str());
 std::puts("PASS explicitcatalog/actualLOGONbytes/KRYPTnestedUTF8/ASCIIcase/separators/NativeFile64/ownedlifetime/nofallback; catalog authenticity remains caller verified");
}
