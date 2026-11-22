#include "mka/retail_sound_bank_catalog.hpp"
#include "mka/retail_sound_catalog_data.hpp"
#include "retail_main_control.hpp"
#include <algorithm>
#include <new>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace mka::retail {
using sound::RetailSoundBankJobStatus;
struct NativeRetailSoundBankCatalog::State {
    struct Record {
        std::string_view name;
        std::string path;
        std::uint8_t flags{};
        bool pending{};
        std::optional<sound::AudioBankHandle> bank;
        std::optional<RetailSoundBankJobStatus> completion;
        std::shared_ptr<sound::NativeRetailSoundBankJobTarget> target;
    };
    std::optional<sound::AudioBankSession> session;
    std::unique_ptr<sound::NativeRetailSoundBankJobs> jobs;
    std::weak_ptr<NativeRetailMainLifetime> lifetime;
    const std::thread::id thread{std::this_thread::get_id()};
    std::vector<Record> records;
    std::unordered_map<std::uint32_t, RetailSoundSelection> slots;
    bool closed{}, failed_after_effect{}, bank_failed{}, selection_active{};
    std::optional<sound::RetailSoundBankJobCompletion> failed_completion;
    std::uint32_t selected_descriptor{};
    bool select(std::uint32_t, std::span<const sound_catalog_data::Selection>,
                NativeRetailBankReferenceRetirement*, RetailSoundCatalogFailure&);

    bool available(RetailSoundCatalogFailure& failure, bool check_terminal = false) const noexcept {
        failure = {};
        const auto owner = lifetime.lock();
        if (thread != std::this_thread::get_id() || !owner || !owner->active_on_owner_thread()) {
            failure.kind = RetailSoundCatalogError::wrong_owner;
            failure.operation = "retail sound catalog Main lifetime/thread";
            return false;
        }
        if (closed) { failure.kind = RetailSoundCatalogError::closed; return false; }
        if (!session || !session->share_native_services(failure.bank)) {
            failure.kind = RetailSoundCatalogError::bank_session;
            failure.operation = "retail sound catalog shared native Session is closed";
            return false;
        }
        if (check_terminal && (failed_after_effect || bank_failed)) {
            failure.kind = bank_failed ? RetailSoundCatalogError::bank_failed : RetailSoundCatalogError::after_effect_failure;
            failure.operation = "retail sound catalog prior required work failure";
            if (failed_completion) {
                failure.job.kind = failed_completion->preparation_failure.kind == sound::PreparationError::none ?
                    sound::RetailSoundBankJobError::bank_failed : sound::RetailSoundBankJobError::prepare_failed;
                failure.job.bank = failed_completion->bank_failure;
                failure.job.preparation = failed_completion->preparation_failure;
            }
            return false;
        }
        return true;
    }
    void completed(std::size_t index, const sound::RetailSoundBankJobCompletion& result) {
        auto& record = records.at(index);
        record.completion = result.status;
        // Source12B380 stores the real result before clearing +4 in its delay
        // slot. Failure/cancellation statuses1..4 clear both handle and pending.
        if (result.status == RetailSoundBankJobStatus::completed) {
            record.bank = result.bank;
            record.pending = false;
            if (!record.bank) { bank_failed = true; failed_completion = result; }
        } else if (result.status == RetailSoundBankJobStatus::pending ||
                   result.status == RetailSoundBankJobStatus::failed ||
                   result.status == RetailSoundBankJobStatus::cancelled_queued ||
                   result.status == RetailSoundBankJobStatus::cancelled_after_active) {
            record.bank.reset();
            record.pending = false;
            if (result.status == RetailSoundBankJobStatus::failed) { bank_failed = true; failed_completion = result; }
        } else {
            failed_after_effect = true;
        }
    }
};

std::shared_ptr<NativeRetailSoundBankCatalog> NativeRetailSoundBankCatalog::open(
    const content::NativeContentVolume& volume, const NativeRetailFilePaths& paths,
    const std::shared_ptr<NativeRetailMainLifetime>& lifetime, RetailSoundCatalogFailure& failure) noexcept {
    failure = {};
    if (!lifetime || !lifetime->active_on_owner_thread()) {
        failure.kind = RetailSoundCatalogError::wrong_owner; return {};
    }
    try {
        auto ms2 = paths.open(volume, "/sounds/MSLASSET.MS2", failure.path);
        if (!ms2) { failure.kind = RetailSoundCatalogError::content; failure.operation = "/sounds/MSLASSET.MS2"; return {}; }
        // Explicit retail asset association, verified by the pinned MS2/MS4
        // resource corpus. Never infer a companion or select a different file.
        auto ms4 = paths.open(volume, "/sounds/MSLASSET.MS4", failure.path);
        if (!ms4) { failure.kind = RetailSoundCatalogError::content; failure.operation = "/sounds/MSLASSET.MS4"; return {}; }
        auto session = sound::AudioBankSession::adopt(std::move(*ms2), std::move(ms4), failure.bank);
        if (!session) { failure.kind = RetailSoundCatalogError::bank_session; return {}; }
        auto work_session = session->share_native_services(failure.bank);
        if (!work_session) { failure.kind = RetailSoundCatalogError::bank_session; return {}; }
        auto jobs = sound::NativeRetailSoundBankJobs::create(std::move(*work_session), failure.job);
        if (!jobs) { failure.kind = RetailSoundCatalogError::jobs; return {}; }
        auto state = std::make_shared<State>();
        state->session = std::move(session);
        state->jobs = std::move(jobs);
        state->lifetime = lifetime;
        state->records.reserve(sound_catalog_data::banks.size());
        state->slots.reserve(sound_catalog_data::mode_one.size());
        for (const auto& data : sound_catalog_data::banks)
            state->records.push_back({data.name, std::string(data.name) + ".msb", data.flags, false, {}, {}, {}});
        const std::weak_ptr<State> weak = state;
        for (std::size_t i = 0; i < state->records.size(); ++i) {
            auto target = sound::NativeRetailSoundBankJobTarget::create(
                [weak, i](const sound::RetailSoundBankJobCompletion& result) {
                    if (const auto retained = weak.lock()) retained->completed(i, result);
                    else throw std::runtime_error("12B380 native catalog expired before completion");
                }, failure.job);
            if (!target) { failure.kind = RetailSoundCatalogError::jobs; return {}; }
            state->records[i].target = std::move(target);
        }
        return std::shared_ptr<NativeRetailSoundBankCatalog>(new NativeRetailSoundBankCatalog(std::move(state)));
    } catch (const std::bad_alloc&) { failure.kind = RetailSoundCatalogError::allocation; }
      catch (const std::length_error&) { failure.kind = RetailSoundCatalogError::host_capacity; }
      catch (...) { failure.kind = RetailSoundCatalogError::native_exception; }
    failure.operation = "native current-sound bank catalog construction";
    return {};
}

bool NativeRetailSoundBankCatalog::select_mode_one(RetailSoundCatalogFailure& failure) {
    return state_->select(1, sound_catalog_data::mode_one, nullptr, failure);
}
bool NativeRetailSoundBankCatalog::select_mode_one(
    NativeRetailBankReferenceRetirement& retirement, RetailSoundCatalogFailure& failure) {
    return state_->select(1, sound_catalog_data::mode_one, &retirement, failure);
}
bool NativeRetailSoundBankCatalog::select_mode_zero(
    NativeRetailBankReferenceRetirement& retirement, RetailSoundCatalogFailure& failure) {
    return state_->select(0, sound_catalog_data::mode_zero, &retirement, failure);
}
std::optional<std::uint32_t> NativeRetailSoundBankCatalog::selected_descriptor(
    RetailSoundCatalogFailure& failure) const noexcept {
    if (!state_->available(failure)) return {};
    return state_->selected_descriptor;
}
bool NativeRetailSoundBankCatalog::State::select(
    std::uint32_t descriptor, std::span<const sound_catalog_data::Selection> requests,
    NativeRetailBankReferenceRetirement* retirement, RetailSoundCatalogFailure& failure) {
    if (!available(failure, true)) return false;
    if (selection_active) {
        failure.kind = RetailSoundCatalogError::selection_active;
        failure.operation = "native bank selection reentry"; return false;
    }
    struct Active { bool& flag; explicit Active(bool& value):flag(value){flag=true;} ~Active(){flag=false;} } active(selection_active);
    selected_descriptor = descriptor; //23D091, before scan/cleanup/requests.
    try {
        // Source12A170(1,0) visits every active record before requesting the
        // selected descriptor. This is a game catalog scan, not a work limit.
        for (std::size_t i = 0; i < records.size(); ++i) {
            auto& record = records[i];
            const bool selected = std::any_of(requests.begin(), requests.end(),
                [i](const auto& entry) { return entry.bank == i; });
            if (selected || (!record.bank && !record.pending)) continue;
            if (record.bank) {
                const auto bank = *record.bank;
                if (!retirement || !retirement->retire(bank, failure)) {
                    failed_after_effect = true;
                    if (failure.kind == RetailSoundCatalogError::none) {
                        failure.kind = RetailSoundCatalogError::live_reference_cleanup_required;
                        failure.operation = "actual live bank reference retirement required";
                    }
                    return false;
                }
                if (!available(failure, true) || !jobs) {
                    failed_after_effect = true;
                    if (failure.kind == RetailSoundCatalogError::none) {
                        failure.kind = RetailSoundCatalogError::closed;
                        failure.operation = "native mode bank owner retired during cleanup";
                    }
                    return false;
                }
                if (!jobs->unload(bank, failure.job)) {
                    failed_after_effect = true; failure.kind = RetailSoundCatalogError::jobs;
                    failure.operation = "native mode bank backing unload"; return false;
                }
                record.bank.reset(); // Only after reference cleanup AND unload.
                continue;
            }
            if (!jobs->cancel_path_from_317960(record.path, failure.job)) {
                failed_after_effect = true; failure.kind = RetailSoundCatalogError::jobs; return false;
            }
        }
        for (const auto& entry : requests) {
            auto& record = records.at(entry.bank);
            if (!record.bank && !record.pending) {
                record.bank.reset();
                record.pending = true;
                record.completion.reset();
                const auto submitted = jobs->submit(record.path, record.target, failure.job);
                if (!submitted) {
                    failed_after_effect = true;
                    failure.kind = RetailSoundCatalogError::jobs;
                    failure.operation = "12A560 native bank job submission failed";
                    return false;
                }
            }
            // Source12A6C0 writes the slot even if a live/pending record
            // suppresses a new load. The byte is original data at record+8.
            slots[entry.slot] = {entry.bank, record.flags};
        }
        return true;
    } catch (const detail::NativeMainReentry&) { throw; }
      catch (const std::bad_alloc&) { failure.kind = RetailSoundCatalogError::allocation; }
      catch (const std::length_error&) { failure.kind = RetailSoundCatalogError::host_capacity; }
      catch (...) { failure.kind = RetailSoundCatalogError::native_exception; }
    failed_after_effect = true;
    failure.operation = "native mode bank selection after-effect failure";
    return false;
}

std::optional<bool> NativeRetailSoundBankCatalog::pending(RetailSoundCatalogFailure& failure) const noexcept {
    if (!state_->available(failure, true)) return {};
    return std::any_of(state_->records.begin(), state_->records.end(), [](const auto& record) { return record.pending; });
}
bool NativeRetailSoundBankCatalog::pump_one(RetailSoundCatalogFailure& failure) {
    if (!state_->available(failure, true)) return false;
    if (state_->selection_active) {
        failure.kind = RetailSoundCatalogError::selection_active;
        failure.operation = "native bank pump during selection"; return false;
    }
    if (!state_->jobs->queued_count()) return true;
    if (!state_->jobs->run_one(failure.job)) {
        failure.kind = RetailSoundCatalogError::jobs;
        failure.operation = "native current-sound bank work/completion";
        return false;
    }
    return state_->available(failure, true);
}
std::optional<RetailSoundBankObservation> NativeRetailSoundBankCatalog::observe(
    std::uint32_t index, RetailSoundCatalogFailure& failure) const noexcept {
    if (!state_->available(failure)) return {};
    if (index >= state_->records.size()) { failure.kind = RetailSoundCatalogError::invalid_bank; return {}; }
    const auto& row = state_->records[index];
    return RetailSoundBankObservation{row.name, row.flags, row.pending, row.bank, row.completion};
}
std::optional<RetailSoundSelection> NativeRetailSoundBankCatalog::selection(
    std::uint32_t slot, RetailSoundCatalogFailure& failure) const noexcept {
    if (!state_->available(failure)) return {};
    const auto it = state_->slots.find(slot);
    if (it == state_->slots.end()) { failure.kind = RetailSoundCatalogError::missing_slot; return {}; }
    return it->second;
}
std::optional<sound::AudioBankSession> NativeRetailSoundBankCatalog::share_bank_session(
    RetailSoundCatalogFailure& failure) const {
    if (!state_->available(failure, true)) return {};
    auto shared = state_->session->share_native_services(failure.bank);
    if (!shared) failure.kind = RetailSoundCatalogError::bank_session;
    return shared;
}
bool NativeRetailSoundBankCatalog::shutdown(RetailSoundCatalogFailure& failure) {
    failure = {};
    if (state_->thread != std::this_thread::get_id()) { failure.kind = RetailSoundCatalogError::wrong_owner; return false; }
    if (state_->selection_active) {
        failure.kind = RetailSoundCatalogError::selection_active;
        failure.operation = "native bank shutdown during selection"; return false;
    }
    if (state_->closed) return true;
    // Explicit cancellation is completed by the real worker; pending state is
    // never cleared merely because Main is retiring.
    for (auto& record : state_->records) {
        if (record.pending && !state_->jobs->cancel_path_from_317960(record.path, failure.job)) {
            failure.kind = RetailSoundCatalogError::jobs; return false;
        }
    }
    while (state_->jobs->queued_count()) {
        if (!state_->jobs->run_one(failure.job)) { failure.kind = RetailSoundCatalogError::jobs; return false; }
    }
    for (auto& record : state_->records) {
        if (record.bank && !state_->jobs->unload(*record.bank, failure.job)) {
            failure.kind = RetailSoundCatalogError::jobs; return false;
        }
        record.bank.reset();
    }
    state_->jobs.reset();
    state_->session.reset();
    state_->closed = true;
    return true;
}
} // namespace mka::retail
