#pragma once
#include "mka/retail_file_paths.hpp"
#include "mka/retail_main_lifetime.hpp"
#include "mka/retail_sound_bank_jobs.hpp"
#include "mka/retail_sound_runtime.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

namespace mka::retail {
enum class RetailSoundCatalogError {
    none, allocation, host_capacity, native_exception, wrong_owner, closed,
    after_effect_failure, content, bank_session, jobs, invalid_bank, missing_slot,
    bank_failed, live_reference_cleanup_required, runtime, selection_active
};
struct RetailSoundCatalogFailure {
    RetailSoundCatalogError kind{RetailSoundCatalogError::none};
    const char* operation{""};
    RetailFilePathFailure path;
    sound::AudioBanksFailure bank;
    sound::RetailSoundBankJobFailure job;
    RetailSoundRuntimeFailure runtime;
};
struct RetailSoundBankObservation {
    std::string_view name;
    std::uint8_t selection_flags{};
    bool pending{};
    std::optional<sound::AudioBankHandle> bank;
    std::optional<sound::RetailSoundBankJobStatus> last_completion;
};
struct RetailSoundSelection {
    std::uint32_t bank{};
    std::uint8_t flags{};
};
// Required game-side retirement before bank backing unload (23C700).
// This is a native service interface, not a source callback address.
class NativeRetailBankReferenceRetirement {
public:
    virtual ~NativeRetailBankReferenceRetirement() = default;
    virtual bool retire(const sound::AudioBankHandle&, RetailSoundCatalogFailure&) = 0;
};
// Persistent native game catalog. The source's record indices/selection slots
// are game data; they are never device handles, native capacity or wire tokens.
class NativeRetailSoundBankCatalog final {
public:
    static std::shared_ptr<NativeRetailSoundBankCatalog> open(
        const content::NativeContentVolume&, const NativeRetailFilePaths&,
        const std::shared_ptr<NativeRetailMainLifetime>&, RetailSoundCatalogFailure&) noexcept;
    bool select_mode_one(RetailSoundCatalogFailure&);
    bool select_mode_one(NativeRetailBankReferenceRetirement&, RetailSoundCatalogFailure&);
    bool select_mode_zero(NativeRetailBankReferenceRetirement&, RetailSoundCatalogFailure&);
    std::optional<std::uint32_t> selected_descriptor(RetailSoundCatalogFailure&) const noexcept;
    std::optional<bool> pending(RetailSoundCatalogFailure&) const noexcept;
    bool pump_one(RetailSoundCatalogFailure&);
    bool shutdown(RetailSoundCatalogFailure&);
    std::optional<RetailSoundBankObservation> observe(std::uint32_t, RetailSoundCatalogFailure&) const noexcept;
    std::optional<RetailSoundSelection> selection(std::uint32_t, RetailSoundCatalogFailure&) const noexcept;
    std::optional<sound::AudioBankSession> share_bank_session(RetailSoundCatalogFailure&) const;
private:
    struct State;
    explicit NativeRetailSoundBankCatalog(std::shared_ptr<State> state) : state_(std::move(state)) {}
    std::shared_ptr<State> state_;
};
} // namespace mka::retail
