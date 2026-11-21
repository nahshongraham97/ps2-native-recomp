#include "mka/audio_banks.hpp"
#include <mutex>
#include <condition_variable>
#include <new>
#include <stdexcept>
#include <unordered_map>

namespace mka::sound {
struct AudioBankHandle::Identity {
    std::weak_ptr<const void> session;
    bool unloaded = false; // Access only under the owning session mutex.
};
struct AudioBankSession::State {
    explicit State(SoundArchive archive_value) : archive(std::make_shared<const SoundArchive>(std::move(archive_value))) {}
    struct Entry { std::shared_ptr<AudioBankHandle::Identity> identity; std::shared_ptr<const BankAsset> bank; std::shared_ptr<SoundArchiveBacking> backing; std::optional<PreparedBank> prepared; bool preparing = false; };
    const std::shared_ptr<const void> cookie = std::make_shared<const int>(0);
    std::shared_ptr<AudioResourceDomain> domain;
    std::shared_ptr<const SoundArchive> archive;
    std::optional<StreamSource> stream;
    std::unordered_map<const AudioBankHandle::Identity*, Entry> banks;
    mutable std::mutex mutex;
    std::condition_variable changed;
    bool closed = false;
    ~State() { for(auto& entry:banks)if(entry.second.prepared)entry.second.prepared->unload(); }
};
std::optional<AudioBankSession> AudioBankSession::open(
    const std::filesystem::path& ms2, const std::optional<std::filesystem::path>& ms4,
    AudioBanksFailure& failure) {
    failure = {};
    auto domain = AudioResourceDomain::create(failure.resource);
    if (!domain) { failure.kind = AudioBanksError::resource_access; return std::nullopt; }
    return open(ms2, ms4, std::move(domain), failure);
}
std::shared_ptr<AudioResourceDomain> AudioBankSession::resource_domain() const noexcept {
    return state_ ? state_->domain : nullptr;
}
std::optional<AudioBankSession> AudioBankSession::open(
    const std::filesystem::path& ms2, const std::optional<std::filesystem::path>& ms4,
    std::shared_ptr<AudioResourceDomain> domain, AudioBanksFailure& failure) {
    failure = {};
    if (!domain) { failure.kind = AudioBanksError::resource_access; failure.resource.kind = AudioResourceError::invalid_owner; return std::nullopt; }
    try {
        auto archive = SoundArchive::open(ms2, domain, failure.archive);
        if (!archive) { failure.kind = AudioBanksError::archive_failed; return std::nullopt; }
        auto state = std::make_shared<State>(std::move(*archive));
        state->domain = domain;
        if (ms4) {
            state->stream = StreamSource::open_associated_ms4(*ms4, domain, failure.stream);
            if (!state->stream) { failure.kind = AudioBanksError::stream_read_failed; return std::nullopt; }
        }
        return AudioBankSession(std::move(state));
    } catch (const std::bad_alloc&) { failure.kind = AudioBanksError::allocation_failed; }
      catch (const std::length_error&) { failure.kind = AudioBanksError::host_capacity; }
    return std::nullopt;
}
std::optional<AudioBankSession> AudioBankSession::adopt(
    content::NativeContentFile&& ms2, std::optional<content::NativeContentFile>&& ms4,
    AudioBanksFailure& failure) {
    failure = {};
    if (!ms2.valid()) {
        failure.kind = AudioBanksError::archive_failed;
        failure.archive.kind = ArchiveError::invalid_content_file;
        failure.archive.content.kind = content::ContentVolumeError::moved_file;
        return std::nullopt;
    }
    if (ms4 && !ms4->valid()) {
        failure.kind = AudioBanksError::stream_read_failed;
        failure.stream.kind = StreamError::backing_failure;
        failure.stream.backing.kind = ArchiveError::invalid_content_file;
        failure.stream.backing.content.kind = content::ContentVolumeError::moved_file;
        return std::nullopt;
    }
    auto domain = AudioResourceDomain::create(failure.resource);
    if (!domain) { failure.kind = AudioBanksError::resource_access; return std::nullopt; }
    return adopt(std::move(ms2), std::move(ms4), std::move(domain), failure);
}
std::optional<AudioBankSession> AudioBankSession::adopt(
    content::NativeContentFile&& ms2, std::optional<content::NativeContentFile>&& ms4,
    std::shared_ptr<AudioResourceDomain> domain, AudioBanksFailure& failure) {
    failure = {};
    if (!domain) { failure.kind = AudioBanksError::resource_access; failure.resource.kind = AudioResourceError::invalid_owner; return std::nullopt; }
    if (!ms2.valid()) { failure.kind = AudioBanksError::archive_failed; failure.archive.kind = ArchiveError::invalid_content_file; failure.archive.content.kind = content::ContentVolumeError::moved_file; return std::nullopt; }
    if (ms4 && !ms4->valid()) { failure.kind = AudioBanksError::stream_read_failed; failure.stream.kind = StreamError::backing_failure; failure.stream.backing.kind = ArchiveError::invalid_content_file; failure.stream.backing.content.kind = content::ContentVolumeError::moved_file; return std::nullopt; }
    try {
        auto archive = SoundArchive::adopt(std::move(ms2), domain, failure.archive);
        if (!archive) { failure.kind = AudioBanksError::archive_failed; return std::nullopt; }
        auto state = std::make_shared<State>(std::move(*archive));
        state->domain = domain;
        if (ms4) {
            state->stream = StreamSource::adopt_associated_ms4(std::move(*ms4), domain, failure.stream);
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
    return it->second.bank.get();
}
std::optional<AudioBankHandle> AudioBankSession::load_bank(std::string_view name, AudioBanksFailure& failure) {
    failure = {};
    if (!state_) { failure.kind = AudioBanksError::moved_session; return std::nullopt; }
    std::unique_lock<std::mutex> lock(state_->mutex);
    if (state_->closed) { failure.kind = AudioBanksError::closed_session; return std::nullopt; }
    auto archive = state_->archive;
    lock.unlock();
    try {
        auto source = archive->read_bank(name, failure.archive);
        if (!source) { failure.kind = AudioBanksError::archive_failed; return std::nullopt; }
        auto backing=source->backing;
        auto bank = BankAsset::parse(std::move(*source), failure.bank);
        if (!bank) { failure.kind = AudioBanksError::bank_parse_failed; return std::nullopt; }
        auto identity = std::make_shared<AudioBankHandle::Identity>();
        identity->session = state_->cookie;
        auto owned_bank = std::make_shared<const BankAsset>(std::move(*bank));
        lock.lock();
        if (state_->closed) { failure.kind = AudioBanksError::closed_session; return std::nullopt; }
        state_->banks.emplace(identity.get(), State::Entry{identity, std::move(owned_bank),std::move(backing),std::nullopt});
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
    std::unique_lock<std::mutex> lock(state_->mutex);
    const auto* bank=checked_bank(handle,failure);if(!bank)return std::nullopt;
    auto it=state_->banks.find(handle.identity_.get());
    if(it->second.prepared){
        if(!it->second.prepared->active()){failure.kind=AudioBanksError::preparation_revoked;preparation.kind=PreparationError::unloaded;}
        else preparation.kind=PreparationError::already_prepared;
        return std::nullopt;
    }
    while (it->second.preparing) {
        state_->changed.wait(lock);
        bank = checked_bank(handle, failure); if (!bank) return std::nullopt;
        it = state_->banks.find(handle.identity_.get());
        if (it->second.prepared) { preparation.kind = PreparationError::already_prepared; return std::nullopt; }
    }
    std::optional<PreparedBank> prepared;
    bool admitted = false;
    auto retire_admission = [&] {
        if (!lock.owns_lock()) lock.lock();
        if (admitted) {
            auto current = state_->banks.find(handle.identity_.get());
            if (current != state_->banks.end()) current->second.preparing = false;
            state_->changed.notify_all();
            admitted = false;
        }
    };
    try {
        BankAssetSource source{bank->origin(),bank->bank().raw_bytes(),it->second.backing};
        it->second.preparing = true;
        admitted = true;
        lock.unlock();
        prepared=PreparedBank::prepare(std::move(source),report,preparation);
    }catch(const std::bad_alloc&){preparation.kind=PreparationError::allocation_failed;}
     catch(const std::length_error&){preparation.kind=PreparationError::host_capacity;}
     catch (...) {
         retire_admission();
         // Source failure retires the complete candidate, not only its busy
         // state. Never leave an unbound bank/backing hidden in the session.
         auto failed = state_->banks.find(handle.identity_.get());
         if (failed != state_->banks.end()) {
             handle.identity_->unloaded = true;
             if (failed->second.prepared) failed->second.prepared->unload();
             state_->banks.erase(failed);
             state_->changed.notify_all();
         }
         throw;
     }
    retire_admission();
    it = state_->banks.find(handle.identity_.get());
    if (state_->closed || it == state_->banks.end() || handle.identity_->unloaded) {
        if (prepared) prepared->unload();
        failure.kind = AudioBanksError::preparation_revoked;
        return std::nullopt;
    }
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
std::shared_ptr<const PreparedProgram> AudioBankSession::prepare_program(
    const AudioBankHandle& handle,std::int32_t selector,PreparationFailure& preparation,AudioBanksFailure& failure) {
    preparation={};failure={};
    if(!state_){failure.kind=AudioBanksError::moved_session;return {};}
    std::unique_lock<std::mutex> lock(state_->mutex);
    if(!checked_bank(handle,failure))return {};
    auto prepared=state_->banks.at(handle.identity_.get()).prepared;
    if(!prepared){failure.kind=AudioBanksError::bank_not_prepared;return {};}
    if(!prepared->active()){failure.kind=AudioBanksError::preparation_revoked;preparation.kind=PreparationError::unloaded;return {};}
    lock.unlock();
    auto program=prepared->eager_program(selector,preparation);
    if(!program&&preparation.kind==PreparationError::source_deferred)
        program=prepared->prepare_deferred_normal(selector,preparation);
    return program;
}
std::shared_ptr<PreparedProgramWork> AudioBankSession::admit_deferred_normal(
    const AudioBankHandle& handle,std::int32_t selector,PreparationFailure& preparation,AudioBanksFailure& failure) {
    preparation={};failure={};
    if(!state_){failure.kind=AudioBanksError::moved_session;return {};}
    std::unique_lock<std::mutex> lock(state_->mutex);
    if(!checked_bank(handle,failure))return {};
    auto prepared=state_->banks.at(handle.identity_.get()).prepared;
    if(!prepared){failure.kind=AudioBanksError::bank_not_prepared;return {};}
    lock.unlock();
    return prepared->admit_deferred_normal(selector,preparation);
}

std::shared_ptr<RuntimeProgramWork> AudioBankSession::admit_runtime_program(
    const AudioBankHandle& handle,std::int32_t selector,PreparationFailure& preparation,AudioBanksFailure& failure) {
    preparation={};failure={};
    if(!state_){failure.kind=AudioBanksError::moved_session;return {};}
    std::unique_lock<std::mutex> lock(state_->mutex);
    if(!checked_bank(handle,failure))return {};
    auto prepared=state_->banks.at(handle.identity_.get()).prepared;
    if(!prepared){failure.kind=AudioBanksError::bank_not_prepared;return {};}
    lock.unlock();
    return prepared->admit_runtime_program(selector,preparation);
}

std::unique_ptr<RuntimeProgramReference> AudioBankSession::acquire_runtime_program(
    const AudioBankHandle& handle,std::int32_t selector,PreparationFailure& preparation,AudioBanksFailure& failure) {
    preparation={};failure={};
    if(!state_){failure.kind=AudioBanksError::moved_session;return {};}
    std::unique_lock<std::mutex> lock(state_->mutex);
    if(!checked_bank(handle,failure))return {};
    auto prepared=state_->banks.at(handle.identity_.get()).prepared;
    if(!prepared){failure.kind=AudioBanksError::bank_not_prepared;return {};}
    lock.unlock();
    return prepared->acquire_runtime_program(selector,preparation);
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
    std::unique_lock<std::mutex> lock(state_->mutex);
    const auto* checked = checked_bank(handle, failure);
    if (!checked || !resolve(*checked, selector, index, failure)) return std::nullopt;
    auto bank = state_->banks.at(handle.identity_.get()).bank;
    lock.unlock();
    auto sample = bank->read_sample(selector, index, failure.bank);
    if (!sample) failure.kind = AudioBanksError::normal_read_failed;
    return sample;
}
std::optional<StreamSourceGroup> AudioBankSession::read_stream_group(
    const AudioBankHandle& handle, std::int32_t selector, std::size_t index, std::uint64_t ordinal,
    AudioBanksFailure& failure) const {
    failure = {};
    if (!state_) { failure.kind = AudioBanksError::moved_session; return std::nullopt; }
    std::unique_lock<std::mutex> lock(state_->mutex);
    const auto* bank = checked_bank(handle, failure);
    if (!bank) return std::nullopt;
    auto resource = resolve(*bank, selector, index, failure);
    if (!resource) return std::nullopt;
    if (!state_->stream) { failure.kind = AudioBanksError::no_associated_ms4; return std::nullopt; }
    auto stream = *state_->stream;
    lock.unlock();
    auto group = stream.read_group(resource->resource, ordinal, failure.stream);
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
    state_->changed.notify_all();
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
    state_->changed.notify_all();
    return true;
}
} // namespace mka::sound
