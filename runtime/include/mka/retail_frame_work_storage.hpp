#pragma once
#include "mka/retail_main_lifetime.hpp"
#include "mka/retail_renderer_allocator.hpp"
#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>

namespace mka::retail {
enum class RetailFrameWorkError {
    none, missing_owner, wrong_thread, retired_main, invalid_category, pending,
    stale_reservation, foreign_reservation, invalid_commit, host_capacity,
    allocator, stale_allocation, allocation, native_exception
};
struct RetailFrameWorkFailure {
    RetailFrameWorkError kind{RetailFrameWorkError::none};
    const char* operation{""};
    RetailRendererAllocatorFailure allocator;
};
namespace detail {
struct RetailFrameWorkContext;
struct RetailFrameWorkBlock;
struct RetailFrameWorkRequest;
}
// A committed native data lease, not an original arena address. The actual
// backing remains owned while a native consumer retains this value.
class NativeRetailFrameWorkData final {
public:
    std::optional<std::span<const std::byte>> bytes(RetailFrameWorkFailure&) const & noexcept;
    std::optional<std::span<const std::byte>> bytes(RetailFrameWorkFailure&) const &&=delete;
    std::size_t size() const noexcept{return size_;}
    std::size_t aligned_size() const noexcept{return aligned_;}
private:
    friend class NativeRetailFrameWorkStorage;
    NativeRetailFrameWorkData(std::shared_ptr<detail::RetailFrameWorkBlock>,std::size_t,std::size_t) noexcept;
    std::shared_ptr<detail::RetailFrameWorkBlock> block_;
    std::size_t size_{},aligned_{};
};
class NativeRetailFrameWorkReservation final {
public:
    NativeRetailFrameWorkReservation(NativeRetailFrameWorkReservation&&) noexcept=default;
    NativeRetailFrameWorkReservation& operator=(NativeRetailFrameWorkReservation&&) noexcept=default;
    NativeRetailFrameWorkReservation(const NativeRetailFrameWorkReservation&)=delete;
    NativeRetailFrameWorkReservation& operator=(const NativeRetailFrameWorkReservation&)=delete;
    std::optional<std::span<std::byte>> writable_bytes(RetailFrameWorkFailure&) & noexcept;
    std::size_t size() const noexcept;
private:
    friend class NativeRetailFrameWorkStorage;
    explicit NativeRetailFrameWorkReservation(std::shared_ptr<detail::RetailFrameWorkRequest>) noexcept;
    std::shared_ptr<detail::RetailFrameWorkRequest> request_;
};
struct RetailFrameWorkCategoryObservation {
    std::size_t native_blocks{},retained_blocks{};
    bool pending{};
};
struct RetailFrameWorkObservation {
    memory::NativeHeapIdentity heap;
    std::array<RetailFrameWorkCategoryObservation,2> categories;
};
// Native destination of2E9930->2E8E10's work-storage initialization.
// Two logical work categories remain, but no fixed addresses, byte ceiling,
// rotating PS2 windows, register image or graphics device is created.
class NativeRetailFrameWorkStorage final {
public:
    static std::unique_ptr<NativeRetailFrameWorkStorage> create(
        std::shared_ptr<NativeRetailMainLifetime>,
        std::shared_ptr<NativeRetailRendererAllocatorService>,RetailFrameWorkFailure&) noexcept;
    ~NativeRetailFrameWorkStorage();
    NativeRetailFrameWorkStorage(const NativeRetailFrameWorkStorage&)=delete;
    NativeRetailFrameWorkStorage& operator=(const NativeRetailFrameWorkStorage&)=delete;
    bool same_heap(const NativeRetailRendererAllocatorService&,RetailFrameWorkFailure&) const noexcept;
    bool reset_for_initialization(RetailFrameWorkFailure&) noexcept;
    std::optional<NativeRetailFrameWorkReservation> reserve(
        std::size_t category,std::size_t bytes,RetailFrameWorkFailure&) noexcept;
    std::optional<NativeRetailFrameWorkData> commit(
        NativeRetailFrameWorkReservation&,std::size_t written,RetailFrameWorkFailure&) noexcept;
    std::optional<RetailFrameWorkObservation> observe(RetailFrameWorkFailure&) const noexcept;
private:
    explicit NativeRetailFrameWorkStorage(std::shared_ptr<detail::RetailFrameWorkContext>) noexcept;
    std::shared_ptr<detail::RetailFrameWorkContext> context_;
};
} // namespace mka::retail
