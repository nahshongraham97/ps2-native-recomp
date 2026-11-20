#pragma once
#include "mka/audio_banks.hpp"
#include <vector>

namespace mka::sound {
enum class AudioBankOpcode : std::uint32_t { load_sync=0x14, unload=0x15, play=0x36, load_async=0x3c, play_extended=0x6a };
struct BankWireToken { std::uint32_t value = 0; }; // Entire opaque word, never a slot or address conversion.
enum class AudioProtocolError {
    none, null_input, truncated_frame, invalid_frame_size, unknown_opcode,
    invalid_command_size, unterminated_path, empty_path, wrong_command,
    unsupported_null_bank_token, unknown_bank_token, duplicate_bank_token, moved_bridge,
    bank_failure, allocation_failed, host_capacity
};
struct AudioProtocolFailure { AudioProtocolError kind=AudioProtocolError::none; AudioBanksFailure bank; };
struct AudioBankCommand {
    AudioBankOpcode opcode=AudioBankOpcode::unload;
    std::uint32_t callback_token=0; // Envelope token; distinct from bank_token.
    std::uint32_t context_word=0, opaque_load_argument=0, opaque_load_tag=0;
    BankWireToken bank_token;
    std::int32_t selector=0;
    std::string pathname;
    // Own all original payload bytes, including unassigned play parameters/padding.
    std::vector<std::uint8_t> payload;
    std::size_t frame_bytes=0;
};
// Decode ONE word-count-framed EE command: {total_words,callback_token,payload...}.
// Allows a following frame in the supplied buffer, never reads beyond this frame.
std::optional<AudioBankCommand> decode_audio_bank_frame(const std::uint8_t*, std::size_t,
                                                       AudioProtocolFailure&);
// Owns the parsed bank and original request association, not load completion.
struct UnpreparedBankLoad { AudioBankCommand request; AudioBankHandle handle; };
// No transport registration, callback send, prepared/ready transition or playback.
// Explicit token assignment is a bridge input, not invented pointer/ordinal encoding.
class AudioBankProtocol {
public:
    AudioBankProtocol(const AudioBankProtocol&)=delete;
    AudioBankProtocol& operator=(const AudioBankProtocol&)=delete;
    AudioBankProtocol(AudioBankProtocol&&) noexcept=default;
    AudioBankProtocol& operator=(AudioBankProtocol&&) noexcept=default;
    static std::optional<AudioBankProtocol> create(AudioBankSession&&, AudioProtocolFailure&);
    std::optional<UnpreparedBankLoad> load_unprepared(const AudioBankCommand&, AudioProtocolFailure&);
    std::optional<PreparedBank> prepare_loaded(const AudioBankHandle&, BankPreparationReport&, PreparationFailure&, AudioProtocolFailure&);
    std::shared_ptr<const PreparedProgram> prepare_program(const AudioBankCommand&,
        PreparationFailure&,AudioProtocolFailure&);
    std::shared_ptr<PreparedProgramWork> admit_deferred_normal(const AudioBankCommand&,
        PreparationFailure&,AudioProtocolFailure&);
    std::unique_ptr<RuntimeProgramReference> acquire_runtime_program(const AudioBankCommand&,
        PreparationFailure&,AudioProtocolFailure&);
    bool close_session(AudioProtocolFailure&);
    bool discard_unprepared(const AudioBankHandle&, AudioProtocolFailure&);
    // Null bank tokens are explicitly unsupported by this host ownership route;
    // retail null unload is a no-op, not a global-context bank identity.
    bool bind_unprepared(BankWireToken, const AudioBankHandle&, AudioProtocolFailure&);
    std::optional<AudioBankHandle> lookup(BankWireToken, AudioProtocolFailure&) const;
    // Checks binding and exact selector/command route only. It never starts sound.
    std::optional<AudioResource> resolve_play_kind1(const AudioBankCommand&, std::size_t command_index,
                                                   AudioProtocolFailure&) const;
    bool unload(const AudioBankCommand&, AudioProtocolFailure&);
    std::shared_ptr<AudioResourceDomain> resource_domain() const noexcept;
private:
    struct State;
    explicit AudioBankProtocol(std::shared_ptr<State> s):state_(std::move(s)){}
    std::shared_ptr<State> state_;
};
} // namespace mka::sound
