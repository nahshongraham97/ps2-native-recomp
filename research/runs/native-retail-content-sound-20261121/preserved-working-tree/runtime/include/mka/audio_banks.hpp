#pragma once
#include "mka/bank_asset.hpp"
#include "mka/stream_source.hpp"
#include "mka/bank_preparation.hpp"
#include "mka/runtime_preparation.hpp"
#include <memory>
#include <optional>

namespace mka::sound {
enum class AudioBanksError {
    none, moved_session, closed_session, unknown_handle, moved_handle,
    expired_session, foreign_session, unloaded_handle, archive_failed,
    bank_parse_failed, absent_selector, absent_program, malformed_bank,
    empty_program, command_out_of_bounds, unsupported_command_kind,
    unsupported_named_command_route, absent_resource, normal_read_failed,
    no_associated_ms4, stream_read_failed, allocation_failed, host_capacity, preparation_revoked, bank_not_prepared, resource_access
};
struct AudioBanksFailure {
    AudioBanksError kind = AudioBanksError::none;
    AudioResourceFailure resource;
    ArchiveFailure archive;
    BankAssetFailure bank;
    StreamFailure stream;
};
enum class AudioBankLoadState { loaded_unprepared, eager_prepared_source_deferred_allowed };
// Host object identity only. An external bridge must explicitly associate its
// wire token with this handle; no address/index/low16 encoding is provided.
class AudioBankHandle {
public:
    AudioBankHandle() = default;
    AudioBankHandle(const AudioBankHandle&) = default;
    AudioBankHandle& operator=(const AudioBankHandle&) = default;
    AudioBankHandle(AudioBankHandle&& other) noexcept
        : identity_(std::move(other.identity_)), moved_(other.moved_) { other.moved_ = true; }
    AudioBankHandle& operator=(AudioBankHandle&& other) noexcept {
        if (this != &other) { identity_ = std::move(other.identity_); moved_ = other.moved_; other.moved_ = true; }
        return *this;
    }
private:
    struct Identity;
    explicit AudioBankHandle(std::shared_ptr<Identity> identity) : identity_(std::move(identity)) {}
    std::shared_ptr<Identity> identity_;
    bool moved_ = false;
    friend class AudioBankSession;
};
struct AudioResource {
    ArchiveRecord bank_origin;
    std::size_t program_index = 0, command_index = 0;
    CommandRecord command;
    std::uint32_t resolved_key = 0;
    ResourceLookup resource;
};
// Host ownership/data/preparation service and owned fresh-data work admission.
// No playback scheduler, callbacks, voices or playback-ready claim.
// Admission is checked against unload/close. Admitted reads retain immutable
// backing owners while local registry locks are released; new work is revoked
// on unload. Returned data owns its bytes. No playback job is modeled here.
class AudioBankSession {
public:
    AudioBankSession(const AudioBankSession&) = delete;
    AudioBankSession& operator=(const AudioBankSession&) = delete;
    AudioBankSession(AudioBankSession&&) noexcept = default;
    AudioBankSession& operator=(AudioBankSession&&) noexcept = default;
    // MS4 association is trusted explicit caller input, never suffix inference.
    // Omitting MS4 permits normal reads only; it never selects another file.
    static std::optional<AudioBankSession> open(
        const std::filesystem::path& ms2,
        const std::optional<std::filesystem::path>& associated_ms4, AudioBanksFailure&);
    static std::optional<AudioBankSession> open(const std::filesystem::path&,
        const std::optional<std::filesystem::path>&, std::shared_ptr<AudioResourceDomain>, AudioBanksFailure&);
    static std::optional<AudioBankSession> adopt(content::NativeContentFile&&,
        std::optional<content::NativeContentFile>&&, AudioBanksFailure&);
    static std::optional<AudioBankSession> adopt(content::NativeContentFile&&,
        std::optional<content::NativeContentFile>&&,
        std::shared_ptr<AudioResourceDomain>, AudioBanksFailure&);
    std::shared_ptr<AudioResourceDomain> resource_domain() const noexcept;
    std::optional<AudioBankHandle> load_bank(std::string_view name, AudioBanksFailure&);
    std::optional<AudioBankLoadState> load_state(const AudioBankHandle&, AudioBanksFailure&) const;
    // Failed preparation invalidates ONLY this bank handle, matching candidate cleanup.
    // No wire callback/status is emitted; source-deferred programs stay deferred.
    std::optional<PreparedBank> prepare_bank(const AudioBankHandle&, BankPreparationReport&, PreparationFailure&, AudioBanksFailure&);
    // Retrieve an eager program or perform the source-selected deferred normal
    // work. This is preparation, not a duplicate bank-load completion/playback.
    std::shared_ptr<const PreparedProgram> prepare_program(const AudioBankHandle&,
        std::int32_t selector,PreparationFailure&,AudioBanksFailure&);
    // Exact source-deferred data admission; no resource work or callback begins.
    std::shared_ptr<PreparedProgramWork> admit_deferred_normal(const AudioBankHandle&,
        std::int32_t selector,PreparationFailure&,AudioBanksFailure&);
    std::shared_ptr<RuntimeProgramWork> admit_runtime_program(const AudioBankHandle&,
        std::int32_t selector,PreparationFailure&,AudioBanksFailure&);
    std::unique_ptr<RuntimeProgramReference> acquire_runtime_program(const AudioBankHandle&,
        std::int32_t selector,PreparationFailure&,AudioBanksFailure&);
    std::optional<AudioResource> resolve_kind1(const AudioBankHandle&, std::int32_t selector,
                                              std::size_t command_index, AudioBanksFailure&) const;
    std::optional<BankSample> read_normal(const AudioBankHandle&, std::int32_t selector,
                                         std::size_t command_index, AudioBanksFailure&) const;
    std::optional<StreamSourceGroup> read_stream_group(const AudioBankHandle&, std::int32_t selector,
        std::size_t command_index, std::uint64_t group_ordinal, AudioBanksFailure&) const;
    bool unload_bank(const AudioBankHandle&, AudioBanksFailure&);
    bool close(AudioBanksFailure&);
private:
    struct State;
    explicit AudioBankSession(std::shared_ptr<State> state) : state_(std::move(state)) {}
    // Caller holds State::mutex. These helpers never execute command semantics.
    const BankAsset* checked_bank(const AudioBankHandle&, AudioBanksFailure&) const;
    static std::optional<AudioResource> resolve(const BankAsset&, std::int32_t,
                                               std::size_t, AudioBanksFailure&);
    std::shared_ptr<State> state_;
};
} // namespace mka::sound
