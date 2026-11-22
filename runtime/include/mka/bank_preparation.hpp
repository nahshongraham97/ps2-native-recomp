#pragma once
#include "mka/bank_asset.hpp"
#include "mka/normal_sample.hpp"
namespace mka::sound {
enum class PreparationError { none, no_backing, malformed_bank, unsupported_named_keys, duplicate_resource_key, source_deferred, already_prepared,
    absent_resource, payload_failed, codec_failed, count2_failed, unsupported_count,
    unsupported_alternate_preparation, absent_selector, absent_program, unloaded,
    allocation_failed, host_capacity, eager_program_failures, resource_access, cancelled, invalid_work_state, wrong_thread, native_exception, runtime_building };
struct PreparationFailure {
    PreparationError kind=PreparationError::none,cleanup_kind=PreparationError::none;
    AudioResourceFailure resource,cleanup_resource;
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
// Actual fresh normal donor state. Clone copies this value; it does not reset it.
struct NormalInstanceTemplateState {
    std::uint32_t flags=0;
    std::int16_t repeat_operand=0;
    std::uint64_t repeat_counter=0;
    std::int32_t gain_q14=0x4000,pan_q14=0,pitch_q12=0x1000;
    std::uint32_t descriptor_word=0,start_argument=0;
};
struct PreparedSampleInstance {
    std::size_t command_index=0;
    CommandRecord command;
    std::int16_t repeat_operand=0; // command+0A, preserved not executed.
    bool source_preserve_resource_after_instance=false; // Source flag40 suppresses cached-ref decrement.
    std::shared_ptr<const PreparedNormal> sample;
    // The cache association is an actual instance owner, independent of PCM consumers.
    std::shared_ptr<const void> cache_reference;
    NormalInstanceTemplateState native_template;
};
struct PreparedProgram {
    std::size_t program_index=0;
    std::uint32_t effective_flags=0;
    std::vector<CommandRecord> commands; // ALL kinds retained; no execution/no-op claim.
    std::vector<PreparedSampleInstance> normal_instances;
};
class NativeRuntimePreparation;
class RuntimeProgramReference;
class PreparedProgramWork;
class RuntimeProgramWork;
struct RuntimeCacheObservation {std::size_t program_index=0,references=0;bool cached=false,deferred=false,building=false;};
struct SampleCacheObservation {std::size_t resource_index=0;bool occupied=false,preserved=false;};
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
    // Admit exact fresh normal data and backing without starting resource work.
    // Full runtime descriptor/callback/coordinator semantics are separate.
    std::shared_ptr<PreparedProgramWork> admit_deferred_normal(std::int32_t selector,PreparationFailure&);
    std::optional<std::vector<SampleCacheObservation>> sample_cache(PreparationFailure&) const;
    std::shared_ptr<RuntimeProgramWork> admit_runtime_program(std::int32_t selector,PreparationFailure&);
    std::unique_ptr<RuntimeProgramReference> acquire_runtime_program(std::int32_t selector,PreparationFailure&);
    std::optional<std::vector<RuntimeCacheObservation>> runtime_cache(PreparationFailure&) const;
    // Synchronous native convenience over the same owned fresh work lifecycle.
    // Does not rewrite a source-deferred program as eagerly prepared.
    std::shared_ptr<const PreparedProgram> prepare_deferred_normal(std::int32_t selector, PreparationFailure&);
    // Serialized against preparation. Revokes new work; already returned results survive.
    bool active() const;
    void unload();
private:
    struct State;
    explicit PreparedBank(std::shared_ptr<State> state):state_(std::move(state)){}
    std::shared_ptr<State> state_;
    friend class PreparedProgramWork;
    friend class RuntimeProgramReference;
    friend class RuntimeProgramWork;
};
enum class ProgramWorkState {queued,building,ready,cancelled,failed,published,retired};
enum class ProgramWorkCancellation {queued_removed,running_requested,not_cancellable};
struct ProgramWorkObservation {
    ProgramWorkState state=ProgramWorkState::queued;
    bool cancellation_open=false,cancellation_requested=false,result_published=false;
    std::optional<PreparationFailure> failure;
};
// One exact fresh data preparation. Cancellation request and actual retirement
// are distinct; there is no default callback or guessed runtime cache branch.
// build/take_result/retire use the actual worker thread. Queue admission owns no
// program-category lease. The host must retain work through callback/suppression
// and explicit retirement; destruction is cleanup, never source completion.
class PreparedProgramWork final {
public:
    PreparedProgramWork(const PreparedProgramWork&)=delete;
    PreparedProgramWork& operator=(const PreparedProgramWork&)=delete;
    ~PreparedProgramWork();
    bool build(PreparationFailure&);
    ProgramWorkCancellation cancel();
    std::shared_ptr<const PreparedProgram> take_result(PreparationFailure&);
    bool retire(PreparationFailure&);
    ProgramWorkObservation observe() const;
private:
    struct Impl;
    explicit PreparedProgramWork(std::shared_ptr<Impl> state):state_(std::move(state)){}
    std::shared_ptr<Impl> state_;
    friend class PreparedBank;
};

enum class RuntimeProgramWorkState {queued,building,ready,build_failed,committed,taken,cancelled,failed,retired};
enum class RuntimeProgramWorkBranch {unselected,cached,fresh};
struct RuntimeProgramWorkObservation {
    RuntimeProgramWorkState state=RuntimeProgramWorkState::queued;
    RuntimeProgramWorkBranch branch=RuntimeProgramWorkBranch::unselected;
    // runtime_published records a historical fresh commit, even if a later
    // direct bank unload revokes the still-untaken result.
    bool cancellation_open=false,cancellation_requested=false,runtime_published=false,result_taken=false;
    std::optional<PreparationFailure> failure;
};
// Private runtime construction and exact cached-reference admission. No callback,
// source outstanding-job counter or public association release is synthesized.
class RuntimeProgramWork final {
public:
    RuntimeProgramWork(const RuntimeProgramWork&)=delete;
    RuntimeProgramWork& operator=(const RuntimeProgramWork&)=delete;
    ~RuntimeProgramWork();
    // Selection publishes running ownership before the worker leaves its queue.
    // No resource access or construction occurs here. build() also supports
    // synchronous callers that have not selected their work separately.
    bool begin_build(PreparationFailure&);
    // EE8/E28: begin on this thread only if an actual cached runtime or all
    // required kind1 sample owners are available. False leaves work queued.
    // A selected cached runtime acquires its real semantic reference here.
    std::optional<bool> try_begin_direct(PreparationFailure&);
    bool cached_runtime_available() const;
    bool build(PreparationFailure&);
    bool finish_build(PreparationFailure&);
    ProgramWorkCancellation cancel();
    // Direct unload revokes an untaken result; prior returned references survive.
    std::unique_ptr<RuntimeProgramReference> take_result(PreparationFailure&);
    bool retire(PreparationFailure&);
    RuntimeProgramWorkObservation observe() const;
private:
    struct Impl;
    explicit RuntimeProgramWork(std::shared_ptr<Impl> state):state_(std::move(state)){}
    std::shared_ptr<Impl> state_;
    friend class PreparedBank;
};
} // namespace mka::sound
