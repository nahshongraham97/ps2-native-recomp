#pragma once
#include "mka/bank_asset.hpp"
#include "mka/normal_sample.hpp"
namespace mka::sound {
enum class PreparationError { none, no_backing, malformed_bank, unsupported_named_keys, duplicate_resource_key, source_deferred, already_prepared,
    absent_resource, payload_failed, codec_failed, count2_failed, unsupported_count,
    unsupported_alternate_preparation, absent_selector, absent_program, unloaded,
    allocation_failed, host_capacity, eager_program_failures };
struct PreparationFailure {
    PreparationError kind=PreparationError::none;
    BankError bank=BankError::none;
    PayloadFailure payload;
    AdpcmError codec=AdpcmError::none;
    NormalSampleFailure count2;
    std::size_t program_index=0, command_index=0;
};
enum class ProgramPreparationState { source_deferred, eager_prepared, failed };
struct ProgramPreparationObservation {
    std::uint32_t serialized_flags=0, effective_flags=0;
    ProgramPreparationState state=ProgramPreparationState::failed;
    PreparationFailure failure;
};
struct BankPreparationReport {
    bool resource_precreation_skipped=false;
    std::size_t resource_precreation_attempts=0, resource_precreation_failures=0;
    std::size_t eager_error_count=0;
    std::vector<ProgramPreparationObservation> programs;
};
struct PreparedNormal {
    ResourceLookup resource;
    SamplePayload source;
    std::vector<AdpcmMono> pcm; // Ordered planes, no left/right/loop/playtime assertion.
    std::optional<NormalCount2Encoded> count2_encoded;
};
struct PreparedSampleInstance {
    std::size_t command_index=0;
    CommandRecord command;
    std::int16_t repeat_operand=0; // command+0A, preserved not executed.
    bool source_release_resource_on_last_instance=false; // source flag40 policy.
    std::shared_ptr<const PreparedNormal> sample;
};
struct PreparedProgram {
    std::size_t program_index=0;
    std::uint32_t effective_flags=0;
    std::vector<CommandRecord> commands; // ALL kinds retained; no execution/no-op claim.
    std::vector<PreparedSampleInstance> normal_instances;
};
// Source292E4 load-preparation predicate only, not playback/completion transport.
// Returns no bank if any eager program fails. Deferred programs remain explicit.
// All source-byte objects and instance references are host-owned and uncapped.
class PreparedBank {
public:
    PreparedBank(const PreparedBank&)=default;
    PreparedBank(PreparedBank&&) noexcept=default;
    PreparedBank& operator=(const PreparedBank&)=default;
    PreparedBank& operator=(PreparedBank&&) noexcept=default;
    static std::optional<PreparedBank> prepare(BankAssetSource, BankPreparationReport&, PreparationFailure&);
    std::shared_ptr<const PreparedProgram> eager_program(std::int32_t selector, PreparationFailure&) const;
    // Synchronous native work only. Alternate/queue/cancel semantics unsupported.
    // Does not rewrite a source-deferred program as eagerly prepared.
    std::shared_ptr<const PreparedProgram> prepare_deferred_normal(std::int32_t selector, PreparationFailure&);
    // Serialized against preparation. Revokes new work; already returned results survive.
    bool active() const;
    void unload();
private:
    struct State;
    explicit PreparedBank(std::shared_ptr<State> state):state_(std::move(state)){}
    std::shared_ptr<State> state_;
};
} // namespace mka::sound
