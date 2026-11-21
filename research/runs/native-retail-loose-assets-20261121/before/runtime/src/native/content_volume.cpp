#include "mka/content_volume.hpp"
#include <new>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
namespace mka::content {
namespace {
#ifdef _WIN32
bool query_handle_path(HANDLE handle,std::filesystem::path& out,std::error_code& error){
    out.clear();error.clear();static_assert(sizeof(wchar_t)==2);
    const auto fail=[&](){const auto code=GetLastError();error={int(code?code:ERROR_GEN_FAILURE),std::system_category()};return false;};
    DWORD capacity=GetFinalPathNameByHandleW(handle,nullptr,0,FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);
    if(!capacity)return fail();
    try{
        std::vector<wchar_t> buffer;
        for(;;){
            if(capacity>buffer.max_size()){error=std::make_error_code(std::errc::value_too_large);return false;}
            buffer.resize(capacity);
            const DWORD size=GetFinalPathNameByHandleW(handle,buffer.data(),capacity,FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);
            if(!size)return fail();
            if(size>=capacity){
                if(size==MAXDWORD){error=std::make_error_code(std::errc::value_too_large);return false;}
                capacity=size+1;continue;
            }
            if(buffer[size]!=L'\0'){error=std::make_error_code(std::errc::invalid_argument);return false;}
            for(DWORD i=0;i<size;++i){const auto c=std::uint16_t(buffer[i]);
                if(!c){error=std::make_error_code(std::errc::invalid_argument);return false;}
                if(c>=0xd800&&c<=0xdbff){
                    if(i+1>=size||std::uint16_t(buffer[i+1])<0xdc00||std::uint16_t(buffer[i+1])>0xdfff){error=std::make_error_code(std::errc::illegal_byte_sequence);return false;}
                    ++i;
                }else if(c>=0xdc00&&c<=0xdfff){error=std::make_error_code(std::errc::illegal_byte_sequence);return false;}
            }
            // Preserve the OS-returned extended namespace. This path is a
            // handle observation, not a new fopen/canonical lookup.
            std::filesystem::path observed(std::wstring(buffer.data(),size));
            if(!observed.is_absolute()){error=std::make_error_code(std::errc::invalid_argument);return false;}
            out=std::move(observed);return true;
        }
    }catch(const std::bad_alloc&){error=std::make_error_code(std::errc::not_enough_memory);}
     catch(const std::length_error&){error=std::make_error_code(std::errc::value_too_large);}
     catch(const std::filesystem::filesystem_error& e){error=e.code();}
    return false;
}
#endif
bool utf8(std::string_view s){
    for(std::size_t i=0;i<s.size();){const auto a=static_cast<unsigned char>(s[i++]);
        if(a<0x80)continue;
        unsigned n=0;std::uint32_t v=0,minimum=0;
        if(a>=0xc2&&a<=0xdf){n=1;v=a&31;minimum=0x80;}
        else if(a>=0xe0&&a<=0xef){n=2;v=a&15;minimum=0x800;}
        else if(a>=0xf0&&a<=0xf4){n=3;v=a&7;minimum=0x10000;}else return false;
        if(n>s.size()-i)return false;
        while(n--){auto b=static_cast<unsigned char>(s[i++]);if((b&0xc0)!=0x80)return false;v=(v<<6)|(b&63);}
        if(v<minimum||v>0x10ffff||(v>=0xd800&&v<=0xdfff))return false;
    }return true;
}
// Comparison only: make the two documented DOS spellings component-comparable.
// Do not reopen a stripped path (extended paths retain different I/O semantics).
std::filesystem::path comparison_path(const std::filesystem::path& path){
#ifdef _WIN32
    auto preferred=path;preferred.make_preferred();const auto& s=preferred.native();
    if(s.size()>=8&&s.compare(0,4,L"\\\\?\\")==0&&
       (s[4]==L'U'||s[4]==L'u')&&(s[5]==L'N'||s[5]==L'n')&&(s[6]==L'C'||s[6]==L'c')&&s[7]==L'\\')return std::filesystem::path(L"\\\\"+s.substr(8));
    if(s.size()>=7&&s.compare(0,4,L"\\\\?\\")==0&&
       ((s[4]>=L'A'&&s[4]<=L'Z')||(s[4]>=L'a'&&s[4]<=L'z'))&&s[5]==L':'&&s[6]==L'\\')return std::filesystem::path(s.substr(4));
#endif
    return path;
}
// libc++ canonical() can return an unprefixed DOS path even when its input
// was extended. Keep the canonical identity but restore an extended spelling
// for subsequent I/O. Only already-canonical absolute paths enter this helper;
// raw user path aliases/dot components are not reinterpreted under a prefix.
std::filesystem::path io_path(const std::filesystem::path& canonical){
#ifdef _WIN32
    auto preferred=canonical;preferred.make_preferred();const auto& s=preferred.native();
    if(s.size()>=4&&(s.compare(0,4,L"\\\\?\\")==0||s.compare(0,4,L"\\\\.\\")==0))return preferred;
    if(s.size()>=3&&((s[0]>=L'A'&&s[0]<=L'Z')||(s[0]>=L'a'&&s[0]<=L'z'))&&s[1]==L':'&&s[2]==L'\\')return std::filesystem::path(L"\\\\?\\"+s);
    if(s.size()>=2&&s[0]==L'\\'&&s[1]==L'\\')return std::filesystem::path(L"\\\\?\\UNC\\"+s.substr(2));
#endif
    return canonical;
}
bool inside(const std::filesystem::path& root,const std::filesystem::path& path){
    auto r=root.begin(),p=path.begin();for(;r!=root.end();++r,++p){if(p==path.end())return false;
        if(*r!=*p)return false;
    }
    return p!=path.end();
}

std::string upper_ascii(std::string_view s){std::string r(s);for(auto& c:r)if(c>='a'&&c<='z')c=char(c-'a'+'A');return r;}
bool device_name(std::string_view part){
    const auto base=upper_ascii(part.substr(0,part.find('.')));
    if(base=="CON"||base=="PRN"||base=="AUX"||base=="NUL"||base=="CONIN$"||base=="CONOUT$")return true;
    if(base.size()==4&&(base.substr(0,3)=="COM"||base.substr(0,3)=="LPT")&&base[3]>='1'&&base[3]<='9')return true;
    return base.size()>=3&&(base.substr(0,3)=="COM"||base.substr(0,3)=="LPT")&&(base.substr(3)=="\xc2\xb9"||base.substr(3)=="\xc2\xb2"||base.substr(3)=="\xc2\xb3");
}
std::optional<std::string> key(std::string_view text,ContentVolumeFailure& f){
    if(!utf8(text)){f.kind=ContentVolumeError::invalid_utf8;return {};}
    if(text.empty()||text.find('\0')!=text.npos){f.kind=ContentVolumeError::invalid_key;return {};}
    const auto colon=text.find(':');if(colon!=text.npos){if(upper_ascii(text.substr(0,colon))!="CDROM0"){f.kind=ContentVolumeError::foreign_prefix;return {};}text.remove_prefix(colon+1);}
    if(!text.empty()&&(text.front()=='/'||text.front()=='\\'))text.remove_prefix(1);
    // Retail220C00 emits this exact final provider suffix for startup IMG.
    // Decode it only; native keys do not manufacture provider/device syntax.
    if(text.size()>=2&&text.substr(text.size()-2)==";1")text.remove_suffix(2);
    if(text.empty()){f.kind=ContentVolumeError::invalid_key;return {};}
    std::string out;out.reserve(text.size());std::size_t begin=0;
    for(std::size_t i=0;i<=text.size();++i)if(i==text.size()||text[i]=='/'||text[i]=='\\'){
        auto part=text.substr(begin,i-begin);
        if(part.empty()){f.kind=ContentVolumeError::invalid_key;return {};}
        if(part=="."||part==".."){f.kind=ContentVolumeError::traversal;return {};}
        if(part.find(';')!=part.npos){f.kind=ContentVolumeError::unsupported_suffix;return {};}
        if(part.back()=='.'||part.back()==' '||device_name(part)){f.kind=ContentVolumeError::unsafe_component;return {};}
        for(unsigned char c:part)if(c<32||c==127||c==':'||c=='*'||c=='?'||c=='"'||c=='<'||c=='>'||c=='|'){f.kind=ContentVolumeError::unsafe_component;return {};}
        if(!out.empty())out.push_back('/');out.append(part);begin=i+1;
    }return out;
}
}
struct NativeContentVolume::State {
    std::filesystem::path root,observed;
#ifdef _WIN32
    HANDLE directory=INVALID_HANDLE_VALUE;
    BY_HANDLE_FILE_INFORMATION identity{};
    ~State(){if(directory!=INVALID_HANDLE_VALUE)CloseHandle(directory);}
    bool unchanged(ContentVolumeFailure& f) const{
        auto h=CreateFileW(io_path(root).c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,nullptr);
        if(h==INVALID_HANDLE_VALUE){f.kind=ContentVolumeError::root_changed;f.host_error={int(GetLastError()),std::system_category()};return false;}
        BY_HANDLE_FILE_INFORMATION now{};const bool ok=GetFileInformationByHandle(h,&now)!=0;const auto error=GetLastError();CloseHandle(h);
        if(!ok||!(now.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)||now.dwVolumeSerialNumber!=identity.dwVolumeSerialNumber||now.nFileIndexHigh!=identity.nFileIndexHigh||now.nFileIndexLow!=identity.nFileIndexLow){f.kind=ContentVolumeError::root_changed;if(!ok)f.host_error={int(error),std::system_category()};return false;}
        return true;
    }
#else
    bool unchanged(ContentVolumeFailure& f)const{f.kind=ContentVolumeError::unsupported_host;return false;}
#endif
};
NativeContentFile::NativeContentFile(std::shared_ptr<const void> v,std::string k,NativeFile&& file):volume_(std::move(v)),key_(std::move(k)),file_(std::move(file)){}
bool NativeContentFile::valid()const noexcept{return volume_&&file_.is_open();}
std::string_view NativeContentFile::logical_key()const noexcept{return valid()?std::string_view(key_):std::string_view{};}
bool NativeContentFile::same_volume(const NativeContentFile& other)const noexcept{return valid()&&other.valid()&&volume_==other.volume_;}
bool NativeContentFile::available(ContentVolumeFailure& f)const{f={};if(!valid()){f.kind=ContentVolumeError::moved_file;return false;}return true;}
FileRead NativeContentFile::read(void* out,std::size_t count,ContentVolumeFailure& f){if(!available(f))return {0,false,std::make_error_code(std::errc::bad_file_descriptor)};auto r=file_.read(out,count);if(r.error){f.kind=ContentVolumeError::read_failed;f.host_error=r.error;}return r;}
bool NativeContentFile::seek(std::int64_t off,SeekOrigin origin,ContentVolumeFailure& f){if(!available(f))return false;if(!file_.seek(off,origin,f.host_error)){f.kind=ContentVolumeError::seek_failed;return false;}return true;}
std::optional<std::uint64_t> NativeContentFile::length(ContentVolumeFailure& f){if(!available(f))return {};std::uint64_t n;if(!file_.length(n,f.host_error)){f.kind=ContentVolumeError::length_failed;return {};}return n;}
std::optional<std::uint64_t> NativeContentFile::position(ContentVolumeFailure& f){if(!available(f))return {};std::uint64_t n;if(!file_.position(n,f.host_error)){f.kind=ContentVolumeError::position_failed;return {};}return n;}
std::optional<NativeContentVolume> NativeContentVolume::open(const std::filesystem::path& root,ContentVolumeFailure& f){
    f={};
#ifndef _WIN32
    (void)root;f.kind=ContentVolumeError::unsupported_host;return {};
#else
    try {auto s=std::make_shared<State>();std::error_code ec;s->root=std::filesystem::canonical(root,ec);
        if(ec||!std::filesystem::is_directory(io_path(s->root),ec)){f.kind=ContentVolumeError::invalid_root;f.host_error=ec;return {};}
        s->directory=CreateFileW(io_path(s->root).c_str(),FILE_LIST_DIRECTORY|FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,nullptr);
        if(s->directory==INVALID_HANDLE_VALUE||!GetFileInformationByHandle(s->directory,&s->identity)){f.kind=ContentVolumeError::invalid_root;f.host_error={int(GetLastError()),std::system_category()};return {};}
        std::filesystem::path actual;if(!query_handle_path(s->directory,actual,f.host_error)){f.kind=ContentVolumeError::opened_path_unavailable;return {};}
        s->observed=comparison_path(actual);if(!s->unchanged(f))return {};
        return NativeContentVolume(std::move(s));
    }catch(const std::bad_alloc&){f.kind=ContentVolumeError::allocation_failed;}catch(const std::length_error&){f.kind=ContentVolumeError::host_capacity;}catch(const std::filesystem::filesystem_error& e){f.kind=ContentVolumeError::invalid_root;f.host_error=e.code();}catch(...){f.kind=ContentVolumeError::native_exception;}return {};
#endif
}
bool NativeContentVolume::validate(ContentVolumeFailure& f) const noexcept {
    f = {};
    if (!state_) { f.kind = ContentVolumeError::moved_volume; return false; }
    try { return state_->unchanged(f); }
    catch (const std::bad_alloc&) { f.kind = ContentVolumeError::allocation_failed; }
    catch (const std::length_error&) { f.kind = ContentVolumeError::host_capacity; }
    catch (const std::filesystem::filesystem_error& e) {
        f.kind = ContentVolumeError::root_changed; f.host_error = e.code();
    }
    catch (...) { f.kind = ContentVolumeError::native_exception; }
    return false;
}
std::optional<NativeContentFile> NativeContentVolume::open_resource(std::string_view name,ContentVolumeFailure& f)const{
    f={};if(!state_){f.kind=ContentVolumeError::moved_volume;return {};}
    try {auto logical=key(name,f);if(!logical)return {};if(!state_->unchanged(f))return {};
        auto joined=io_path(state_->root)/std::filesystem::u8path(*logical);joined.make_preferred();std::error_code ec;auto path=std::filesystem::canonical(joined,ec);
        if(ec){f.kind=ContentVolumeError::open_failed;f.host_error=ec;return {};}
        // Compare canonical lookup spellings together; observed handle paths
        // remain a separate post-open containment check, never reopened.
        if(!inside(comparison_path(state_->root),comparison_path(path))){f.kind=ContentVolumeError::root_escape;return {};}
        if(!std::filesystem::is_regular_file(io_path(path),ec)||ec){f.kind=ContentVolumeError::not_regular_file;f.host_error=ec;return {};}
        NativeFile file;if(!file.open(io_path(path),f.host_error)){f.kind=ContentVolumeError::open_failed;return {};}
        bool regular=false;if(!file.opened_regular_file(regular,f.host_error)||!regular){f.kind=ContentVolumeError::not_regular_file;return {};}
        std::filesystem::path opened;if(!file.opened_path(opened,f.host_error)){f.kind=ContentVolumeError::opened_path_unavailable;return {};}
        if(!inside(state_->observed,comparison_path(opened))){f.kind=ContentVolumeError::root_escape;return {};}
        if(!state_->unchanged(f))return {};
        return NativeContentFile(state_,std::move(*logical),std::move(file));
    }catch(const std::bad_alloc&){f.kind=ContentVolumeError::allocation_failed;}catch(const std::length_error&){f.kind=ContentVolumeError::host_capacity;}catch(const std::filesystem::filesystem_error& e){f.kind=ContentVolumeError::open_failed;f.host_error=e.code();}catch(...){f.kind=ContentVolumeError::native_exception;}return {};
}
} // namespace mka::content
