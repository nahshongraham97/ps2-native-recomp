#pragma once
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <vector>
namespace mka::retail {
enum class RetailDispatchOwnerError {none,unbound_callback,allocation,host_capacity,native_exception};
struct RetailDispatchOwnerFailure {RetailDispatchOwnerError kind=RetailDispatchOwnerError::none;};
// Concrete message framing/context and callback bodies remain required native
// consumers. These owners carry no guest address or inferred packet schema.
class NativeRetailDispatchInvocation {public:virtual ~NativeRetailDispatchInvocation()=default;};
class NativeRetailDispatchDescriptorData {public:virtual ~NativeRetailDispatchDescriptorData()=default;};
class NativeRetailPrefixCallback {
public:
    virtual ~NativeRetailPrefixCallback()=default;
    // Invocation must carry the actual caller context, decoded view and caller
    // parameter4. Copied descriptor+14 metadata is not a proved callback arg.
    virtual void invoke(const NativeRetailDispatchInvocation&)=0;
};
class NativeRetailPrefixDispatchDescriptor;
class NativeRetailPrefixBinding final {
public:
    NativeRetailPrefixBinding(const NativeRetailPrefixBinding&)=default;
    NativeRetailPrefixBinding(NativeRetailPrefixBinding&&) noexcept=default;
    NativeRetailPrefixBinding& operator=(const NativeRetailPrefixBinding&);
    NativeRetailPrefixBinding& operator=(NativeRetailPrefixBinding&&) noexcept;
    std::span<const std::byte> prefix_bytes()const & noexcept{return prefix_;}
    std::span<const std::byte> prefix_bytes()const && =delete;
    std::shared_ptr<NativeRetailPrefixCallback> callback()const noexcept{return callback_;}
    std::shared_ptr<const NativeRetailDispatchDescriptorData> copied_descriptor_data()const noexcept{return descriptor_data_;}
    // Only the proven prefix comparison leaf; does not parse/route a message.
    bool matches_prefix(std::span<const std::byte>)const noexcept;
private:
    NativeRetailPrefixBinding(std::shared_ptr<NativeRetailPrefixCallback>,
        std::vector<std::byte>,std::shared_ptr<const NativeRetailDispatchDescriptorData>) noexcept;
    std::shared_ptr<NativeRetailPrefixCallback> callback_;
    std::vector<std::byte> prefix_;
    std::shared_ptr<const NativeRetailDispatchDescriptorData> descriptor_data_;
    void swap(NativeRetailPrefixBinding&) noexcept;
    friend class NativeRetailPrefixDispatchDescriptor;
};
// Source-static351390 type with CRT-zeroed, initially unbound descriptor data.
// Main-thread ownership; a snapshot owns copies independent of reconfiguration.
// Optional descriptor data is copied metadata with no proved dispatch consumer.
class NativeRetailPrefixDispatchDescriptor final {
public:
    NativeRetailPrefixDispatchDescriptor(const NativeRetailPrefixDispatchDescriptor&)=delete;
    NativeRetailPrefixDispatchDescriptor& operator=(const NativeRetailPrefixDispatchDescriptor&)=delete;
    static std::shared_ptr<NativeRetailPrefixDispatchDescriptor> create(RetailDispatchOwnerFailure&) noexcept;
    bool configure(std::shared_ptr<NativeRetailPrefixCallback>,std::span<const std::byte>,
        std::shared_ptr<const NativeRetailDispatchDescriptorData>,RetailDispatchOwnerFailure&) noexcept;
    std::optional<NativeRetailPrefixBinding> snapshot(RetailDispatchOwnerFailure&)const noexcept;
    bool configured()const noexcept{return bool(callback_);}
    // Actual351D70 has no observable effect and no defined return value.
    void source_default_operation()const noexcept;
private:
    NativeRetailPrefixDispatchDescriptor()=default;
    std::shared_ptr<NativeRetailPrefixCallback> callback_;
    std::vector<std::byte> prefix_;
    std::shared_ptr<const NativeRetailDispatchDescriptorData> descriptor_data_;
};
} // namespace mka::retail
