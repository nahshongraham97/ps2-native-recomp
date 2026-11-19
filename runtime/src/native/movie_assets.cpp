#include "mka/movie_assets.hpp"
#include <unordered_map>
#include <new>
#include <stdexcept>
namespace mka::video {
namespace {
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
std::string ascii_key(std::string s){for(auto& c:s)if(c>='a'&&c<='z')c=char(c-'a'+'A');return s;}
std::optional<std::string> relative_name(std::string_view s,bool fold,MovieAssetFailure& f){
    if(!utf8(s)){f.kind=MovieAssetError::invalid_utf8;return std::nullopt;}
    if(s.empty()){f.kind=MovieAssetError::invalid_name;return std::nullopt;}
    if(s.front()=='/'||s.front()=='\\'||(s.size()>1&&s[1]==':')){f.kind=MovieAssetError::absolute_path;return std::nullopt;}
    if(s.size()>=2&&s.substr(s.size()-2)==";1"){f.kind=MovieAssetError::iso_provider_suffix;return std::nullopt;}
    std::string out;out.reserve(s.size());std::size_t begin=0;
    for(std::size_t i=0;i<=s.size();++i)if(i==s.size()||s[i]=='/'||s[i]=='\\'){
        const auto part=s.substr(begin,i-begin);
        if(part.empty()){f.kind=MovieAssetError::invalid_name;return std::nullopt;}
        if(part=="."||part==".."){f.kind=MovieAssetError::traversal;return std::nullopt;}
        // Trailing spaces/dots and ADS/device syntax have host-dependent aliases;
        // these are outside the verified before-provider relative namespace.
        if(part.back()==' '||part.back()=='.'){f.kind=MovieAssetError::invalid_name;return std::nullopt;}
        for(unsigned char c:part){if(c<32||c==127||c==':'||c=='*'||c=='?'||c=='"'||c=='<'||c=='>'||c=='|'){f.kind=MovieAssetError::invalid_name;return std::nullopt;}}
        if(!out.empty())out.push_back('/');out.append(part.data(),part.size());begin=i+1;
    }
    return fold?ascii_key(std::move(out)):std::move(out);
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
}
struct MovieAssetBinding {
    std::string key;std::filesystem::path path;
    // Actual handle-reported spelling, captured while creating this binding.
    // canonical(path) may preserve an alternate caller case on Windows.
    std::filesystem::path observed_path;
};
struct NativeMovieAssetStore::State {
    std::filesystem::path root;
    std::filesystem::path observed_root;
    std::unordered_map<std::string,std::shared_ptr<const MovieAssetBinding>> catalog;
};
bool NativeMovieAsset::valid()const noexcept{return bool(binding_)&&file_.is_open();}
std::string_view NativeMovieAsset::logical_key()const noexcept{return binding_?std::string_view(binding_->key):std::string_view{};}
const std::filesystem::path* NativeMovieAsset::associated_path()const noexcept{return binding_?&binding_->path:nullptr;}
bool NativeMovieAsset::available(MovieAssetFailure& f)const{f={};if(!valid()){f.kind=MovieAssetError::moved_asset;return false;}return true;}
content::FileRead NativeMovieAsset::read(void* data,std::size_t size,MovieAssetFailure& f){
    if(!available(f))return {0,false,std::make_error_code(std::errc::bad_file_descriptor)};
    auto r=file_.read(data,size);if(r.error){f.kind=MovieAssetError::file_read;f.host_error=r.error;}return r;
}
bool NativeMovieAsset::seek(std::int64_t off,content::SeekOrigin origin,MovieAssetFailure& f){
    if(!available(f))return false;if(!file_.seek(off,origin,f.host_error)){f.kind=MovieAssetError::file_seek;return false;}return true;
}
std::optional<std::uint64_t> NativeMovieAsset::position(MovieAssetFailure& f){
    if(!available(f))return std::nullopt;std::uint64_t p;if(!file_.position(p,f.host_error)){f.kind=MovieAssetError::file_position;return std::nullopt;}return p;
}
std::optional<std::uint64_t> NativeMovieAsset::length(MovieAssetFailure& f){
    if(!available(f))return std::nullopt;std::uint64_t n;if(!file_.length(n,f.host_error)){f.kind=MovieAssetError::file_length;return std::nullopt;}return n;
}
std::optional<NativeMovieAssetStore> NativeMovieAssetStore::create(const std::filesystem::path& root,const std::vector<MovieAssetCatalogEntry>& catalog,MovieAssetFailure& f){
    f={};try{auto s=std::make_shared<State>();std::error_code ec;s->root=std::filesystem::canonical(root,ec);
        if(ec||!std::filesystem::is_directory(io_path(s->root),ec)){f.kind=MovieAssetError::invalid_root;f.host_error=ec;return std::nullopt;}
        std::filesystem::path actual_root;
        if(!content::NativeFile::directory_path(io_path(s->root),actual_root,f.host_error)){f.kind=MovieAssetError::opened_path_unavailable;return std::nullopt;}
        s->observed_root=comparison_path(actual_root);
        for(std::size_t i=0;i<catalog.size();++i){f.catalog_index=i;
            auto key=relative_name(catalog[i].logical_utf8,true,f);if(!key)return std::nullopt;
            auto relative=relative_name(catalog[i].relative_host_utf8,false,f);if(!relative){if(f.kind==MovieAssetError::invalid_name)f.kind=MovieAssetError::invalid_host_path;return std::nullopt;}
            if(s->catalog.find(*key)!=s->catalog.end()){f.kind=MovieAssetError::duplicate_key;return std::nullopt;}
            auto joined=io_path(s->root)/std::filesystem::u8path(*relative);
            // operator/ does not replace embedded forward slashes in its RHS.
            // Extended Windows syntax requires native separators throughout.
            joined.make_preferred();
            auto target=std::filesystem::canonical(joined,ec);
            if(ec){f.kind=MovieAssetError::invalid_host_path;f.host_error=ec;return std::nullopt;}
            if(!std::filesystem::is_regular_file(io_path(target),ec)){f.kind=MovieAssetError::not_regular_file;f.host_error=ec;return std::nullopt;}
            content::NativeFile actual;
            if(!actual.open(io_path(target),f.host_error)){f.kind=MovieAssetError::file_open;return std::nullopt;}
            std::filesystem::path observed;
            if(!actual.opened_path(observed,f.host_error)){f.kind=MovieAssetError::opened_path_unavailable;return std::nullopt;}
            auto normalized=comparison_path(observed);
            // Both names are normalized observations of actual handles. A
            // differently cased sibling is not the registered root directory.
            if(!inside(s->observed_root,normalized)){f.kind=MovieAssetError::root_escape;return std::nullopt;}
            auto entry=std::make_shared<MovieAssetBinding>();entry->key=*key;entry->path=std::move(target);entry->observed_path=std::move(normalized);
            s->catalog.emplace(std::move(*key),std::move(entry));
        }f={};return NativeMovieAssetStore(std::move(s));
    }catch(const std::bad_alloc&){f.kind=MovieAssetError::allocation_failed;}catch(const std::length_error&){f.kind=MovieAssetError::host_capacity;}
     catch(const std::filesystem::filesystem_error& e){f.kind=MovieAssetError::invalid_host_path;f.host_error=e.code();}return std::nullopt;
}
std::optional<NativeMovieAsset> NativeMovieAssetStore::open(std::string_view name,MovieAssetFailure& f)const{
    f={};if(!state_){f.kind=MovieAssetError::moved_store;return std::nullopt;}
    try{auto key=relative_name(name,true,f);if(!key)return std::nullopt;auto i=state_->catalog.find(*key);
        if(i==state_->catalog.end()){f.kind=MovieAssetError::unknown_key;return std::nullopt;}
        std::error_code ec;auto current=std::filesystem::canonical(io_path(i->second->path),ec);
        if(ec){f.kind=MovieAssetError::file_open;f.host_error=ec;return std::nullopt;}
        if(comparison_path(current)!=comparison_path(i->second->path)){f.kind=MovieAssetError::association_changed;return std::nullopt;}
        if(!std::filesystem::is_regular_file(io_path(current),ec)){f.kind=MovieAssetError::not_regular_file;f.host_error=ec;return std::nullopt;}
        content::NativeFile file;if(!file.open(io_path(current),f.host_error)){f.kind=MovieAssetError::file_open;return std::nullopt;}
        std::filesystem::path observed;
        if(!file.opened_path(observed,f.host_error)){f.kind=MovieAssetError::opened_path_unavailable;return std::nullopt;}
        const auto opened=comparison_path(observed);
        // No canonical/equivalent/reopen on the observed path: it describes
        // the actual still-owned handle, not another pathname lookup.
        if(opened!=i->second->observed_path||!inside(state_->observed_root,opened)){f.kind=MovieAssetError::opened_path_mismatch;return std::nullopt;}
        return NativeMovieAsset(i->second,std::move(file));
    }catch(const std::bad_alloc&){f.kind=MovieAssetError::allocation_failed;}catch(const std::length_error&){f.kind=MovieAssetError::host_capacity;}
     catch(const std::filesystem::filesystem_error& e){f.kind=MovieAssetError::invalid_host_path;f.host_error=e.code();}return std::nullopt;
}
} // namespace mka::video
