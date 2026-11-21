#pragma once

#include "mka/audio_banks.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

namespace mka::sound {

enum class RetailSoundBankJobError {
    none, wrong_thread, invalid_target, invalid_path, invalid_session, duplicate_target_owner,
    no_pending_work, callback_active, after_effect_failure, allocation_failed,
    host_capacity, bank_failed, prepare_failed, unload_failed
};
struct RetailSoundBankJobFailure {
    RetailSoundBankJobError kind = RetailSoundBankJobError::none;
    AudioBanksFailure bank;
    PreparationFailure preparation;
};
enum class RetailSoundBankJobStatus : std::uint32_t {
    pending = 1, failed = 2, cancelled_queued = 3,
    cancelled_after_active = 4, completed = 0
};
struct RetailSoundBankJobCompletion {
    RetailSoundBankJobStatus status = RetailSoundBankJobStatus::failed;
    std::optional<AudioBankHandle> bank;
    AudioBanksFailure bank_failure;
    PreparationFailure preparation_failure;
};
struct RetailSoundBankJobObservation {
    RetailSoundBankJobStatus status = RetailSoundBankJobStatus::pending;
    std::optional<AudioBankHandle> bank;
    bool callback_delivered = false;
};

// A required native callback destination. Its identity, not a catalog index,
// is the duplicate-request key used by source 317B30. No callback is supplied
// by this component; the real catalog owner must bind one.
class NativeRetailSoundBankJobTarget final {
public:
    using Callback = std::function<void(const RetailSoundBankJobCompletion&)>;
    static std::shared_ptr<NativeRetailSoundBankJobTarget> create(
        Callback, RetailSoundBankJobFailure&);
    std::optional<RetailSoundBankJobObservation> observe(RetailSoundBankJobFailure&) const noexcept;
private:
    explicit NativeRetailSoundBankJobTarget(Callback callback)
        : callback_(std::move(callback)), owner_(std::this_thread::get_id()) {}
    Callback callback_;
    std::thread::id owner_;
    RetailSoundBankJobObservation state_;
    friend class NativeRetailSoundBankJobs;
};

enum class RetailSoundBankSubmission { queued, already_pending };

// A dynamic native pathname queue over one real AudioBankSession. One run_one
// performs actual load and source-verified preparation, then publishes its
// result before the next queued job starts. It creates no wire bank token.
class NativeRetailSoundBankJobs final {
public:
    static std::unique_ptr<NativeRetailSoundBankJobs> create(
        AudioBankSession&&, RetailSoundBankJobFailure&);
    NativeRetailSoundBankJobs(const NativeRetailSoundBankJobs&) = delete;
    NativeRetailSoundBankJobs& operator=(const NativeRetailSoundBankJobs&) = delete;
    std::optional<RetailSoundBankSubmission> submit(
        std::string_view pathname, std::shared_ptr<NativeRetailSoundBankJobTarget>,
        RetailSoundBankJobFailure&);
    bool run_one(RetailSoundBankJobFailure&);
    std::optional<std::size_t> cancel_path_from_317960(
        std::string_view pathname, RetailSoundBankJobFailure&);
    bool unload(const AudioBankHandle&, RetailSoundBankJobFailure&);
    std::size_t queued_count() const noexcept { return jobs_.size(); }
private:
    struct Job;
    explicit NativeRetailSoundBankJobs(AudioBankSession&& session)
        : session_(std::move(session)), owner_(std::this_thread::get_id()) {}
    bool available(RetailSoundBankJobFailure&) const noexcept;
    void notify(const std::shared_ptr<Job>&, RetailSoundBankJobCompletion&,
        RetailSoundBankJobFailure&);
    bool drain_marked_same_path(RetailSoundBankJobFailure&);
    AudioBankSession session_;
    std::list<std::shared_ptr<Job>> jobs_;
    std::thread::id owner_;
    bool callback_active_ = false;
    bool failed_after_effect_ = false;
    std::shared_ptr<Job> completed_cancel_anchor_;
};

} // namespace mka::sound
