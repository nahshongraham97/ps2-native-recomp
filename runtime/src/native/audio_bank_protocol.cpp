#include "mka/audio_bank_protocol.hpp"
#include <cstring>
#include <limits>
#include <mutex>
#include <new>
#include <stdexcept>
#include <unordered_map>

namespace mka::sound {
namespace {
std::uint32_t word(const std::uint8_t* b) noexcept {
    return std::uint32_t(b[0])|(std::uint32_t(b[1])<<8)|(std::uint32_t(b[2])<<16)|(std::uint32_t(b[3])<<24);
}
bool is_load(AudioBankOpcode op) { return op==AudioBankOpcode::load_sync || op==AudioBankOpcode::load_async; }
bool is_play(AudioBankOpcode op) { return op==AudioBankOpcode::play || op==AudioBankOpcode::play_extended; }
}
std::optional<AudioBankCommand> decode_audio_bank_frame(
    const std::uint8_t* data, std::size_t size, AudioProtocolFailure& failure) {
    failure={};
    auto fail=[&](AudioProtocolError e)->std::optional<AudioBankCommand>{failure.kind=e;return std::nullopt;};
    if(!data) return fail(AudioProtocolError::null_input);
    if(size<8) return fail(AudioProtocolError::truncated_frame);
    const auto count=word(data);
    if(count<3) return fail(AudioProtocolError::invalid_frame_size);
    const std::uint64_t extent=std::uint64_t(count)*4;
    if(extent>size) return fail(AudioProtocolError::truncated_frame);
    if(extent>std::numeric_limits<std::size_t>::max()) return fail(AudioProtocolError::host_capacity);
    const auto payload_size=static_cast<std::size_t>(extent)-8;
    const auto* p=data+8;
    const auto opcode=word(p);
    AudioBankCommand command;
    command.callback_token=word(data+4);command.frame_bytes=static_cast<std::size_t>(extent);
    switch(opcode) {
    case 0x14: command.opcode=AudioBankOpcode::load_sync;break;
    case 0x15: command.opcode=AudioBankOpcode::unload;break;
    case 0x36: command.opcode=AudioBankOpcode::play;break;
    case 0x3c: command.opcode=AudioBankOpcode::load_async;break;
    case 0x6a: command.opcode=AudioBankOpcode::play_extended;break;
    default:return fail(AudioProtocolError::unknown_opcode);
    }
    const auto op=command.opcode;
    if((op==AudioBankOpcode::unload && payload_size!=8) ||
       (op==AudioBankOpcode::play && payload_size!=32) ||
       (op==AudioBankOpcode::play_extended && payload_size!=40)) return fail(AudioProtocolError::invalid_command_size);
    try {
        if(is_load(op)) {
            const std::size_t path_offset=op==AudioBankOpcode::load_async?16:8;
            if(payload_size<=path_offset) return fail(AudioProtocolError::invalid_command_size);
            command.context_word=word(p+4);
            if(op==AudioBankOpcode::load_async) { command.opaque_load_argument=word(p+8);command.opaque_load_tag=word(p+12); }
            const auto* end=static_cast<const std::uint8_t*>(std::memchr(p+path_offset,0,payload_size-path_offset));
            if(!end) return fail(AudioProtocolError::unterminated_path);
            if(end==p+path_offset) return fail(AudioProtocolError::empty_path);
            command.pathname.assign(reinterpret_cast<const char*>(p+path_offset),static_cast<std::size_t>(end-(p+path_offset)));
        } else {
            command.bank_token={word(p+4)};
            if(is_play(op)) {
                const auto bits=word(p+8);
                command.selector=bits<=0x7fffffffu?static_cast<std::int32_t>(bits):
                    -1-static_cast<std::int32_t>(0xffffffffu-bits);
            }
        }
        command.payload.assign(p,p+payload_size);
        return command;
    } catch(const std::bad_alloc&){return fail(AudioProtocolError::allocation_failed);}
      catch(const std::length_error&){return fail(AudioProtocolError::host_capacity);}
}
struct AudioBankProtocol::State {
    explicit State(AudioBankSession&& value):session(std::move(value)){}
    AudioBankSession session;
    // Retain revoked bindings as tombstones; no implicit stale-token reuse.
    std::unordered_map<std::uint32_t,AudioBankHandle> bindings;
    mutable std::mutex mutex;
};
std::shared_ptr<AudioResourceDomain> AudioBankProtocol::resource_domain() const noexcept {
    return state_ ? state_->session.resource_domain() : nullptr;
}
std::optional<AudioBankProtocol> AudioBankProtocol::create(AudioBankSession&& session, AudioProtocolFailure& failure) {
    failure={};
    try{return AudioBankProtocol(std::make_shared<State>(std::move(session)));}
    catch(const std::bad_alloc&){failure.kind=AudioProtocolError::allocation_failed;}
    catch(const std::length_error&){failure.kind=AudioProtocolError::host_capacity;}
    return std::nullopt;
}
std::optional<UnpreparedBankLoad> AudioBankProtocol::load_unprepared(
    const AudioBankCommand& command, AudioProtocolFailure& failure) {
    failure={};
    if(!state_){failure.kind=AudioProtocolError::moved_bridge;return std::nullopt;}
    if(!is_load(command.opcode)){failure.kind=AudioProtocolError::wrong_command;return std::nullopt;}
    if(command.pathname.empty()){failure.kind=AudioProtocolError::empty_path;return std::nullopt;}
    try {
        // Copy request before publishing a bank, so allocation failure cannot strand it.
        AudioBankCommand copy=command;
        auto bank=state_->session.load_bank(command.pathname,failure.bank);
        if(!bank){failure.kind=AudioProtocolError::bank_failure;return std::nullopt;}
        return UnpreparedBankLoad{std::move(copy),std::move(*bank)};
    } catch(const std::bad_alloc&){failure.kind=AudioProtocolError::allocation_failed;}
      catch(const std::length_error&){failure.kind=AudioProtocolError::host_capacity;}
    return std::nullopt;
}
std::optional<PreparedBank> AudioBankProtocol::prepare_loaded(const AudioBankHandle& handle,
    BankPreparationReport& report,PreparationFailure& preparation,AudioProtocolFailure& failure){
    failure={};report={};preparation={};
    if(!state_){failure.kind=AudioProtocolError::moved_bridge;return std::nullopt;}
    auto prepared=state_->session.prepare_bank(handle,report,preparation,failure.bank);
    if(!prepared)failure.kind=AudioProtocolError::bank_failure;
    return prepared;
}
bool AudioBankProtocol::close_session(AudioProtocolFailure& failure){
    failure={};if(!state_){failure.kind=AudioProtocolError::moved_bridge;return false;}
    std::lock_guard<std::mutex> lock(state_->mutex);
    if(!state_->session.close(failure.bank)){failure.kind=AudioProtocolError::bank_failure;return false;}
    return true;
}
bool AudioBankProtocol::discard_unprepared(const AudioBankHandle& handle,AudioProtocolFailure& failure) {
    failure={};
    if(!state_){failure.kind=AudioProtocolError::moved_bridge;return false;}
    std::lock_guard<std::mutex> lock(state_->mutex);
    if(!state_->session.unload_bank(handle,failure.bank)){failure.kind=AudioProtocolError::bank_failure;return false;}
    return true;
}
bool AudioBankProtocol::bind_unprepared(BankWireToken token,const AudioBankHandle& handle,AudioProtocolFailure& failure) {
    failure={};
    if(!state_){failure.kind=AudioProtocolError::moved_bridge;return false;}
    if(!token.value){failure.kind=AudioProtocolError::unsupported_null_bank_token;return false;}
    std::lock_guard<std::mutex> lock(state_->mutex);
    if(!state_->session.load_state(handle,failure.bank)){failure.kind=AudioProtocolError::bank_failure;return false;}
    try {
        if(!state_->bindings.emplace(token.value,handle).second){failure.kind=AudioProtocolError::duplicate_bank_token;return false;}
        return true;
    } catch(const std::bad_alloc&){failure.kind=AudioProtocolError::allocation_failed;}
      catch(const std::length_error&){failure.kind=AudioProtocolError::host_capacity;}
    return false;
}
std::optional<AudioBankHandle> AudioBankProtocol::lookup(BankWireToken token,AudioProtocolFailure& failure) const {
    failure={};
    if(!state_){failure.kind=AudioProtocolError::moved_bridge;return std::nullopt;}
    if(!token.value){failure.kind=AudioProtocolError::unsupported_null_bank_token;return std::nullopt;}
    std::lock_guard<std::mutex> lock(state_->mutex);
    auto it=state_->bindings.find(token.value);
    if(it==state_->bindings.end()){failure.kind=AudioProtocolError::unknown_bank_token;return std::nullopt;}
    if(!state_->session.load_state(it->second,failure.bank)){failure.kind=AudioProtocolError::bank_failure;return std::nullopt;}
    return it->second;
}
std::optional<AudioResource> AudioBankProtocol::resolve_play_kind1(
    const AudioBankCommand& command,std::size_t index,AudioProtocolFailure& failure) const {
    failure={};
    if(!is_play(command.opcode)){failure.kind=AudioProtocolError::wrong_command;return std::nullopt;}
    if(!state_){failure.kind=AudioProtocolError::moved_bridge;return std::nullopt;}
    if(!command.bank_token.value){failure.kind=AudioProtocolError::unsupported_null_bank_token;return std::nullopt;}
    std::lock_guard<std::mutex> lock(state_->mutex);
    auto it=state_->bindings.find(command.bank_token.value);
    if(it==state_->bindings.end()){failure.kind=AudioProtocolError::unknown_bank_token;return std::nullopt;}
    auto resource=state_->session.resolve_kind1(it->second,command.selector,index,failure.bank);
    if(!resource)failure.kind=AudioProtocolError::bank_failure;
    return resource;
}
bool AudioBankProtocol::unload(const AudioBankCommand& command,AudioProtocolFailure& failure) {
    failure={};
    if(command.opcode!=AudioBankOpcode::unload){failure.kind=AudioProtocolError::wrong_command;return false;}
    if(!state_){failure.kind=AudioProtocolError::moved_bridge;return false;}
    if(!command.bank_token.value){failure.kind=AudioProtocolError::unsupported_null_bank_token;return false;}
    std::lock_guard<std::mutex> lock(state_->mutex);
    auto it=state_->bindings.find(command.bank_token.value);
    if(it==state_->bindings.end()){failure.kind=AudioProtocolError::unknown_bank_token;return false;}
    if(!state_->session.unload_bank(it->second,failure.bank)){failure.kind=AudioProtocolError::bank_failure;return false;}
    return true;
}
std::shared_ptr<const PreparedProgram> AudioBankProtocol::prepare_program(
    const AudioBankCommand& command,PreparationFailure& preparation,AudioProtocolFailure& failure) {
    preparation={};failure={};
    if(!is_play(command.opcode)){failure.kind=AudioProtocolError::wrong_command;return {};}
    if(!state_){failure.kind=AudioProtocolError::moved_bridge;return {};}
    if(!command.bank_token.value){failure.kind=AudioProtocolError::unsupported_null_bank_token;return {};}
    AudioBankHandle handle;
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        const auto found=state_->bindings.find(command.bank_token.value);
        if(found==state_->bindings.end()){failure.kind=AudioProtocolError::unknown_bank_token;return {};}
        handle=found->second;
    }
    auto program=state_->session.prepare_program(handle,command.selector,preparation,failure.bank);
    if(!program)failure.kind=AudioProtocolError::bank_failure;
    return program;
}
} // namespace mka::sound
