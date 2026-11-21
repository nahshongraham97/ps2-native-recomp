#include "mka/retail_record_owners.hpp"
#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"
#include <algorithm>
#include <charconv>
#include <cstdio>
#include <exception>
#include <new>
#include <stdexcept>
#include <utility>

namespace mka::retail {
namespace {
constexpr std::size_t initial_workspace_count = 30;

void report_exception(RetailRecordOwnerFailure& failure) noexcept {
    try { throw; }
    catch (const std::bad_alloc&) { failure.kind = RetailRecordOwnerError::allocation; }
    catch (const std::length_error&) { failure.kind = RetailRecordOwnerError::host_capacity; }
    catch (...) { failure.kind = RetailRecordOwnerError::native_exception; }
}
} // namespace

std::optional<std::string> make_retail_named_record_key(
    std::string_view name, std::int32_t record, RetailRecordOwnerFailure& failure) noexcept {
    failure = {};
    if (name.find('\0') != std::string_view::npos) {
        failure.kind = RetailRecordOwnerError::invalid_name;
        return {};
    }
    try {
        char decimal[12];
        const auto digits = std::to_chars(decimal, decimal + sizeof(decimal), record);
        std::string key = "/n/";
        key.append(name);
        key.append("/r/");
        key.append(decimal, static_cast<std::size_t>(digits.ptr - decimal));
        return key;
    } catch (...) {
        report_exception(failure);
        return {};
    }
}

NativeRetailRecordOwners::NativeRetailRecordOwners()
    : workspaces_(initial_workspace_count) {}

std::shared_ptr<NativeRetailRecordOwners> NativeRetailRecordOwners::create(
    RetailRecordOwnerFailure& failure) noexcept {
    failure = {};
    try {
        auto owner = std::shared_ptr<NativeRetailRecordOwners>(new NativeRetailRecordOwners);
        owner->first_descriptor_ = NativeRetailPrefixDispatchDescriptor::create(failure.descriptor);
        if (!owner->first_descriptor_) {
            failure.kind = RetailRecordOwnerError::descriptor;
            return {};
        }
        owner->second_descriptor_ = NativeRetailPrefixDispatchDescriptor::create(failure.descriptor);
        if (!owner->second_descriptor_) {
            failure.kind = RetailRecordOwnerError::descriptor;
            return {};
        }
        return owner;
    } catch (...) {
        report_exception(failure);
        return {};
    }
}

NativeRetailRecordOwners::~NativeRetailRecordOwners() noexcept {
    if (std::any_of(workspaces_.begin(), workspaces_.end(), [](const auto& slot) { return bool(slot.request); })) {
        std::fputs("Native retail record owners destroyed with pending requests\n", stderr);
        std::terminate();
    }
}

bool NativeRetailRecordOwners::access(RetailRecordOwnerFailure& failure) const noexcept {
    failure = {};
    if (std::this_thread::get_id() == thread_) return true;
    failure.kind = RetailRecordOwnerError::wrong_thread;
    return false;
}

std::optional<NativeRetailRecordObservation> NativeRetailRecordOwners::observe(
    RetailRecordOwnerFailure& failure) const noexcept {
    if (!access(failure)) return {};
    const auto occupied = std::count_if(workspaces_.begin(), workspaces_.end(),
        [](const auto& slot) { return bool(slot.request); });
    return NativeRetailRecordObservation{workspaces_.size(), static_cast<std::size_t>(occupied)};
}

NativeRetailPrefixDispatchDescriptor&
NativeRetailRecordOwners::first_prefix_descriptor() noexcept { return *first_descriptor_; }

NativeRetailPrefixDispatchDescriptor&
NativeRetailRecordOwners::second_prefix_descriptor() noexcept { return *second_descriptor_; }

std::optional<NativeRetailRecordTicket> NativeRetailRecordOwners::retain_request(
    std::shared_ptr<NativeRetailNamedRecordRequest> request, RetailRecordOwnerFailure& failure) noexcept {
    if (!access(failure)) return {};
    if (!request || std::any_of(workspaces_.begin(), workspaces_.end(),
        [&](const auto& slot) { return slot.request == request; })) {
        failure.kind = RetailRecordOwnerError::invalid_request;
        return {};
    }
    try {
        auto occurrence = std::make_shared<int>(0);
        const auto vacant = std::find_if(workspaces_.begin(), workspaces_.end(),
            [](const auto& slot) { return !slot.request; });
        const auto index = static_cast<std::size_t>(vacant - workspaces_.begin());
        if (vacant == workspaces_.end()) workspaces_.emplace_back();
        auto& slot = workspaces_[index];
        slot = {std::move(request), occurrence};
        NativeRetailRecordTicket ticket;
        ticket.owner_ = identity_;
        ticket.occurrence_ = std::move(occurrence);
        ticket.workspace_ = index;
        return ticket;
    } catch (...) {
        report_exception(failure);
        return {};
    }
}

bool NativeRetailRecordOwners::release_after_callback_ack(
    const NativeRetailRecordTicket& ticket, RetailRecordOwnerFailure& failure) noexcept {
    if (!access(failure)) return false;
    if (ticket.owner_ != identity_) {
        failure.kind = RetailRecordOwnerError::foreign_ticket;
        return false;
    }
    if (ticket.workspace_ >= workspaces_.size()) {
        failure.kind = RetailRecordOwnerError::stale_ticket;
        return false;
    }
    auto& slot = workspaces_[ticket.workspace_];
    if (!slot.request || slot.occurrence != ticket.occurrence_) {
        failure.kind = RetailRecordOwnerError::stale_ticket;
        return false;
    }
    auto retired = std::move(slot.request);
    slot.occurrence.reset();
    return true;
}

namespace main_binding {
bool construct_retail_record_owners(NativeRetailMainState& main, StartupFailure& failure) {
    failure = {};
    if (main.record_owners) {
        failure = {StartupError::already_initialized, "retail named record owners already initialized"};
        return false;
    }
    RetailRecordOwnerFailure native;
    auto owners = NativeRetailRecordOwners::create(native);
    if (!owners) {
        const bool allocation_failed = native.kind == RetailRecordOwnerError::allocation ||
            (native.kind == RetailRecordOwnerError::descriptor &&
                native.descriptor.kind == RetailDispatchOwnerError::allocation);
        failure.kind = allocation_failed ? StartupError::allocation : StartupError::native_exception;
        failure.operation = "retail named record owner construction";
        return false;
    }
    main.record_owners = std::move(owners);
    return true;
}
} // namespace main_binding
} // namespace mka::retail
