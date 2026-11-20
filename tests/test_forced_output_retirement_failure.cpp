#include "mka/playback_owner.hpp"

#include <cstdio>
#include <cstdlib>
#include <new>

using namespace mka::sound;
namespace {
thread_local int fail_after = -1;
constexpr std::uint32_t bank_token = 0xcf127321;
void check(bool ok, const char* reason) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", reason);
        std::abort();
    }
}
AudioBankCommand command(AudioBankOpcode op) {
    AudioBankCommand c;
    c.opcode = op;
    c.bank_token = {bank_token};
    return c;
}
NativePlayParameters parameters(std::int32_t role) {
    return {initial_program_control_factors(), initial_program_control_factors(),
            {{1,14336,false},{1,-16384,true},{1,14336,false},{1,-16384,false},2048},
            {1000}, 3, 0, {role, 0}};
}
}

extern "C" void* real_new(std::size_t) __asm__("__real__Znwy");
extern "C" void* wrapped_new(std::size_t) __asm__("__wrap__Znwy");
extern "C" void* wrapped_new(std::size_t size) {
    if (fail_after == 0) throw std::bad_alloc();
    if (fail_after > 0) --fail_after;
    return real_new(size);
}

namespace {
struct Fixture {
    std::optional<NativePlaybackOwner> owner;
    NativeSoundContext context;
    explicit Fixture(const char* path) {
        AudioBanksFailure bf;
        auto session = AudioBankSession::open(path, std::nullopt, bf);
        check(bool(session), "actual session");
        AudioProtocolFailure pf;
        auto protocol = AudioBankProtocol::create(std::move(*session), pf);
        check(bool(protocol), "actual protocol");
        BankServiceFailure sf;
        auto service = AudioBankService::create(std::move(*protocol), sf);
        check(bool(service), "actual Service");
        PlaybackFailure f;
        owner = NativePlaybackOwner::create(std::move(*service), 48000, f);
        check(bool(owner), "actual Owner");
        auto made = owner->create_sound_context({2}, f);
        check(bool(made), "actual context");
        context = *made;
        auto load = command(AudioBankOpcode::load_sync);
        load.pathname = "announcr";
        auto result = owner->load_sync(load, {bank_token}, f);
        check(result && result->bank.value == bank_token, "actual ANNOUNCR bank");
    }
    NativePlayHandle start(std::int32_t role) {
        PlaybackFailure f;
        auto play = owner->play(context, command(AudioBankOpcode::play), parameters(role), f);
        check(play && owner->tick(*play, {1000}, f), "actual active voice");
        return *play;
    }
    void unchanged(const NativePlayHandle& handle, const PlaybackObservation& before) {
        PlaybackFailure f;
        auto after = owner->observe(handle, f);
        check(after && after->output_role == before.output_role && after->output_owned &&
              after->dispatch_live && after->private_copy_bound && !after->explicitly_stopped &&
              after->active_voices == before.active_voices &&
              after->first_voice_source_index == before.first_voice_source_index &&
              after->command_index == before.command_index,
              "allocation failure preserves play identity, slot, executor and voice position");
    }
    void no_partial_groups(std::size_t expected_programs) {
        PlaybackFailure f;
        auto state = owner->observe_sound_context(context, f);
        check(state && state->programs == expected_programs && state->retired_groups == 0 &&
              state->active_voices == expected_programs && state->pending_preparations == 0,
              "failed preflight publishes no partial retired group");
        check(!owner->unload(command(AudioBankOpcode::unload), f) &&
              f.kind == PlaybackError::active_playback, "live admission survives allocation failure");
    }
    void drain() {
        PlaybackFailure f;
        check(owner->unload(command(AudioBankOpcode::unload), f),
              "actual completed retirement releases all bank keepers");
        check(!owner->close_when_idle(f) && f.kind == PlaybackError::active_playback,
              "live retired software tails still guard close");
        check(bool(owner->mix(32, f)) && owner->release_sound_context(context, f) &&
              owner->close_when_idle(f), "retired tails drain and shutdown succeeds");
    }
};
bool allocation_failed(const PlaybackFailure& f) {
    return f.kind == PlaybackError::allocation_failed ||
           (f.kind == PlaybackError::copy_failed && f.copy.kind == ProgramCopyError::allocation_failed);
}
unsigned single_failpoints(const char* path) {
    Fixture x(path);
    auto handle = x.start(0);
    PlaybackFailure f;
    check(bool(x.owner->mix(256, f)), "warm real output before failpoints");
    auto before = x.owner->observe(handle, f);
    check(before && before->active_voices == 1, "actual starting voice state");
    unsigned failures = 0;
    for (int n = 0; n < 64; ++n) {
        fail_after = n;
        auto result = x.owner->force_retire(handle, f);
        fail_after = -1;
        if (result) {
            check(failures && result->native_resources_released && result->voices == 1,
                  "successful retry after real single-retirement failures");
            x.drain();
            return failures;
        }
        check(allocation_failed(f), "single preflight names allocation failure");
        x.unchanged(handle, *before);
        x.no_partial_groups(1);
        ++failures;
    }
    check(false, "single allocation sweep reached committed success");
    return 0;
}
unsigned context_failpoints(const char* path) {
    Fixture x(path);
    auto first = x.start(0);
    auto second = x.start(1);
    PlaybackFailure f;
    check(bool(x.owner->mix(256, f)), "warm both real context voices");
    auto a = x.owner->observe(first, f);
    auto b = x.owner->observe(second, f);
    check(a && b && a->active_voices == 1 && b->active_voices == 1,
          "actual starting context voice states");
    unsigned failures = 0;
    for (int n = 0; n < 64; ++n) {
        fail_after = n;
        auto result = x.owner->force_retire_context_outputs(x.context, f);
        fail_after = -1;
        if (result) {
            check(failures && result->size() == 2, "successful context retry commits both plays");
            for (const auto& item : *result)
                check(item.native_resources_released && item.voices == 1,
                      "successful context batch frees each real keeper");
            x.drain();
            return failures;
        }
        check(allocation_failed(f), "context preflight names allocation failure");
        x.unchanged(first, *a);
        x.unchanged(second, *b);
        x.no_partial_groups(2);
        ++failures;
    }
    check(false, "context allocation sweep reached committed success");
    return 0;
}
void mixed_allocation_failure(const char* path) {
    Fixture x(path);
    auto old = x.start(0);
    auto live = x.start(1);
    PlaybackFailure f;
    check(bool(x.owner->mix(256, f)), "warm both actual outputs before retiring one");
    auto group = x.owner->force_retire(old, f);
    check(group && group->native_resources_released, "actual retired tail before mix failure");
    auto before = x.owner->observe(live, f);
    auto tail = x.owner->observe_retired(group->outputs, f);
    check(before && tail && tail->active_voices == 1, "both mix contributors remain active");
    fail_after = 0;
    auto denied = x.owner->mix(32, f);
    fail_after = -1;
    check(!denied && f.kind == PlaybackError::allocation_failed,
          "first actual mixed-pool preflight allocation fails by name");
    x.unchanged(live, *before);
    auto unchanged_tail = x.owner->observe_retired(group->outputs, f);
    check(unchanged_tail && unchanged_tail->active_voices == tail->active_voices &&
          unchanged_tail->first_voice_source_index == tail->first_voice_source_index,
          "allocation failure neither advances nor prunes the retired tail");
    auto retried = x.owner->mix(32, f);
    auto advanced = x.owner->observe(live, f);
    check(retried && advanced && advanced->first_voice_source_index != before->first_voice_source_index,
          "unwrapped retry advances the real live voice");
    check(!x.owner->observe_retired(group->outputs, f) && f.kind == PlaybackError::retired_output,
          "successful retry drains and prunes the old tail");
    check(x.owner->stop(live, f) && bool(x.owner->mix(32, f)) &&
          x.owner->release_sound_context(x.context, f) && x.owner->close_when_idle(f),
          "mixed rollback fixture tears down idle owners");
}
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    auto single = single_failpoints(argv[1]);
    auto context = context_failpoints(argv[1]);
    mixed_allocation_failure(argv[1]);
    std::printf("PASS_NATIVE_FORCED_OUTPUT_RETIREMENT_FAILURE: %u single and %u context allocation failures; mixed-pool rollback; test-only allocator wrap\n",
                single, context);
    return std::fflush(stdout) || std::ferror(stdout) ? 1 : 0;
}
