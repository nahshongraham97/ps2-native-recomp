#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <thread>

namespace mka::retail {

enum class RetailParticipantError {
    none, wrong_thread, invalid_identity, allocation, native_exception
};
struct RetailParticipantFailure {
    RetailParticipantError kind{RetailParticipantError::none};
    const char* operation{""};
};

// The original first two participant records are game identities. Their
// identity is separate from a physical sample route and from a role record.
struct RetailParticipantObservation {
    std::int32_t assigned_role{-1};
    std::int32_t ordinal{5};
    std::uint32_t game_state{}; // 1319E0 writes this separately at source +8.
    // The later gameplay meaning of these source values is not yet joined.
    std::uint32_t source_float_0c_bits{0x3f800000};
    std::array<std::uint32_t, 4> initial_game_words{};
    std::uint32_t source_word_2c{0x40};
    bool has_fighter{};
    std::array<std::uint32_t, 4> linked_game_words{};
    std::uint32_t source_word_44{3};
};

class NativeRetailParticipants;
class NativeRetailInputAssignments;
class NativeRetailInputRoles;
class NativeRetailInputParticipant final {
public:
    NativeRetailInputParticipant(const NativeRetailInputParticipant&) = delete;
    NativeRetailInputParticipant& operator=(const NativeRetailInputParticipant&) = delete;
    std::optional<RetailParticipantObservation> observe(RetailParticipantFailure&) const noexcept;
    // Exact 1319E0 effect. The 137810 constructor deliberately leaves this
    // state untouched on a later menu reconstruction.
    bool set_game_state(std::uint32_t, RetailParticipantFailure&) noexcept;
private:
    friend class NativeRetailParticipants;
    friend class NativeRetailInputAssignments;
    explicit NativeRetailInputParticipant(std::uint32_t identity) noexcept;
    bool reinitialize(RetailParticipantFailure&) noexcept;
    bool access(RetailParticipantFailure&) const noexcept;

    std::uint32_t identity_{};
    RetailParticipantObservation state_{};
    const std::thread::id owner_thread_{std::this_thread::get_id()};
};

// Born at the two 137810 calls in 1303F0. The mode-indexed third/fourth
// source records are not constructed by that startup sequence.
class NativeRetailParticipants final {
public:
    static std::shared_ptr<NativeRetailParticipants> create_first_two(
        RetailParticipantFailure&) noexcept;
    NativeRetailParticipants(const NativeRetailParticipants&) = delete;
    NativeRetailParticipants& operator=(const NativeRetailParticipants&) = delete;

    std::shared_ptr<NativeRetailInputParticipant> first_two(
        std::uint32_t identity, RetailParticipantFailure&) const noexcept;
    // Caller must perform the original 280F90 detach and 1319E0 state writes
    // before the menu's two consecutive 137810 reconstructions.
    bool reconstruct_first_two_after_detach(RetailParticipantFailure&) noexcept;
private:
    friend class NativeRetailInputAssignments;
    NativeRetailParticipants() noexcept = default;
    bool access(RetailParticipantFailure&) const noexcept;
    std::array<std::shared_ptr<NativeRetailInputParticipant>, 2> first_two_{};
    std::weak_ptr<NativeRetailInputRoles> assignment_domain_;
    bool assignment_domain_bound_{};
    const std::thread::id owner_thread_{std::this_thread::get_id()};
};

} // namespace mka::retail
