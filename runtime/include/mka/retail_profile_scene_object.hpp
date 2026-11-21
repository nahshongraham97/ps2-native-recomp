#pragma once

#include "mka/retail_scene_object.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <thread>

namespace mka::retail {
enum class RetailProfileSceneObjectError {
    none, wrong_thread, retired, invalid_role, allocation, native_exception,
    missing_session, incompatible_object, expired_session
};
struct RetailProfileSceneObjectFailure {
    RetailProfileSceneObjectError kind{RetailProfileSceneObjectError::none};
    const char* operation{""};
};
struct RetailProfileSceneLaneValues {
    std::uint32_t word_10{}, word_24{}, word_2c{};
};
struct RetailProfileSceneValues {
    std::array<RetailProfileSceneLaneValues,2> lanes;
    std::uint32_t word_60{};
};
struct RetailProfileSceneObjectObservation {
    RetailProfileSceneValues values;
    std::uint32_t source_cookie{};
    bool retired{};
};

// Typed menu-object fields reached by18A260/18B0D0 on the cookie-validated
// live scene. The real18B270 producer must supply its actual initial values
// and cookie. This class does not complete that producer or invent its state.
class NativeRetailProfileSceneObject final : public NativeRetailSceneObject {
public:
    static std::shared_ptr<NativeRetailProfileSceneObject> create(
        std::shared_ptr<NativeRetailGameSession>, std::uint32_t actual_cookie,
        const RetailProfileSceneValues& actual_initial_values,
        RetailProfileSceneObjectFailure&) noexcept;
    NativeRetailProfileSceneObject(const NativeRetailProfileSceneObject&) = delete;
    NativeRetailProfileSceneObject& operator=(const NativeRetailProfileSceneObject&) = delete;

    const NativeRetailGameSession* owner_session() const noexcept override;
    bool scene_identity_live() const noexcept override;
    std::uint32_t source_cookie() const noexcept override;
    void cleanup_from_12fa00() override;
    bool retire_from_12df60(RetailProfileSceneObjectFailure&) noexcept;
    // Same selected lane, then shared counter; no calls between those stores.
    bool stage_from_18a260(std::uint32_t role, bool source_byte14_bit0,
        RetailProfileSceneObjectFailure&) noexcept;
    // The original18B0D0 stores the caller's word to +10/+3C at two distinct
    // source points. Keep this separate from18A260's lane/counter stores.
    bool store_lane_word_from_18b0d0(std::uint32_t role, std::uint32_t value,
        RetailProfileSceneObjectFailure&) noexcept;
    std::optional<RetailProfileSceneObjectObservation> observe(
        RetailProfileSceneObjectFailure&) const noexcept;
private:
    NativeRetailProfileSceneObject(std::shared_ptr<NativeRetailGameSession>,std::uint32_t,
        const RetailProfileSceneValues&) noexcept;
    std::shared_ptr<NativeRetailGameSession> lock_owner(RetailProfileSceneObjectFailure&) const noexcept;
    const NativeRetailGameSession* session_identity_;
    std::weak_ptr<NativeRetailGameSession> session_;
    std::atomic<std::uint32_t> cookie_;
    RetailProfileSceneValues values_;
    bool retired_{};
    const std::thread::id thread_{std::this_thread::get_id()};
};
} // namespace mka::retail
