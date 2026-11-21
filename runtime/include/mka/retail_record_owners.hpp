#pragma once
#include "mka/retail_dispatch_owners.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace mka::retail {
class NativeRetailNamedRecordRequest;

enum class RetailRecordOwnerError {
    none, wrong_thread, invalid_name, invalid_request, foreign_ticket, stale_ticket,
    descriptor, allocation, host_capacity, native_exception
};
struct RetailRecordOwnerFailure {
    RetailRecordOwnerError kind{RetailRecordOwnerError::none};
    RetailDispatchOwnerFailure descriptor;
};

// Exact source key for the named record service. This is not a host file path.
std::optional<std::string> make_retail_named_record_key(
    std::string_view name, std::int32_t record, RetailRecordOwnerFailure&) noexcept;

class NativeRetailRecordTicket final {
public:
    NativeRetailRecordTicket() = default;
private:
    friend class NativeRetailRecordOwners;
    std::shared_ptr<const void> owner_, occurrence_;
    std::size_t workspace_{};
};

struct NativeRetailRecordObservation {
    std::size_t workspaces{}, occupied{};
};

// The fourth constructor's actual workspaces and two distinct prefix
// descriptors. No request producer, endpoint, callback or decoded profile
// record is created by this initialization.
class NativeRetailRecordOwners final {
public:
    static std::shared_ptr<NativeRetailRecordOwners> create(RetailRecordOwnerFailure&) noexcept;
    NativeRetailRecordOwners(const NativeRetailRecordOwners&) = delete;
    NativeRetailRecordOwners& operator=(const NativeRetailRecordOwners&) = delete;
    ~NativeRetailRecordOwners() noexcept;

    std::optional<NativeRetailRecordObservation> observe(RetailRecordOwnerFailure&) const noexcept;
    NativeRetailPrefixDispatchDescriptor& first_prefix_descriptor() noexcept;
    NativeRetailPrefixDispatchDescriptor& second_prefix_descriptor() noexcept;

    // Record actual owned operation backing. These methods manage structural
    // ownership only, alongside the real async producer and completion body.
    std::optional<NativeRetailRecordTicket> retain_request(
        std::shared_ptr<NativeRetailNamedRecordRequest>, RetailRecordOwnerFailure&) noexcept;
    bool release_after_callback_ack(const NativeRetailRecordTicket&, RetailRecordOwnerFailure&) noexcept;
private:
    struct Workspace {
        std::shared_ptr<NativeRetailNamedRecordRequest> request;
        std::shared_ptr<const void> occurrence;
    };
    NativeRetailRecordOwners();
    bool access(RetailRecordOwnerFailure&) const noexcept;

    const std::thread::id thread_{std::this_thread::get_id()};
    const std::shared_ptr<const void> identity_{std::make_shared<int>(0)};
    std::vector<Workspace> workspaces_;
    std::shared_ptr<NativeRetailPrefixDispatchDescriptor> first_descriptor_, second_descriptor_;
};

} // namespace mka::retail
