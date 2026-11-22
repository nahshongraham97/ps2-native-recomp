#pragma once
#include "mka/retail_game_resource_registry.hpp"
#include "mka/retail_package.hpp"

namespace mka::retail {
struct NativeRetailMainState;
struct StartupFailure;
enum class RetailPermanentStringError {
    none, main_lifetime, registry, package, read, data, allocation,
    native_exception, foreign_owner, invalid_state
};
struct RetailPermanentStringFailure {
    RetailPermanentStringError kind{RetailPermanentStringError::none};
    const char* operation{""};
    RetailStringDataFailure data;
    RetailGameResourceRegistryFailure registry;
    RetailPackageFailure package;
    content::NativeFileReadFailure read;
};
enum class RetailPermanentStringStep { pending, complete };
struct RetailPermanentStringObservation {
    bool record_published{},job_retained{},ready{},failed{};
    std::string_view source_name;
    std::optional<RetailGameResourceRecordHandle> record;
};

// Real Main/registry-owned215180/215120 producer. Pending file buffers survive
// errors and cancellation until terminal observation. No parsed-success stub.
class NativeRetailPermanentStrings final {
public:
    static std::unique_ptr<NativeRetailPermanentStrings> create(
        NativeRetailMainState&,RetailPermanentStringFailure&) noexcept;
    ~NativeRetailPermanentStrings();
    NativeRetailPermanentStrings(const NativeRetailPermanentStrings&)=delete;
    NativeRetailPermanentStrings& operator=(const NativeRetailPermanentStrings&)=delete;
    bool queue_from_215660(std::int32_t,RetailPermanentStringFailure&) noexcept;
    std::optional<RetailPermanentStringStep> advance_from_215730(RetailPermanentStringFailure&) noexcept;
    std::optional<RetailPermanentStringStep> shutdown(RetailPermanentStringFailure&) noexcept;
    std::optional<RetailPermanentStringObservation> observe(RetailPermanentStringFailure&) const noexcept;
private:
    explicit NativeRetailPermanentStrings(NativeRetailMainState&) noexcept;
    bool access(RetailPermanentStringFailure&) const noexcept;
    void seal(RetailPermanentStringFailure&) noexcept;
    NativeRetailMainState* main_;
    std::weak_ptr<NativeRetailMainLifetime> lifetime_;
    NativeRetailGameResourceRegistry* registry_;
    std::shared_ptr<NativeRetailLoadedResource> module_;
    std::optional<RetailGameResourceRecordHandle> record_;
    std::unique_ptr<content::NativeFileReadJob> job_;
    bool failed_{},retiring_{};
    RetailPermanentStringError failed_kind_{};
    const char* failed_operation_{""};
    const std::thread::id owner_thread_{std::this_thread::get_id()};
};
} // namespace mka::retail
