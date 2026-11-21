#include "mka/retail_sound_bank_jobs.hpp"
#include "retail_main_control.hpp"

#include <new>
#include <stdexcept>
#include <utility>

namespace mka::sound {

struct NativeRetailSoundBankJobs::Job {
    std::string path;
    std::shared_ptr<NativeRetailSoundBankJobTarget> target;
    bool cancelled = false;
    bool notified = false;
};

std::shared_ptr<NativeRetailSoundBankJobTarget> NativeRetailSoundBankJobTarget::create(
    Callback callback, RetailSoundBankJobFailure& failure) {
    failure = {};
    if (!callback) { failure.kind = RetailSoundBankJobError::invalid_target; return {}; }
    try {
        return std::shared_ptr<NativeRetailSoundBankJobTarget>(
            new NativeRetailSoundBankJobTarget(std::move(callback)));
    } catch (const std::bad_alloc&) { failure.kind = RetailSoundBankJobError::allocation_failed; }
      catch (const std::length_error&) { failure.kind = RetailSoundBankJobError::host_capacity; }
    return {};
}

std::optional<RetailSoundBankJobObservation> NativeRetailSoundBankJobTarget::observe(
    RetailSoundBankJobFailure& failure) const noexcept {
    failure = {};
    if (std::this_thread::get_id() != owner_) {
        failure.kind = RetailSoundBankJobError::wrong_thread; return std::nullopt;
    }
    return state_;
}

std::unique_ptr<NativeRetailSoundBankJobs> NativeRetailSoundBankJobs::create(
    AudioBankSession&& session, RetailSoundBankJobFailure& failure) {
    failure = {};
    if (!session.resource_domain()) { failure.kind = RetailSoundBankJobError::invalid_session; return {}; }
    try {
        return std::unique_ptr<NativeRetailSoundBankJobs>(
            new NativeRetailSoundBankJobs(std::move(session)));
    } catch (const std::bad_alloc&) { failure.kind = RetailSoundBankJobError::allocation_failed; }
      catch (const std::length_error&) { failure.kind = RetailSoundBankJobError::host_capacity; }
    return {};
}

bool NativeRetailSoundBankJobs::available(RetailSoundBankJobFailure& failure) const noexcept {
    failure = {};
    if (std::this_thread::get_id() != owner_) {
        failure.kind = RetailSoundBankJobError::wrong_thread; return false;
    }
    if (failed_after_effect_) {
        failure.kind = RetailSoundBankJobError::after_effect_failure; return false;
    }
    if (callback_active_) {
        failure.kind = RetailSoundBankJobError::callback_active; return false;
    }
    return true;
}

void NativeRetailSoundBankJobs::notify(const std::shared_ptr<Job>& job,
    RetailSoundBankJobCompletion& completion, RetailSoundBankJobFailure&) {
    // Source 317560 writes the callback result/status before invoking it.
    job->target->state_ = {completion.status, completion.bank, true};
    job->notified = true;
    callback_active_ = true;
    struct Clear { bool& active; ~Clear() { active = false; } } clear{callback_active_};
    job->target->callback_(completion);
}

bool NativeRetailSoundBankJobs::drain_marked_same_path(RetailSoundBankJobFailure& failure) {
    if (!completed_cancel_anchor_) return true;
    for (auto it = jobs_.begin(); it != jobs_.end(); ) {
        auto job = *it;
        if (job->notified) { it = jobs_.erase(it); continue; }
        if (!job->cancelled || job->path != completed_cancel_anchor_->path) { ++it; continue; }
        RetailSoundBankJobCompletion completion;
        completion.status = RetailSoundBankJobStatus::cancelled_after_active;
        try { notify(job, completion, failure); }
        catch (const mka::retail::detail::NativeMainReentry&) { throw; }
        catch (...) { failed_after_effect_ = true; failure.kind = RetailSoundBankJobError::after_effect_failure; return false; }
        it = jobs_.erase(it);
    }
    completed_cancel_anchor_.reset();
    return true;
}

std::optional<RetailSoundBankSubmission> NativeRetailSoundBankJobs::submit(
    std::string_view pathname, std::shared_ptr<NativeRetailSoundBankJobTarget> target,
    RetailSoundBankJobFailure& failure) {
    if (!available(failure)) return std::nullopt;
    if (!target || !target->callback_ || target->owner_ != owner_) {
        failure.kind = RetailSoundBankJobError::invalid_target; return std::nullopt;
    }
    if (pathname.empty() || pathname.find('\0') != std::string_view::npos) {
        failure.kind = RetailSoundBankJobError::invalid_path; return std::nullopt;
    }
    // Source 3152A0 runs before the existing-callback scan at 317BF4..C28.
    target->state_ = {RetailSoundBankJobStatus::pending, std::nullopt, false};
    for (const auto& job : jobs_)
        if (job->target == target && !job->notified)
            return RetailSoundBankSubmission::already_pending;
    try {
        auto job = std::make_shared<Job>();
        job->path = pathname;
        job->target = target;
        jobs_.push_back(std::move(job));
        return RetailSoundBankSubmission::queued;
    } catch (const std::bad_alloc&) { failure.kind = RetailSoundBankJobError::allocation_failed; }
      catch (const std::length_error&) { failure.kind = RetailSoundBankJobError::host_capacity; }
    // Source 315250 can finish a request rejected after pending publication.
    // Native resource refusal is named; the required sink receives status 2.
    RetailSoundBankJobCompletion completion;
    completion.status = RetailSoundBankJobStatus::failed;
    target->state_ = {completion.status, std::nullopt, true};
    callback_active_ = true;
    struct Clear { bool& active; ~Clear() { active = false; } } clear{callback_active_};
    try { target->callback_(completion); }
    catch (const mka::retail::detail::NativeMainReentry&) { throw; }
    catch (...) { failed_after_effect_ = true; failure.kind = RetailSoundBankJobError::after_effect_failure; }
    return std::nullopt;
}

bool NativeRetailSoundBankJobs::run_one(RetailSoundBankJobFailure& failure) {
    if (!available(failure)) return false;
    // A prior typed Main transfer may have skipped the original unlink after
    // callback. The callback was delivered once; retire its native node now.
    for (auto it = jobs_.begin(); it != jobs_.end(); )
        if ((*it)->notified) it = jobs_.erase(it); else ++it;
    if (!drain_marked_same_path(failure)) return false;
    if (jobs_.empty()) { failure.kind = RetailSoundBankJobError::no_pending_work; return false; }
    auto job = jobs_.front();
    RetailSoundBankJobCompletion completion;
    try {
        auto handle = session_.load_bank(job->path, failure.bank);
        if (!handle) {
            completion.status = job->cancelled ? RetailSoundBankJobStatus::cancelled_after_active :
                RetailSoundBankJobStatus::failed;
            completion.bank_failure = failure.bank;
        } else {
            BankPreparationReport report;
            auto prepared = session_.prepare_bank(*handle, report, failure.preparation, failure.bank);
            if (!prepared) {
                completion.status = job->cancelled ? RetailSoundBankJobStatus::cancelled_after_active :
                    RetailSoundBankJobStatus::failed;
                completion.bank_failure = failure.bank;
                completion.preparation_failure = failure.preparation;
            } else if (job->cancelled) {
                // Source 3175E4 unloads the returned bank before status4 callback.
                if (!session_.unload_bank(*handle, failure.bank)) {
                    failed_after_effect_ = true;
                    failure.kind = RetailSoundBankJobError::unload_failed;
                    return false;
                }
                completion.status = RetailSoundBankJobStatus::cancelled_after_active;
            } else {
                completion.status = RetailSoundBankJobStatus::completed;
                completion.bank = std::move(*handle);
            }
        }
        if (job->cancelled) completed_cancel_anchor_ = job;
        notify(job, completion, failure);
    } catch (const mka::retail::detail::NativeMainReentry&) { throw; }
      catch (...) { failed_after_effect_ = true; failure.kind = RetailSoundBankJobError::after_effect_failure; return false; }
    jobs_.pop_front();
    if (!drain_marked_same_path(failure)) return false;
    return true;
}

std::optional<std::size_t> NativeRetailSoundBankJobs::cancel_path_from_317960(
    std::string_view pathname, RetailSoundBankJobFailure& failure) {
    if (!available(failure)) return std::nullopt;
    if (pathname.empty() || pathname.find('\0') != std::string_view::npos) {
        failure.kind = RetailSoundBankJobError::invalid_path; return std::nullopt;
    }
    if (jobs_.empty()) return 0;
    const auto head = jobs_.front();
    std::size_t matched = 0;
    for (auto it = jobs_.begin(); it != jobs_.end(); ) {
        auto job = *it;
        auto next = std::next(it);
        if (job->notified) { it = jobs_.erase(it); continue; }
        if (job->path != pathname) { it = next; continue; }
        ++matched;
        // Source 317A00 compares the request with the HEAD pathname, not
        // with this node's position. Matching head path defers cancellation.
        if (head->path == pathname) {
            job->cancelled = true;
            it = next;
            continue;
        }
        RetailSoundBankJobCompletion completion;
        completion.status = RetailSoundBankJobStatus::cancelled_queued;
        try { notify(job, completion, failure); }
        catch (const mka::retail::detail::NativeMainReentry&) { throw; }
        catch (...) { failed_after_effect_ = true; failure.kind = RetailSoundBankJobError::after_effect_failure; return std::nullopt; }
        it = jobs_.erase(it);
    }
    return matched;
}

bool NativeRetailSoundBankJobs::unload(const AudioBankHandle& handle,
    RetailSoundBankJobFailure& failure) {
    if (!available(failure)) return false;
    if (!session_.unload_bank(handle, failure.bank)) {
        failure.kind = RetailSoundBankJobError::unload_failed; return false;
    }
    return true;
}

} // namespace mka::sound
