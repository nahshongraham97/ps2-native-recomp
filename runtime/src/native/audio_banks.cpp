#include "mka/audio_banks.hpp"
#include <mutex>
#include <new>
#include <stdexcept>
#include <unordered_map>

namespace mka::sound {
struct AudioBankHandle::Identity {
    std::weak_ptr<const void> session;
    bool unloaded = false; // Access only under the owning session mutex.
};
struct AudioBankSession::State {
    explicit State(SoundArchive archive_value) : archive(std::move(archive_value)) {}
    struct Entry { std::shared_ptr<AudioBankHandle::Identity> identity; BankAsset bank; std::shared_ptr<SoundArchiveBacking> backing; std::optional<PreparedBank> prepared; };
    const std::shared_ptr<const void> cookie = std::make_shared<const int>(0);
    std::optional<SoundArchive> archive;
    std::optional<StreamSource> stream;
    std::unordered_map<const AudioBankHandle::Identity*, Entry> banks;
    mutable std::mutex mutex;
    bool closed = false;
    ~State() { for(auto& entry:banks)if(entry.second.prepared)entry.second.prepared->unload(); }
};
std::optional<AudioBankSession> AudioBankSession::open(
    const std::filesystem::path& ms2, const std::optional<std::filesystem::path>& ms4,
    AudioBanksFailure& failure) {
    failure = {};
    try {
        auto archive = SoundArchive::open(ms2, failure.archive);
        if (!archive) { failure.kind = AudioBanksError::archive_failed; return std::nullopt; }
        auto state = std::make_shared<State>(std::move(*archive));
        if (ms4) {
            state->stream = StreamSource::open_associated_ms4(*ms4, failure.stream);
            if (!state->stream) { failure.kind = AudioBanksError::stream_read_failed; return std::nullopt; }
        }
        return AudioBankSession(std::move(state));
    } catch (const std::bad_alloc&) { failure.kind = AudioBanksError::allocation_failed; }
      catch (const std::length_error&) { failure.kind = AudioBanksError::host_capacity; }
    return std::nullopt;
}
const BankAsset* AudioBankSession::checked_bank(const AudioBankHandle& handle, AudioBanksFailure& failure) const {
    if (state_->closed) { failure.kind = AudioBanksError::closed_session; return nullptr; }
    if (handle.moved_) { failure.kind = AudioBanksError::moved_handle; return nullptr; }
    if (!handle.identity_) { failure.kind = AudioBanksError::unknown_handle; return nullptr; }
    auto owner = handle.identity_->session.lock();
    if (!owner) { failure.kind = AudioBanksError::expired_session; return nullptr; }
    if (owner.get() != state_->cookie.get()) { failure.kind = AudioBanksError::foreign_session; return nullptr; }
    if (handle.identity_->unloaded) { failure.kind = AudioBanksError::unloaded_handle; return nullptr; }
    auto it = state_->banks.find(handle.identity_.get());
    if (it == state_->banks.end()) { failure.kind = AudioBanksError::unknown_handle; return nullptr; }
    return &it->second.bank;
}
std::optional<AudioBankHandle> AudioBankSession::load_bank(std::string_view name, AudioBanksFailure& failure) {
    failure = {};
    if (!state_) { failure.kind = AudioBanksError::moved_session; return std::nullopt; }
    std::lock_guard<std::mutex> lock(state_->mutex);
    if (state_->closed) { failure.kind = AudioBanksError::closed_session; return std::nullopt; }
    try {
        auto source = state_->archive->read_bank(name, failure.archive);
        if (!source) { failure.kind = AudioBanksError::archive_failed; return std::nullopt; }
        auto backing=source->backing;
        auto bank = BankAsset::parse(std::move(*source), failure.bank);
        if (!bank) { failure.kind = AudioBanksError::bank_parse_failed; return std::nullopt; }
        auto identity = std::make_shared<AudioBankHandle::Identity>();
        identity->session = state_->cookie;
        state_->banks.emplace(identity.get(), State::Entry{identity, std::move(*bank),std::move(backing),std::nullopt});
        return AudioBankHandle(std::move(identity));
    } catch (const std::bad_alloc&) { failure.kind = AudioBanksError::allocation_failed; }
      catch (const std::length_error&) { failure.kind = AudioBanksError::host_capacity; }
    return std::nullopt;
}
std::optional<AudioBankLoadState> AudioBankSession::load_state(
    const AudioBankHandle& handle, AudioBanksFailure& failure) const {
    failure = {};
    if (!state_) { failure.kind = AudioBanksError::moved_session; return std::nullopt; }
    std::lock_guard<std::mutex> lock(state_->mutex);
    if (!checked_bank(handle, failure)) return std::nullopt;
    const auto& prepared=state_->banks.at(handle.identity_.get()).prepared;
    if(prepared && !prepared->active()){failure.kind=AudioBanksError::preparation_revoked;return std::nullopt;}
    return prepared ? AudioBankLoadState::eager_prepared_source_deferred_allowed : AudioBankLoadState::loaded_unprepared;
}
std::optional<PreparedBank> AudioBankSession::prepare_bank(const AudioBankHandle& handle,
    BankPreparationReport& report,PreparationFailure& preparation,AudioBanksFailure& failure) {
    failure={};preparation={};report={};
    if(!state_){failure.kind=AudioBanksError::moved_session;return std::nullopt;}
    std::lock_guard<std::mutex> lock(state_->mutex);
    const auto* bank=checked_bank(handle,failure);if(!bank)return std::nullopt;
    auto it=state_->banks.find(handle.identity_.get());
    if(it->second.prepared){
        if(!it->second.prepared->active()){failure.kind=AudioBanksError::preparation_revoked;preparation.kind=PreparationError::unloaded;}
        else preparation.kind=PreparationError::already_prepared;
        return std::nullopt;
    }
    std::optional<PreparedBank> prepared;
    try {
        BankAssetSource source{bank->origin(),bank->bank().raw_bytes(),it->second.backing};
        prepared=PreparedBank::prepare(std::move(source),report,preparation);
    }catch(const std::bad_alloc&){preparation.kind=PreparationError::allocation_failed;}
     catch(const std::length_error&){preparation.kind=PreparationError::host_capacity;}
    if(!prepared){
        // Source loader frees the whole failed candidate after eager error counting.
        // Unrelated registry entries are never changed, including allocation failure.
        handle.identity_->unloaded=true;state_->banks.erase(it);return std::nullopt;
    }
    it->second.prepared=*prepared;
    return prepared;
}
std::optional<AudioResource> AudioBankSession::resolve(
    const BankAsset& bank, std::int32_t selector, std::size_t index, AudioBanksFailure& failure) {
    try {
        auto program = bank.bank().lookup_commands(selector);
        if (program.status != LookupStatus::found) {
            failure.kind = program.status == LookupStatus::absent_selector ? AudioBanksError::absent_selector :
                program.status == LookupStatus::absent_program ? AudioBanksError::absent_program : AudioBanksError::malformed_bank;
            return std::nullopt;
        }
        if (program.commands.empty()) { failure.kind = AudioBanksError::empty_program; return std::nullopt; }
        if (index >= program.commands.size()) { failure.kind = AudioBanksError::command_out_of_bounds; return std::nullopt; }
        auto command = program.commands[index];
        if (command.kind() != 1) { failure.kind = AudioBanksError::unsupported_command_kind; return std::nullopt; }
        if (!(bank.bank().raw_bytes()[4] & 0x10)) {
            failure.kind = AudioBanksError::unsupported_named_command_route; return std::nullopt;
        }
        auto key = command.resource_bits();
        if (key == 0xffffffffu) key = 0; // Proven serialized relocation, not fallback.
        auto resource = bank.bank().lookup_resource(key);
        if (resource.status != ResourceLookupStatus::found) { failure.kind = AudioBanksError::absent_resource; return std::nullopt; }
        return AudioResource{bank.origin(), program.program_index, index, command, key, resource};
    } catch (const std::bad_alloc&) { failure.kind = AudioBanksError::allocation_failed; }
      catch (const std::length_error&) { failure.kind = AudioBanksError::host_capacity; }
    return std::nullopt;
}
std::optional<AudioResource> AudioBankSession::resolve_kind1(
    const AudioBankHandle& handle, std::int32_t selector, std::size_t index, AudioBanksFailure& failure) const {
    failure = {};
    if (!state_) { failure.kind = AudioBanksError::moved_session; return std::nullopt; }
    std::lock_guard<std::mutex> lock(state_->mutex);
    const auto* bank = checked_bank(handle, failure);
    return bank ? resolve(*bank, selector, index, failure) : std::nullopt;
}
std::optional<BankSample> AudioBankSession::read_normal(
    const AudioBankHandle& handle, std::int32_t selector, std::size_t index, AudioBanksFailure& failure) const {
    failure = {};
    if (!state_) { failure.kind = AudioBanksError::moved_session; return std::nullopt; }
    std::lock_guard<std::mutex> lock(state_->mutex);
    const auto* bank = checked_bank(handle, failure);
    if (!bank || !resolve(*bank, selector, index, failure)) return std::nullopt;
    auto sample = bank->read_sample(selector, index, failure.bank);
    if (!sample) failure.kind = AudioBanksError::normal_read_failed;
    return sample;
}
std::optional<StreamSourceGroup> AudioBankSession::read_stream_group(
    const AudioBankHandle& handle, std::int32_t selector, std::size_t index, std::uint64_t ordinal,
    AudioBanksFailure& failure) const {
    failure = {};
    if (!state_) { failure.kind = AudioBanksError::moved_session; return std::nullopt; }
    std::lock_guard<std::mutex> lock(state_->mutex);
    const auto* bank = checked_bank(handle, failure);
    if (!bank) return std::nullopt;
    auto resource = resolve(*bank, selector, index, failure);
    if (!resource) return std::nullopt;
    if (!state_->stream) { failure.kind = AudioBanksError::no_associated_ms4; return std::nullopt; }
    auto group = state_->stream->read_group(resource->resource, ordinal, failure.stream);
    if (!group) failure.kind = AudioBanksError::stream_read_failed;
    return group;
}
bool AudioBankSession::unload_bank(const AudioBankHandle& handle, AudioBanksFailure& failure) {
    failure = {};
    if (!state_) { failure.kind = AudioBanksError::moved_session; return false; }
    std::lock_guard<std::mutex> lock(state_->mutex);
    if (!checked_bank(handle, failure)) return false;
    handle.identity_->unloaded = true;
    auto& entry=state_->banks.at(handle.identity_.get());
    if(entry.prepared)entry.prepared->unload();
    state_->banks.erase(handle.identity_.get());
    return true;
}
bool AudioBankSession::close(AudioBanksFailure& failure) {
    failure = {};
    if (!state_) { failure.kind = AudioBanksError::moved_session; return false; }
    std::lock_guard<std::mutex> lock(state_->mutex);
    if (state_->closed) { failure.kind = AudioBanksError::closed_session; return false; }
    state_->closed = true;
    for (auto& entry : state_->banks) { entry.second.identity->unloaded = true; if(entry.second.prepared)entry.second.prepared->unload(); }
    state_->banks.clear();
    state_->stream.reset();
    state_->archive.reset();
    return true;
}
} // namespace mka::sound
