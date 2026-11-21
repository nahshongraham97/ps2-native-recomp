#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <thread>
#include <vector>

namespace mka::retail {
struct NativeRetailMainState;
class NativeRetailGameSession;

enum class RetailProfileResourceIdsError {
    none, wrong_thread, retired_main, foreign_main, after_effect_unknown,
    secret_camera_effect,
    divergent_staging, allocation, native_exception
};
struct RetailProfileResourceIdsFailure {
    RetailProfileResourceIdsError kind{RetailProfileResourceIdsError::none};
    const char* operation{""};
};
enum class RetailProfileResourceIdOutcome {
    duplicate, appended, outside_byte_identity
};
struct RetailProfileResourceIdsObservation {
    std::vector<std::uint8_t> active, staging;
    std::size_t active_count{}, staging_count{};
    bool after_effect_unknown{};
};

// Required production effect: search both real actor-local +E8C observer
// lists and execute the selected secret-camera object's virtual +0xC cleanup.
// A true return also covers the source's legitimate no-matching-object case.
// There is deliberately no default implementation or optional callback.
bool retire_matching_retail_secret_camera_from_446f80(
    NativeRetailMainState&, std::uint32_t input_id,
    RetailProfileResourceIdsFailure&);

// Typed native active/staging byte IDs for the proved 446F80 path. The source
// 60-byte backing region is not a host storage ceiling. Active/staging transfer
// and divergent prefix birth belong to later source routines, not this owner.
class NativeRetailProfileResourceIds final {
public:
    NativeRetailProfileResourceIds(const NativeRetailProfileResourceIds&) = delete;
    NativeRetailProfileResourceIds& operator=(const NativeRetailProfileResourceIds&) = delete;

    std::optional<RetailProfileResourceIdOutcome> record_from_446f80(
        NativeRetailMainState&, std::uint32_t input_id,
        RetailProfileResourceIdsFailure&);
    std::optional<RetailProfileResourceIdsObservation> observe(
        RetailProfileResourceIdsFailure&) const noexcept;

private:
    friend class NativeRetailGameSession;
    explicit NativeRetailProfileResourceIds(NativeRetailGameSession& session) noexcept
        : session_(&session) {}
    bool access(RetailProfileResourceIdsFailure&) const noexcept;
    NativeRetailGameSession* session_;
    std::vector<std::uint8_t> active_, staging_;
    std::size_t active_count_{}, staging_count_{};
    bool after_effect_unknown_{};
    const std::thread::id owner_thread_{std::this_thread::get_id()};
};
} // namespace mka::retail
