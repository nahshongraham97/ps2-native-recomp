#include "mka/retail_permanent_string_data.hpp"
#include <iterator>
#include <new>

namespace mka::retail {
namespace {
bool fail(RetailStringDataFailure& f,RetailStringDataError kind,const char* operation,std::size_t offset=0) noexcept {
    f.kind=kind;f.operation=operation;f.offset=offset;return false;
}
constexpr RetailPermanentStringDescriptor descriptors[]{
    {"permanent_strings_eng.mko",0x1e5800,0x6e0},
    {"permanent_strings_spa.mko",0x1e6000,0x800},
    {"permanent_strings_ger.mko",0x1e6800,0x700},
    {"permanent_strings_fre.mko",0x1e7000,0x760},
    {"permanent_strings_ita.mko",0x1e7800,0x760},
};
std::uint32_t word(std::span<const std::uint8_t> b,std::size_t at) noexcept {
    return std::uint32_t(b[at])|(std::uint32_t(b[at+1])<<8)|
        (std::uint32_t(b[at+2])<<16)|(std::uint32_t(b[at+3])<<24);
}
bool range(std::size_t size,std::size_t at,std::size_t n) noexcept {return at<=size&&n<=size-at;}
}
std::optional<RetailPermanentStringDescriptor> permanent_string_descriptor_from_source(
    std::int32_t language,RetailStringDataFailure& f) noexcept {
    f={};
    if(language<0||language>=std::int32_t(std::size(descriptors))) {
        fail(f,RetailStringDataError::language,"26EF70 actual permanent-string language");return {};
    }
    return descriptors[language];
}
NativeRetailStringValue::NativeRetailStringValue(
    std::shared_ptr<const std::vector<std::uint8_t>> b,std::size_t at,std::size_t n,bool null) noexcept
    :storage_(std::move(b)),offset_(at),length_(n),null_(null) {}
std::span<const std::uint8_t> NativeRetailStringValue::bytes() const & noexcept {
    if(null_)return {};
    return std::span<const std::uint8_t>(*storage_).subspan(offset_,length_);
}
std::shared_ptr<const NativeRetailPermanentStringImage> NativeRetailPermanentStringImage::decode(
    std::vector<std::uint8_t> bytes,RetailStringDataFailure& f) noexcept {
    f={};
    try {
        const auto b=std::span<const std::uint8_t>(bytes);
        if(b.size()<32){fail(f,RetailStringDataError::truncated,"2170E0 MKO header");return {};}
        std::uint32_t h[8];for(unsigned i=0;i<8;++i)h[i]=word(b,i*4);
        if(h[0]!=0){fail(f,RetailStringDataError::unsupported_functions,"required native MKO functions");return {};}
        if(h[1]!=0){fail(f,RetailStringDataError::unsupported_tail,"required native MKO external-reference tail");return {};}
        if(h[6]!=1){fail(f,RetailStringDataError::unsupported_variables,"required data-only MKO variable shape");return {};}
        //2170E0 skips zero function slots and reserves one variable slot.
        // Its serialized placeholder is data, never a relocated guest pointer.
        constexpr std::size_t record_at=36;
        if(!range(b.size(),record_at,20)){fail(f,RetailStringDataError::truncated,"2170E0 variable record",record_at);return {};}
        std::uint32_t r[5];for(unsigned i=0;i<5;++i)r[i]=word(b,record_at+i*4);
        if(r[1]==0){fail(f,RetailStringDataError::unsupported_variables,"required native imported MKO variable");return {};}
        std::uint64_t cursor=record_at+20+std::uint64_t(r[4])*4;
        if(cursor>b.size()){fail(f,RetailStringDataError::truncated,"2170E0 string-reference list");return {};}
        const auto names_at=cursor;cursor+=h[2];
        const auto strings_at=cursor;cursor+=h[3];
        const auto types_at=cursor;cursor+=h[4];
        const auto values_at=cursor;cursor+=h[7];
        if(cursor>b.size()||cursor>std::size_t(-1)) {
            fail(f,RetailStringDataError::truncated,"2170E0 names/string/type/value regions");return {};
        }
        if(!r[0]||r[0]>h[2]){fail(f,RetailStringDataError::malformed_name,"one-based MKO export name");return {};}
        const auto name_at=std::size_t(names_at)+r[0]-1;
        auto name_end=name_at;
        while(name_end<std::size_t(names_at)+h[2]&&b[name_end]!=0)++name_end;
        if(name_end==std::size_t(names_at)+h[2]){fail(f,RetailStringDataError::malformed_name,"terminated MKO export name");return {};}
        const auto type_relative=std::uint64_t(r[1])-1;
        if(type_relative+6>h[4]||b[std::size_t(types_at+type_relative+4)]!=1||
           b[std::size_t(types_at+type_relative+5)]!=4) {
            fail(f,RetailStringDataError::unsupported_type,"216D80 native one-field string primitive4");return {};
        }
        if((h[7]&3u)!=0||std::uint64_t(r[2])+r[3]>h[7]/4u) {
            fail(f,RetailStringDataError::malformed_reference,"216D80 variable cell range");return {};
        }
        auto result=std::shared_ptr<NativeRetailPermanentStringImage>(new NativeRetailPermanentStringImage);
        result->export_offset_=name_at;result->export_length_=name_end-name_at;
        result->values_.reserve(r[3]);
        for(std::uint32_t i=0;i<r[3];++i) {
            const auto reference=word(b,std::size_t(values_at)+(std::size_t(r[2])+i)*4);
            if(!reference){result->values_.push_back({0,0,true});continue;}
            if(reference>r[4]){fail(f,RetailStringDataError::malformed_reference,"216EB4 one-based string reference",i);return {};}
            const auto relative=word(b,record_at+20+std::size_t(reference-1)*4);
            if(relative>=h[3]){fail(f,RetailStringDataError::malformed_string,"216EC0 string-pool offset",i);return {};}
            const auto at=std::size_t(strings_at)+relative;
            auto end=at;
            while(end<std::size_t(strings_at)+h[3]&&b[end]!=0)++end;
            if(end==std::size_t(strings_at)+h[3]){fail(f,RetailStringDataError::malformed_string,"bounded MKO string terminator",i);return {};}
            result->values_.push_back({at,end-at,false});
        }
        result->storage_=std::make_shared<const std::vector<std::uint8_t>>(std::move(bytes));
        return result;
    } catch(const std::bad_alloc&){fail(f,RetailStringDataError::allocation,"native MKO string data owners");}
    catch(...){fail(f,RetailStringDataError::native_exception,"native MKO string data decode");}
    return {};
}
std::string_view NativeRetailPermanentStringImage::export_name() const & noexcept {
    return {reinterpret_cast<const char*>(storage_->data()+export_offset_),export_length_};
}
std::optional<NativeRetailStringValue> NativeRetailPermanentStringImage::value(
    std::uint64_t index,RetailStringDataFailure& f) const noexcept {
    f={};
    if(index>=values_.size()){fail(f,RetailStringDataError::invalid_index,"native string-list index");return {};}
    const auto& v=values_[std::size_t(index)];
    return NativeRetailStringValue(storage_,v.offset,v.length,v.null);
}
std::shared_ptr<NativeRetailLoadedResource> NativeRetailLoadedResource::prepare_permanent_strings(
    std::int32_t language,RetailStringDataFailure& f) noexcept {
    const auto d=permanent_string_descriptor_from_source(language,f);if(!d)return {};
    try{return std::shared_ptr<NativeRetailLoadedResource>(new NativeRetailLoadedResource(*d));}
    catch(const std::bad_alloc&){fail(f,RetailStringDataError::allocation,"215180 native queued resource record");}
    catch(...){fail(f,RetailStringDataError::native_exception,"215180 native queued resource record");}
    return {};
}
bool NativeRetailLoadedResource::access(RetailStringDataFailure& f) const noexcept {
    f={};
    if(std::this_thread::get_id()!=owner_thread_)return fail(f,RetailStringDataError::wrong_thread,"native loaded resource owner thread");
    if(phase_==RetailLoadedStringPhase::retired)return fail(f,RetailStringDataError::retired,"retired native string resource");
    const auto lifetime=main_.lock();
    if(!lifetime||!lifetime->active_on_owner_thread())return fail(f,RetailStringDataError::lifetime,"native string resource Main lifetime");
    if(phase_!=RetailLoadedStringPhase::ready||!image_)return fail(f,RetailStringDataError::not_ready,"native string resource not linked");
    return true;
}
std::optional<NativeRetailStringValue> NativeRetailLoadedResource::value(
    std::uint64_t index,RetailStringDataFailure& f) const noexcept {
    if(!access(f))return {};
    return image_->value(index,f);
}
} // namespace mka::retail
