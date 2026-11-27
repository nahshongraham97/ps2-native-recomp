#include "mka/retail_ssf_bundle.hpp"
#include <algorithm>
#include <bit>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace mka::retail {
namespace {
constexpr std::uint32_t sec_magic=0x53454320;
std::uint32_t u32(std::span<const std::byte> b,std::size_t p) noexcept {
    return std::to_integer<std::uint32_t>(b[p]) | (std::to_integer<std::uint32_t>(b[p+1])<<8) |
        (std::to_integer<std::uint32_t>(b[p+2])<<16) | (std::to_integer<std::uint32_t>(b[p+3])<<24);
}
bool fail(RetailSsfBundleFailure& f,RetailSsfBundleError e,const char* op) noexcept {
    f.kind=e;f.operation=op;return false;
}
struct Chunk {std::uint32_t id;std::span<const std::byte> bytes;};
// Walk only the declared immediate container, never search for signatures.
bool children(std::span<const std::byte> b,std::vector<Chunk>& out) {
    while(!b.empty()) {
        if(b.size()<12)return false;
        const auto n=u32(b,4);
        if(n>b.size()-12 || u32(b,8)!=0x1803ffff)return false;
        out.push_back({u32(b,0),b.first(12+std::size_t(n))});
        b=b.subspan(12+std::size_t(n));
    }
    return true;
}
bool clump_container(std::span<const std::byte> rw) {
    std::vector<Chunk> cc;
    if(!children(rw.subspan(12),cc))return false;
    for(auto id:{1u,14u,26u})if(std::count_if(cc.begin(),cc.end(),[id](const auto& c){return c.id==id;})!=1)return false;
    const auto gl=std::find_if(cc.begin(),cc.end(),[](const auto& c){return c.id==26;});
    std::vector<Chunk> gc;
    if(!children(gl->bytes.subspan(12),gc)||gc.empty()||gc[0].id!=1||gc[0].bytes.size()<16)return false;
    const auto expected=u32(gc[0].bytes,12);
    if(std::count_if(gc.begin()+1,gc.end(),[](const auto& c){return c.id==15;})!=expected)return false;
    // Native geometry STRUCT lengths are not used to locate later chunks.
    for(const auto& c:gc)if(c.id==15) {
        if(c.bytes.size()<40||u32(c.bytes,12)!=1||u32(c.bytes,16)<16||u32(c.bytes,16)>c.bytes.size()-24)return false;
    }
    return true;
}
} // namespace

RetailSsfResourceView::RetailSsfResourceView(std::shared_ptr<const NativeRetailSsfArtSection> o,
    std::size_t i,std::span<const std::byte> b) noexcept:owner_(std::move(o)),index_(i),bytes_(b) {}
const content::ArtSectionRecord& RetailSsfResourceView::record() const noexcept {return owner_->records()[index_];}
RetailSsfClumpView::RetailSsfClumpView(RetailSsfResourceView r,std::span<const std::byte> rw,
    std::int32_t first,std::int32_t last,std::vector<std::size_t> textures) noexcept:
    resource_(std::move(r)),rw_(rw),first_(first),last_(last),texture_records_(std::move(textures)) {}
NativeRetailSsfArtSection::NativeRetailSsfArtSection(content::NativeArtSectionData d) noexcept:data_(std::move(d)) {}

std::unique_ptr<NativeRetailSsfBundle> NativeRetailSsfBundle::parse(std::vector<std::byte> b,RetailSsfBundleFailure& f) noexcept {
    f={};try {
        if(b.size()<28){fail(f,RetailSsfBundleError::invalid_header,"ssf.header.truncated");return {};}
        if(u32(b,0)!=sec_magic||u32(b,4)!=4||u32(b,8)!=0){fail(f,RetailSsfBundleError::unsupported_format,"ssf.SEC4.class0");return {};}
        if(u32(b,24)!=b.size()){fail(f,RetailSsfBundleError::invalid_header,"ssf.declared_size");return {};}
        const std::size_t count=u32(b,16);
        if(count>(b.size()-28)/16){fail(f,RetailSsfBundleError::invalid_directory,"ssf.directory.count");return {};}
        auto out=std::unique_ptr<NativeRetailSsfBundle>(new NativeRetailSsfBundle);
        for(std::size_t i=0;i<7;++i)out->header_[i]=u32(b,i*4);
        out->sections_.reserve(count);
        const auto table_end=28+count*16;
        for(std::size_t i=0;i<count;++i) {
            const auto p=28+i*16;
            RetailSsfSectionRecord r{i,u32(b,p),u32(b,p+4),u32(b,p+8),u32(b,p+12)};
            if(r.offset<table_end||r.offset>b.size()||r.size>b.size()-r.offset){fail(f,RetailSsfBundleError::invalid_payload_range,"ssf.section.range");return {};}
            out->sections_.push_back(r);
        }
        auto sorted=out->sections_;
        std::sort(sorted.begin(),sorted.end(),[](const auto& a,const auto& z){return a.offset<z.offset;});
        std::uint64_t end=table_end;
        for(const auto& r:sorted)if(r.size){if(r.offset<end){fail(f,RetailSsfBundleError::overlapping_payloads,"ssf.section.overlap");return {};}end=r.offset+r.size;}
        out->bytes_=std::move(b);return out;
    }catch(const std::length_error&){fail(f,RetailSsfBundleError::host_capacity,"ssf.capacity");}
    catch(const std::bad_alloc&){fail(f,RetailSsfBundleError::allocation,"ssf.allocation");}
    catch(...){fail(f,RetailSsfBundleError::native_exception,"ssf.exception");}return {};
}
std::unique_ptr<NativeRetailSsfBundle> NativeRetailSsfBundle::open(NativeRetailArchive& a,std::size_t i,RetailSsfBundleFailure& f) noexcept {
    f={};try {
        auto bytes=a.extract(i,f.archive);
        if(!bytes){fail(f,RetailSsfBundleError::archive,"ssf.archive.extract");return {};}
        std::vector<std::byte> b(bytes->size());
        std::transform(bytes->begin(),bytes->end(),b.begin(),[](auto x){return std::byte(x);});
        return parse(std::move(b),f);
    }catch(const std::length_error&){fail(f,RetailSsfBundleError::host_capacity,"ssf.capacity");}
    catch(const std::bad_alloc&){fail(f,RetailSsfBundleError::allocation,"ssf.allocation");}
    catch(...){fail(f,RetailSsfBundleError::native_exception,"ssf.exception");}return {};
}
std::unique_ptr<NativeRetailSsfBundle> NativeRetailSsfBundle::open(NativeRetailArchive& a,std::string_view name,RetailSsfBundleFailure& f) noexcept {
    f={};auto r=a.find(name,f.archive);if(!r){fail(f,RetailSsfBundleError::archive,"ssf.archive.find");return {};}return open(a,r->index,f);
}
std::shared_ptr<const NativeRetailSsfArtSection> NativeRetailSsfBundle::open_art_section(std::size_t i,RetailSsfBundleFailure& f) const noexcept {
    f={};try {
        if(i>=sections_.size()){fail(f,RetailSsfBundleError::invalid_section_index,"ssf.section.index");return {};}
        const auto& r=sections_[i];
        if(r.raw_type!=1){fail(f,RetailSsfBundleError::unsupported_section_type,"ssf.section.type");return {};}
        const auto b=std::span<const std::byte>(bytes_).subspan(static_cast<std::size_t>(r.offset),static_cast<std::size_t>(r.size));
        if(b.size()<28||u32(b,0)!=sec_magic||u32(b,4)!=4){fail(f,RetailSsfBundleError::unsupported_section_format,"ssf.section.not_plain_SEC4");return {};}
        auto d=content::NativeArtSectionData::parse(std::vector<std::byte>(b.begin(),b.end()),f.section);
        if(!d){fail(f,RetailSsfBundleError::art_section,"ssf.section.directory");return {};}
        // Inner declared lengths may exclude outer alignment padding. Preserve
        // the existing parser's full bounded range rather than invent equality.
        return std::shared_ptr<const NativeRetailSsfArtSection>(new NativeRetailSsfArtSection(std::move(*d)));
    }catch(const std::length_error&){fail(f,RetailSsfBundleError::host_capacity,"ssf.capacity");}
    catch(const std::bad_alloc&){fail(f,RetailSsfBundleError::allocation,"ssf.allocation");}
    catch(...){fail(f,RetailSsfBundleError::native_exception,"ssf.exception");}return {};
}
std::optional<RetailSsfResourceView> NativeRetailSsfArtSection::resource(std::size_t i,RetailSsfBundleFailure& f) const noexcept {
    f={};try {
        if(i>=records().size()){fail(f,RetailSsfBundleError::invalid_resource_index,"ssf.resource.index");return {};}
        const auto& r=records()[i];
        if(r.raw_type!=2&&r.raw_type!=3&&r.raw_type!=4){fail(f,RetailSsfBundleError::unsupported_resource_type,"ssf.resource.type_or_flags");return {};}
        if(!r.payload_offset){fail(f,RetailSsfBundleError::missing_payload,"ssf.resource.missing_payload");return {};}
        if(std::count_if(records().begin(),records().end(),[&](const auto& x){return x.payload_offset==r.payload_offset;})!=1){fail(f,RetailSsfBundleError::invalid_resource_length,"ssf.resource.shared_payload");return {};}
        const auto b=data_.resource_bytes(i,f.section);
        if(!b){fail(f,RetailSsfBundleError::art_section,"ssf.resource.bounds");return {};}
        if(!r.raw_word_08||r.raw_word_08>b->size()){fail(f,RetailSsfBundleError::invalid_resource_length,"ssf.resource.word08_extent");return {};}
        return RetailSsfResourceView(shared_from_this(),i,b->first(r.raw_word_08));
    }catch(const std::bad_alloc&){fail(f,RetailSsfBundleError::allocation,"ssf.allocation");}
    catch(...){fail(f,RetailSsfBundleError::native_exception,"ssf.exception");}return {};
}
std::optional<RetailSsfResourceView> NativeRetailSsfArtSection::resource(std::string_view name,RetailSsfBundleFailure& f) const noexcept {
    f={};std::size_t index=records().size();
    for(std::size_t i=0;i<records().size();++i)if(records()[i].name==name){
        if(index!=records().size()){fail(f,RetailSsfBundleError::ambiguous_resource,"ssf.resource.duplicate_name");return {};}index=i;
    }
    if(index==records().size()){fail(f,RetailSsfBundleError::missing_resource,"ssf.resource.exact_name");return {};}
    return resource(index,f);
}
std::optional<RetailSsfClumpView> NativeRetailSsfArtSection::clump(std::string_view name,RetailSsfBundleFailure& f) const noexcept {
    auto r=resource(name,f);if(!r)return {};return clump(r->index(),f);
}
std::optional<RetailSsfClumpView> NativeRetailSsfArtSection::clump(std::size_t i,RetailSsfBundleFailure& f) const noexcept {
    auto r=resource(i,f);if(!r)return {};
    try {
        if(r->record().raw_type!=4){fail(f,RetailSsfBundleError::unsupported_resource_type,"ssf.clump.type4");return {};}
        const auto b=r->bytes();
        if(b.size()<20||u32(b,8)!=0x10||u32(b,16)!=0x1803ffff){fail(f,RetailSsfBundleError::unsupported_clump,"ssf.clump.direct_RW_3_1");return {};}
        const auto n=u32(b,12);
        if(n>b.size()-20){fail(f,RetailSsfBundleError::malformed_clump,"ssf.clump.root_bounds");return {};}
        const auto rw=b.subspan(8,12+std::size_t(n));
        if(!clump_container(rw)){fail(f,RetailSsfBundleError::malformed_clump,"ssf.clump.child_partition");return {};}
        const auto first=std::bit_cast<std::int32_t>(u32(b,0)),last=std::bit_cast<std::int32_t>(u32(b,4));
        std::vector<std::size_t> textures;
        if(first==-1) {
            for(std::size_t t=0;t<records().size();++t)if(records()[t].raw_type==2||records()[t].raw_type==3)textures.push_back(t);
        } else {
            if(first<0||last< -1){fail(f,RetailSsfBundleError::invalid_texture_range,"ssf.clump.texture_range_signed");return {};}
            // 3011D0 rebases only unsigned first<=last; an empty range stays empty.
            if(first<=last) {
                auto base=std::find_if(records().begin(),records().end(),[](const auto& x){return x.raw_type==3;});
                const auto begin=std::size_t(base-records().begin());
                if(base==records().end()||std::uint64_t(last)>=records().size()-begin){fail(f,RetailSsfBundleError::invalid_texture_range,"ssf.clump.texture_range_bounds");return {};}
                for(std::size_t t=begin+std::size_t(first);t<=begin+std::size_t(last);++t)if(records()[t].raw_type==3)textures.push_back(t);
            }
        }
        return RetailSsfClumpView(std::move(*r),rw,first,last,std::move(textures));
    }catch(const std::length_error&){fail(f,RetailSsfBundleError::host_capacity,"ssf.capacity");}
    catch(const std::bad_alloc&){fail(f,RetailSsfBundleError::allocation,"ssf.allocation");}
    catch(...){fail(f,RetailSsfBundleError::native_exception,"ssf.exception");}return {};
}
} // namespace mka::retail
