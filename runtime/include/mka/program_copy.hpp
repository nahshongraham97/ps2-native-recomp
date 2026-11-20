#pragma once
#include "mka/runtime_preparation.hpp"
#include "mka/host_voice.hpp"
namespace mka::sound {
enum class ProgramCopyError {none,absent_runtime_reference,empty_program,missing_instance,
    duplicate_instance,foreign_instance,alternate_instance,missing_terminator,already_scheduled,
    alternate_schedule_unbound,not_scheduled,foreign_step,wrong_command,output_already_bound,
    unsupported_stop_mode,wrong_stop_metadata,stopped_copy,voice_failed,resource_access,allocation_failed,host_capacity};
struct ProgramCopyFailure {ProgramCopyError kind=ProgramCopyError::none;std::size_t command_index=0;HostVoiceFailure voice;AudioResourceFailure resource;};
struct NormalPlayInstanceObservation {
    NormalInstanceTemplateState state;
    bool output_bound=false,output_active=false;
    std::optional<std::uint64_t> output_source_index;
};
struct NormalOutputParameters {
    AudioControlFactors context,program;
    NativeEnvelopeProfile envelope;
    std::uint32_t source_output_mode,host_output_rate;
};
struct OrdinaryProgramSchedule {
    std::shared_ptr<const PreparedProgram> data;
    ProgramClockWord deadline;
    std::uint16_t play_flags=0;
};
struct NativeProgramOutputObservation {
    std::size_t voices=0,active_voices=0;
    std::optional<std::uint64_t> first_voice_source_index;
};
// Source30000 arithmetic only; this samples no clock and admits no output owner.
ProgramClockWord ordinary_initial_deadline(const CommandRecord&,ProgramClockWord)noexcept;
// Source2FFA0 command reset only; preflight prevents partial malformed mutation.
bool reset_ordinary_command_state(std::vector<CommandRecord>&,ProgramCopyFailure&);
// Private command/instance copies and real native voice ownership. Admission,
// owner-list identity, source activation and callback delivery remain separate.
// The native owner serializes calls and borrowed views for each copy.
class NativeProgramCopy final {
public:
    NativeProgramCopy(const NativeProgramCopy&)=delete;
    NativeProgramCopy& operator=(const NativeProgramCopy&)=delete;
    ~NativeProgramCopy();
    // Failure leaves the actual semantic reference with the caller.
    static std::unique_ptr<NativeProgramCopy> clone(std::unique_ptr<RuntimeProgramReference>&,ProgramCopyFailure&);
    const std::vector<CommandRecord>& commands()const noexcept;
    std::optional<NormalPlayInstanceObservation> instance(std::size_t command_index,ProgramCopyFailure&)const;
    // Required inputs come from the real caller; this is a schedule plan, never
    // a substitute for native output admission/identity-list installation.
    std::optional<OrdinaryProgramSchedule> prepare_ordinary_schedule(ProgramClockWord,std::uint16_t actual_play_flags,ProgramCopyFailure&);
    // Actual command operands are applied before attempting voice construction;
    // a failed voice leaves those operand writes, not a fabricated output.
    bool start_output(const ProgramStep&,const NormalOutputParameters&,ProgramCopyFailure&);
    std::optional<std::vector<float>> mix(std::size_t host_frames,ProgramCopyFailure&);
    bool stop_outputs(const ProgramStep&,ProgramCopyFailure&);
private:
    friend class NativePlaybackOwner;
    // The existing native caller supplies an already activated deadline. This
    // binds a private copy without inventing a source clock/admission receipt.
    std::optional<OrdinaryProgramSchedule> bind_preactivated_schedule(ProgramClockWord,std::uint16_t,ProgramCopyFailure&);
    NativeProgramOutputObservation output_observation()const noexcept;
    void append_outputs(std::vector<HostVoice*>&);
    struct Impl;
    explicit NativeProgramCopy(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> state_;
};
} // namespace mka::sound
