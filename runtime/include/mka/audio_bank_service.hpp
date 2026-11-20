#pragma once
#include "mka/audio_bank_protocol.hpp"
namespace mka::sound {
enum class BankServiceError { none, moved_service, closed, wrong_command, unsupported_null_bank_token,
    duplicate_bank_token, unknown_bank_token, unloaded_bank_token, unpublished_bank_token,
    sync_during_async_unsupported, no_pending_work, worker_active, pending_completions,
    cancellation_unsupported, protocol_failed, allocation_failed, host_capacity, native_exception };
struct BankServiceFailure { BankServiceError kind=BankServiceError::none; AudioProtocolFailure protocol; };
struct BankLoadOutcome {
    // Sync source0x14 result: exact supplied opaque bank word on prepared success,
    // zero on load failure. Detailed host failure remains available separately.
    BankWireToken bank;
    BankServiceError native_failure = BankServiceError::none;
    BankPreparationReport preparation;
    PreparationFailure preparation_failure;
    AudioProtocolFailure protocol_failure;
};
struct BankLoadCompletion {
    std::uint32_t callback_token=0;
    std::array<std::uint32_t,2> words{}; // Actual producer0/status+fullbank or2/error.
    BankLoadOutcome outcome;
};
enum class AsyncBankSubmission { queued, completed_busy };
// Host-owned deterministic executor/queue. Call run_one from the host executor;
// no IOP/SIF/guest callback address, library thread emulation, or runtime fallback.
// Admitted jobs/completions belong to this service: drain before close/destruction.
// Cancellation is explicitly unsupported; destruction does not invent callbacks.
class AudioBankService {
public:
    AudioBankService(const AudioBankService&)=delete;
    AudioBankService& operator=(const AudioBankService&)=delete;
    AudioBankService(AudioBankService&&) noexcept=default;
    AudioBankService& operator=(AudioBankService&&) noexcept=default;
    static std::optional<AudioBankService> create(AudioBankProtocol&&, BankServiceFailure&);
    std::optional<BankLoadOutcome> load_sync(const AudioBankCommand&, BankWireToken explicit_result_identity, BankServiceFailure&);
    std::optional<AsyncBankSubmission> submit_async(const AudioBankCommand&, BankWireToken explicit_result_identity, BankServiceFailure&);
    bool run_one(BankServiceFailure&);
    std::optional<BankLoadCompletion> poll_completion(BankServiceFailure&);
    std::optional<AudioResource> resolve_play_kind1(const AudioBankCommand&,std::size_t, BankServiceFailure&) const;
    // Completion must have been delivered. Returned immutable data owns its
    // bytes; live executor/voice stop ordering belongs to the playback owner.
    std::shared_ptr<const PreparedProgram> prepare_program(const AudioBankCommand&,
        PreparationFailure&,BankServiceFailure&);
    bool unload(const AudioBankCommand&,BankServiceFailure&);
    bool cancel_async(BankServiceFailure&); // Always named unsupported; pending work unchanged.
    bool close_when_idle(BankServiceFailure&);
    std::shared_ptr<AudioResourceDomain> resource_domain() const noexcept;
private:
    struct State;
    explicit AudioBankService(std::shared_ptr<State> state):state_(std::move(state)){}
    std::shared_ptr<State> state_;
};
} // namespace mka::sound
