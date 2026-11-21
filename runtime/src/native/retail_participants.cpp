#include "mka/retail_participants.hpp"

#include <new>
#include <utility>

namespace mka::retail {
namespace {
bool fail(RetailParticipantFailure& failure, RetailParticipantError kind,
    const char* operation) noexcept {
    failure = {kind, operation};
    return false;
}
} // namespace

NativeRetailInputParticipant::NativeRetailInputParticipant(
    std::uint32_t identity) noexcept : identity_(identity) {}

bool NativeRetailInputParticipant::access(RetailParticipantFailure& failure) const noexcept {
    failure = {};
    return std::this_thread::get_id() == owner_thread_ ||
        fail(failure, RetailParticipantError::wrong_thread, "retail participant thread");
}

bool NativeRetailInputParticipant::reinitialize(RetailParticipantFailure& failure) noexcept {
    if (!access(failure)) return false;
    // 137810 writes the association, ordinal, float bits, two zero groups,
    // source +0x2c/+0x44 values, and absent fighter. It does not write +8.
    const auto game_state = state_.game_state;
    state_ = {};
    state_.game_state = game_state;
    return true;
}

std::optional<RetailParticipantObservation> NativeRetailInputParticipant::observe(
    RetailParticipantFailure& failure) const noexcept {
    if (!access(failure)) return {};
    return state_;
}

bool NativeRetailInputParticipant::set_game_state(
    std::uint32_t value, RetailParticipantFailure& failure) noexcept {
    if (!access(failure)) return false;
    state_.game_state = value;
    return true;
}

bool NativeRetailParticipants::access(RetailParticipantFailure& failure) const noexcept {
    failure = {};
    return std::this_thread::get_id() == owner_thread_ ||
        fail(failure, RetailParticipantError::wrong_thread, "retail participant collection thread");
}

std::shared_ptr<NativeRetailParticipants> NativeRetailParticipants::create_first_two(
    RetailParticipantFailure& failure) noexcept {
    failure = {};
    try {
        auto owner = std::shared_ptr<NativeRetailParticipants>(new NativeRetailParticipants);
        for (std::uint32_t identity = 0; identity != owner->first_two_.size(); ++identity) {
            auto participant = std::shared_ptr<NativeRetailInputParticipant>(
                new NativeRetailInputParticipant(identity));
            if (!participant->reinitialize(failure)) return {};
            owner->first_two_[identity] = std::move(participant);
        }
        return owner;
    } catch (const std::bad_alloc&) {
        fail(failure, RetailParticipantError::allocation, "retail participant allocation");
    } catch (...) {
        fail(failure, RetailParticipantError::native_exception, "retail participant creation");
    }
    return {};
}

std::shared_ptr<NativeRetailInputParticipant> NativeRetailParticipants::first_two(
    std::uint32_t identity, RetailParticipantFailure& failure) const noexcept {
    if (!access(failure)) return {};
    if (identity >= first_two_.size()) {
        fail(failure, RetailParticipantError::invalid_identity, "retail participant identity");
        return {};
    }
    return first_two_[identity];
}

bool NativeRetailParticipants::reconstruct_first_two_after_detach(
    RetailParticipantFailure& failure) noexcept {
    if (!access(failure)) return false;
    // The source calls 137810 on the first record, then the second. This
    // operation is only the constructor pair; the caller owns prior detach.
    for (auto& participant : first_two_) {
        if (!participant->reinitialize(failure)) return false;
    }
    return true;
}

} // namespace mka::retail
