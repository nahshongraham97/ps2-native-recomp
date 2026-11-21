#pragma once

#include "mka/retail_observer_list.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <thread>

namespace mka::retail {

enum class RetailInputRole : std::uint8_t;
enum class RetailInputTaskArgumentKind : std::uint32_t {
    task_2065 = 0x2065,
    task_2066 = 0x2066,
    task_208b = 0x208b
};
enum class RetailInputTaskArgumentError {
    none, wrong_thread, invalid_kind, invalid_role, invalid_cookie,
    incompatible_field, retired, allocation, native_exception
};
struct RetailInputTaskArgumentFailure {
    RetailInputTaskArgumentError kind{RetailInputTaskArgumentError::none};
    const char* operation{""};
};
struct RetailInputTaskArgumentObservation {
    RetailInputTaskArgumentKind kind;
    std::uint32_t source_cookie{};
    std::optional<RetailInputRole> role;
    std::optional<bool> restore_analog_inhibited;
    std::optional<std::uint32_t> source_control_word_208b;
    bool retired{};
};

// The typed source 59FE40 object, attached by source 12FEE0 to one task's
// owned +E8 observer list. The caller must provide its real 130D00 cookie;
// this object never invents a second cookie sequence or task body.
class NativeRetailInputTaskArgument final : public NativeRetailObservedObject,
    public std::enable_shared_from_this<NativeRetailInputTaskArgument> {
public:
    static std::shared_ptr<NativeRetailInputTaskArgument> create(
        RetailInputTaskArgumentKind, std::uint32_t source_cookie,
        RetailInputTaskArgumentFailure&) noexcept;
    NativeRetailInputTaskArgument(const NativeRetailInputTaskArgument&) = delete;
    NativeRetailInputTaskArgument& operator=(
        const NativeRetailInputTaskArgument&) = delete;

    bool store_role_from_source(RetailInputRole,
        RetailInputTaskArgumentFailure&) noexcept;
    bool store_restore_analog_inhibited_from_source(bool,
        RetailInputTaskArgumentFailure&) noexcept;
    bool store_control_word_for_208b_from_source(std::uint32_t,
        RetailInputTaskArgumentFailure&) noexcept;
    std::shared_ptr<NativeRetailInputTaskArgument> self_from_12d860(
        RetailInputTaskArgumentFailure&) noexcept;
    bool retire_from_12df60(RetailInputTaskArgumentFailure&) noexcept;
    std::optional<RetailInputTaskArgumentObservation> observe(
        RetailInputTaskArgumentFailure&) const noexcept;

    std::uint32_t source_cookie() const noexcept override;
    void cleanup_from_12fa00() override;

private:
    NativeRetailInputTaskArgument(RetailInputTaskArgumentKind,
        std::uint32_t) noexcept;
    bool access(RetailInputTaskArgumentFailure&) const noexcept;
    bool live_access(RetailInputTaskArgumentFailure&) const noexcept;

    const RetailInputTaskArgumentKind kind_;
    std::atomic<std::uint32_t> source_cookie_;
    std::optional<RetailInputRole> role_;
    std::optional<bool> restore_analog_inhibited_;
    std::optional<std::uint32_t> source_control_word_208b_;
    bool retired_{};
    const std::thread::id owner_thread_{std::this_thread::get_id()};
};

} // namespace mka::retail
