#include "mka/retail_dispatch_owners.hpp"
#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"
#include <algorithm>
#include <new>
#include <stdexcept>
#include <utility>
namespace mka::retail {
NativeRetailPrefixBinding::NativeRetailPrefixBinding(std::shared_ptr<NativeRetailPrefixCallback> callback,
    std::vector<std::byte> prefix,std::shared_ptr<const NativeRetailDispatchDescriptorData> metadata)noexcept
    :callback_(std::move(callback)),prefix_(std::move(prefix)),descriptor_data_(std::move(metadata)){}
void NativeRetailPrefixBinding::swap(NativeRetailPrefixBinding& other)noexcept{
    callback_.swap(other.callback_);prefix_.swap(other.prefix_);descriptor_data_.swap(other.descriptor_data_);
}
NativeRetailPrefixBinding& NativeRetailPrefixBinding::operator=(const NativeRetailPrefixBinding& other){
    if(this!=&other){NativeRetailPrefixBinding candidate(other);swap(candidate);}return *this;
}
NativeRetailPrefixBinding& NativeRetailPrefixBinding::operator=(NativeRetailPrefixBinding&& other)noexcept{
    if(this!=&other){NativeRetailPrefixBinding candidate(std::move(other));swap(candidate);}return *this;
}
bool NativeRetailPrefixBinding::matches_prefix(std::span<const std::byte> input)const noexcept{
    return callback_&&input.size()>=prefix_.size()&&std::equal(prefix_.begin(),prefix_.end(),input.begin());
}
std::shared_ptr<NativeRetailPrefixDispatchDescriptor> NativeRetailPrefixDispatchDescriptor::create(RetailDispatchOwnerFailure& f)noexcept{
    f={};try{return std::shared_ptr<NativeRetailPrefixDispatchDescriptor>(new NativeRetailPrefixDispatchDescriptor());}
    catch(const std::bad_alloc&){f.kind=RetailDispatchOwnerError::allocation;}
    catch(...){f.kind=RetailDispatchOwnerError::native_exception;}return {};
}
bool NativeRetailPrefixDispatchDescriptor::configure(std::shared_ptr<NativeRetailPrefixCallback> callback,
    std::span<const std::byte> prefix,std::shared_ptr<const NativeRetailDispatchDescriptorData> metadata,RetailDispatchOwnerFailure& f)noexcept{
    f={};if(!callback){f.kind=RetailDispatchOwnerError::unbound_callback;return false;}
    try{
        // Complete all fallible copying before replacing any actual binding.
        std::vector<std::byte> copied(prefix.begin(),prefix.end());
        callback_.swap(callback);prefix_.swap(copied);descriptor_data_.swap(metadata);
        return true;
    }catch(const std::length_error&){f.kind=RetailDispatchOwnerError::host_capacity;}
    catch(const std::bad_alloc&){f.kind=RetailDispatchOwnerError::allocation;}
    catch(...){f.kind=RetailDispatchOwnerError::native_exception;}return false;
}
std::optional<NativeRetailPrefixBinding> NativeRetailPrefixDispatchDescriptor::snapshot(RetailDispatchOwnerFailure& f)const noexcept{
    f={};if(!callback_){f.kind=RetailDispatchOwnerError::unbound_callback;return {};}
    try{return NativeRetailPrefixBinding(callback_,prefix_,descriptor_data_);}
    catch(const std::length_error&){f.kind=RetailDispatchOwnerError::host_capacity;}
    catch(const std::bad_alloc&){f.kind=RetailDispatchOwnerError::allocation;}
    catch(...){f.kind=RetailDispatchOwnerError::native_exception;}return {};
}
void NativeRetailPrefixDispatchDescriptor::source_default_operation()const noexcept{}
namespace main_binding {
bool construct_retail_dispatch_object(NativeRetailMainState& main,StartupFailure& f){
    f={};if(main.dispatch_object){f.kind=StartupError::already_initialized;f.operation="retail prefix descriptor exists";return false;}
    RetailDispatchOwnerFailure failure;auto object=NativeRetailPrefixDispatchDescriptor::create(failure);
    if(!object){f.kind=failure.kind==RetailDispatchOwnerError::allocation?StartupError::allocation:StartupError::native_exception;f.operation="retail prefix descriptor construction";return false;}
    main.dispatch_object=std::move(object);return true;
}
} // namespace main_binding
} // namespace mka::retail
