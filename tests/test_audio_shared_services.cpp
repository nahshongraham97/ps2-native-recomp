#include "mka/retail_sound_bank_jobs.hpp"
#include <cstdio>
#include <stdexcept>
using namespace mka::sound;
namespace { void check(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); } }
int main(int argc, char** argv) try {
    check(argc == 3, "expected original native MS2/MS4 pair");
    AudioBanksFailure failure;
    auto primary = AudioBankSession::open(argv[1], std::filesystem::path(argv[2]), failure);
    auto foreign = AudioBankSession::open(argv[1], std::filesystem::path(argv[2]), failure);
    check(primary && foreign, "actual separate native sessions");
    auto service = primary->share_native_services(failure);
    auto worker = primary->share_native_services(failure);
    check(service && worker && service->resource_domain() == primary->resource_domain(), "same native resource owner");
    RetailSoundBankJobFailure job_failure;
    auto jobs = NativeRetailSoundBankJobs::create(std::move(*worker), job_failure);
    check(bool(jobs), "actual jobs over shared canonical registry");
    std::optional<AudioBankHandle> result;
    auto target = NativeRetailSoundBankJobTarget::create([&](const RetailSoundBankJobCompletion& completion) {
        check(completion.status == RetailSoundBankJobStatus::completed && completion.bank.has_value(), "real prepared completion");
        result = completion.bank;
    }, job_failure);
    check(target && jobs->submit("announcr.msb", target, job_failure) == RetailSoundBankSubmission::queued &&
        jobs->run_one(job_failure) && result, "actual source announcr completes");
    check(primary->load_state(*result, failure) == AudioBankLoadState::eager_prepared_source_deferred_allowed &&
        service->load_state(*result, failure) == AudioBankLoadState::eager_prepared_source_deferred_allowed,
        "worker handle is the same bank in primary and service registries");
    const auto resource = service->resolve_kind1(*result, 0, 0, failure);
    check(resource && resource->resolved_key == 0x20000002u, "shared native handle resolves actual original command");
    check(!foreign->load_state(*result, failure) && failure.kind == AudioBanksError::foreign_session,
        "independent archive open still cannot alias this bank");
    check(primary->unload_bank(*result, failure), "primary unload commits once");
    check(!service->load_state(*result, failure) && failure.kind == AudioBanksError::unloaded_handle &&
        !jobs->unload(*result, job_failure) && job_failure.bank.kind == AudioBanksError::unloaded_handle,
        "unload revocation is shared, not copied service state");
    auto moved = std::move(*primary);
    check(!primary->share_native_services(failure) && failure.kind == AudioBanksError::moved_session,
        "moved source cannot publish new service view");
    primary.reset();
    check(jobs->submit("announcr.msb", target, job_failure) == RetailSoundBankSubmission::queued && jobs->run_one(job_failure),
        "service work survives destruction of public original wrapper");
    check(service->close(failure), "one canonical explicit close");
    check(!moved.load_state(*result, failure) && failure.kind == AudioBanksError::closed_session &&
        !moved.share_native_services(failure) && failure.kind == AudioBanksError::closed_session,
        "close revokes every same-registry view and prevents new ones");
    std::puts("PASS_NATIVE_AUDIO_SHARED_SERVICES; real files/handle/revocation only; no playback or endpoint");
    return 0;
} catch (const std::exception& error) { std::fprintf(stderr, "FAIL shared sound services: %s\n", error.what()); return 1; }
