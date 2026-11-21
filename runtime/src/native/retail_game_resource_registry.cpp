#include "mka/retail_game_resource_registry.hpp"

#include <atomic>
#include <limits>
#include <new>
#include <utility>

namespace mka::retail {
namespace {
std::atomic<std::uint64_t> next_registry_identity{1};

bool fail(RetailGameResourceRegistryFailure& f,
          RetailGameResourceRegistryError kind,const char* operation) noexcept {
    f.kind=kind;
    f.operation=operation;
    return false;
}
std::uint64_t identity() noexcept {
    auto value=next_registry_identity.load(std::memory_order_relaxed);
    while(value!=std::numeric_limits<std::uint64_t>::max()) {
        if(next_registry_identity.compare_exchange_weak(value,value+1,
                std::memory_order_relaxed)) return value;
    }
    return 0;
}
std::uint8_t fold_ascii(std::uint8_t byte) noexcept {
    return byte>='A' && byte<='Z' ? static_cast<std::uint8_t>(byte+0x20) : byte;
}
bool equal_source(std::string_view a,std::string_view b) noexcept {
    if(a.size()!=b.size()) return false;
    for(std::size_t i=0;i<a.size();++i)
        if(fold_ascii(static_cast<std::uint8_t>(a[i]))!=
           fold_ascii(static_cast<std::uint8_t>(b[i]))) return false;
    return true;
}
bool has_value(const NativeRetailGameResourceValue& value) noexcept {
    return std::visit([](const auto& pointer) { return bool(pointer); },value);
}
}

std::string_view NativeRetailGameResourceRegistry::c_prefix(
    std::string_view name) noexcept {
    const auto end=name.find('\0');
    return end==std::string_view::npos ? name : name.substr(0,end);
}

std::uint32_t NativeRetailGameResourceRegistry::source_hash(
    std::string_view name) noexcept {
    std::uint32_t hash=0;
    for(const auto character:c_prefix(name)) {
        const auto byte=static_cast<std::uint8_t>(character);
        // The source loads a signed byte, then its ctype table folds only
        // ASCII uppercase. Preserve the negative contribution of high bytes.
        const std::int32_t contribution=byte>127 ?
            static_cast<std::int32_t>(byte)-256 :
            static_cast<std::int32_t>(fold_ascii(byte));
        hash=(hash<<4)+static_cast<std::uint32_t>(contribution);
        const std::uint32_t high=hash&0xf0000000u;
        if(high) {
            const std::uint32_t shifted=(high>>24) |
                ((high&0x80000000u) ? 0xffffff00u : 0u);
            hash^=shifted^high;
        }
    }
    return hash;
}

std::unique_ptr<NativeRetailGameResourceRegistry>
NativeRetailGameResourceRegistry::create(
    RetailGameResourceRegistryFailure& f) noexcept {
    f={};
    const auto id=identity();
    if(!id) {
        fail(f,RetailGameResourceRegistryError::identity_exhausted,
             "2173D0 native registry identity");
        return {};
    }
    try {
        auto owner=std::unique_ptr<NativeRetailGameResourceRegistry>(
            new NativeRetailGameResourceRegistry(id));
        owner->records_.resize(20);
        for(std::size_t i=0;i<owner->records_.size();++i)
            owner->records_[i].next=i+1<owner->records_.size() ? i+1 : no_slot;
        owner->free_head_=0;
        return owner;
    } catch(const std::bad_alloc&) {
        fail(f,RetailGameResourceRegistryError::allocation,
             "2173D0 initial resource records");
    } catch(...) {
        fail(f,RetailGameResourceRegistryError::native_exception,
             "2173D0 registry construction");
    }
    return {};
}

bool NativeRetailGameResourceRegistry::access(
    RetailGameResourceRegistryFailure& f) const noexcept {
    f={};
    return std::this_thread::get_id()==owner_thread_ ||
        fail(f,RetailGameResourceRegistryError::wrong_thread,
             "game resource registry owner thread");
}

bool NativeRetailGameResourceRegistry::insert_global(
    std::optional<std::string_view> name,NativeRetailGameResourceValue value,
    RetailGameResourceRegistryFailure& f) noexcept {
    if(!access(f)) return false;
    if(!name) return true; // Source 2404A0 ignores a null C-string key.
    if(!has_value(value))
        return fail(f,RetailGameResourceRegistryError::missing_value,
                    "2404A0 actual typed resource value");
    const auto key=c_prefix(*name);
    const auto hash=source_hash(key);
    auto& chain=buckets_[hash%23u];
    for(auto& entry:chain) {
        if(equal_source(key,entry.original_name)) {
            entry.value=std::move(value); // Keep source first spelling/position.
            return true;
        }
    }
    try {
        chain.push_front(GlobalEntry{std::string(key),std::move(value),hash});
        ++global_count_;
        return true;
    } catch(const std::bad_alloc&) {
        return fail(f,RetailGameResourceRegistryError::allocation,
                    "2404A0 owned name/entry insertion");
    } catch(...) {
        return fail(f,RetailGameResourceRegistryError::native_exception,
                    "2404A0 name insertion");
    }
}

std::optional<RetailGameResourceGlobalObservation>
NativeRetailGameResourceRegistry::lookup_global(
    std::optional<std::string_view> name,
    RetailGameResourceRegistryFailure& f) const noexcept {
    if(!access(f) || !name) return {};
    const auto key=c_prefix(*name);
    const auto hash=source_hash(key);
    try {
        for(const auto& entry:buckets_[hash%23u])
            if(equal_source(key,entry.original_name))
                return RetailGameResourceGlobalObservation{
                    entry.original_name,entry.hash,hash%23u,entry.value};
    } catch(const std::bad_alloc&) {
        fail(f,RetailGameResourceRegistryError::allocation,
             "240370 global name result");
    } catch(...) {
        fail(f,RetailGameResourceRegistryError::native_exception,
             "240370 global name lookup");
    }
    return {};
}

std::optional<RetailGameResourceRecordHandle>
NativeRetailGameResourceRegistry::activate_loaded_record(
    std::shared_ptr<const NativeRetailLoadedResource> resource,
    RetailGameResourceRegistryFailure& f) noexcept {
    if(!access(f)) return {};
    if(!resource) {
        fail(f,RetailGameResourceRegistryError::missing_value,
             "214120/215180 actual loaded resource");
        return {};
    }
    try {
        if(free_head_==no_slot) {
            // The original twenty entries establish initial ordering. Host
            // storage may grow; no guest fixed-pool bound becomes a host cap.
            if(records_.size()==no_slot) {
                fail(f,RetailGameResourceRegistryError::identity_exhausted,
                     "native resource record index");
                return {};
            }
            records_.emplace_back();
            records_.back().next = no_slot;
            free_head_=records_.size()-1;
        }
        const auto index=free_head_;
        auto& slot=records_[index];
        if(slot.generation==std::numeric_limits<std::uint64_t>::max()) {
            fail(f,RetailGameResourceRegistryError::identity_exhausted,
                 "native resource record generation");
            return {};
        }
        free_head_=slot.next;
        slot.next=active_head_;
        slot.resource=std::move(resource);
        slot.active=true;
        ++slot.generation;
        active_head_=index;
        return RetailGameResourceRecordHandle{identity_,slot.generation,index};
    } catch(const std::bad_alloc&) {
        fail(f,RetailGameResourceRegistryError::allocation,
             "native resource record growth");
    } catch(...) {
        fail(f,RetailGameResourceRegistryError::native_exception,
             "native resource record activation");
    }
    return {};
}

bool NativeRetailGameResourceRegistry::valid_record(
    RetailGameResourceRecordHandle handle,
    RetailGameResourceRegistryFailure& f) const noexcept {
    if(handle.registry_identity!=identity_)
        return fail(f,RetailGameResourceRegistryError::foreign_record,
                    "resource record registry identity");
    if(handle.slot>=records_.size() || !records_[handle.slot].active ||
       records_[handle.slot].generation!=handle.generation)
        return fail(f,RetailGameResourceRegistryError::stale_record,
                    "resource record active generation");
    return true;
}

bool NativeRetailGameResourceRegistry::retire_loaded_record(
    RetailGameResourceRecordHandle handle,
    RetailGameResourceRegistryFailure& f) noexcept {
    if(!access(f) || !valid_record(handle,f)) return false;
    auto* link=&active_head_;
    while(*link!=no_slot && *link!=handle.slot)
        link=&records_[*link].next;
    if(*link==no_slot)
        return fail(f,RetailGameResourceRegistryError::native_exception,
                    "217FC0 active record link missing");
    auto& slot=records_[handle.slot];
    *link=slot.next;
    slot.resource.reset();
    slot.active=false;
    slot.next=free_head_;
    free_head_=handle.slot;
    return true;
}

std::shared_ptr<const NativeRetailLoadedResource>
NativeRetailGameResourceRegistry::retained_loaded_record(
    RetailGameResourceRecordHandle handle,
    RetailGameResourceRegistryFailure& f) const noexcept {
    if(!access(f) || !valid_record(handle,f)) return {};
    return records_[handle.slot].resource;
}

std::optional<RetailGameResourceRegistryObservation>
NativeRetailGameResourceRegistry::observe(
    RetailGameResourceRegistryFailure& f) const noexcept {
    if(!access(f)) return {};
    try {
        RetailGameResourceRegistryObservation result;
        result.global_names=global_count_;
        result.record_slots=records_.size();
        for(auto index=free_head_;index!=no_slot;index=records_[index].next)
            result.free_order.push_back(index);
        for(auto index=active_head_;index!=no_slot;index=records_[index].next)
            result.active_order.push_back(index);
        return result;
    } catch(const std::bad_alloc&) {
        fail(f,RetailGameResourceRegistryError::allocation,
             "native resource registry observation");
    } catch(...) {
        fail(f,RetailGameResourceRegistryError::native_exception,
             "native resource registry observation");
    }
    return {};
}
} // namespace mka::retail
