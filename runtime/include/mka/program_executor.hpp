#pragma once
#include "mka/bank_preparation.hpp"
namespace mka::sound {
// Source millisecond clock WORD, not guest cycles or a host scheduling cap.
struct ProgramClockWord { std::uint32_t milliseconds=0; };
enum class ProgramError { none, null_program, empty_program, moved_executor, unsupported_command_kind,
    missing_prepared_sample, ambiguous_prepared_sample, invalid_prepared_sample,
    invalid_start_acknowledgement, native_start_failed };
struct ProgramFailure { ProgramError kind=ProgramError::none; std::size_t command_index=0; std::int32_t command_kind=0; };
struct SampleStartOperands {
    std::int16_t parameter_44=0, parameter_46=0, parameter_48=0;
    std::uint32_t descriptor_word_28=0, start_argument=0;
    std::int16_t repeat_operand=0;
    bool set_descriptor_flag_1=false, set_descriptor_flag_4=false;
};
enum class ProgramStopReason { command_kind7, owner_request };
enum class ProgramStepKind { waiting, sample_start, awaiting_start_result, program_stop, structural_end, finished };
struct ProgramStep {
    ProgramStepKind kind=ProgramStepKind::waiting;
    std::size_t command_index=0;
    ProgramClockWord scheduled;
    std::optional<ProgramClockWord> observed;
    CommandRecord command;
    std::shared_ptr<const PreparedProgram> owner;
    std::shared_ptr<const PreparedNormal> clip; // Real owned prepared PCM+encoded flags.
    SampleStartOperands start;
    ProgramStopReason stop_reason=ProgramStopReason::command_kind7;
    std::uint16_t stop_mode_bit=0; // State+1E bit0 passed into cleanup; not fade semantics.
};
// Executes the proved normal command timeline into typed host operations.
// No hardware/mixer/voice allocation or guessed gain/pan/pitch/loop behavior.
// Caller supplies the already-activated initial source deadline explicitly.
// Each step emits at most one operation; kind1 requires actual host start ACK
// before advancing. Unknown/control kinds fail at their actual cursor position.
class ProgramExecutor {
public:
    ProgramExecutor(const ProgramExecutor&)=delete;
    ProgramExecutor& operator=(const ProgramExecutor&)=delete;
    ProgramExecutor(ProgramExecutor&&) noexcept=default;
    ProgramExecutor& operator=(ProgramExecutor&&) noexcept=default;
    static std::optional<ProgramExecutor> create(std::shared_ptr<const PreparedProgram>, ProgramClockWord initial_deadline, ProgramFailure&);
    std::optional<ProgramStep> step(ProgramClockWord now, ProgramFailure&);
    bool acknowledge_start(bool actually_started, ProgramFailure&);
    std::optional<ProgramStep> stop_from_owner(ProgramFailure&);
    ProgramClockWord deadline() const noexcept { return {deadline_}; }
    std::size_t command_index() const noexcept { return cursor_; }
private:
    enum class State { running, awaiting_start, finished, failed };
    ProgramExecutor(std::shared_ptr<const PreparedProgram> p, ProgramClockWord d):program_(std::move(p)),deadline_(d.milliseconds){}
    void advance();
    bool fail(ProgramError,ProgramFailure&);
    std::shared_ptr<const PreparedProgram> program_;
    std::size_t cursor_=0;
    std::uint32_t deadline_=0;
    State state_=State::running;
    bool structural_end_pending_=false;
    ProgramFailure terminal_failure_;
};
} // namespace mka::sound
