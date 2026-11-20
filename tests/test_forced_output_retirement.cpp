#include "mka/playback_owner.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <thread>

using namespace mka::sound;
namespace {
constexpr std::uint32_t first_bank = 0xaa127321, second_bank = 0xbb347321;
void check(bool ok, const char* reason) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", reason);
        std::abort();
    }
}
AudioBankCommand command(AudioBankOpcode op, std::uint32_t token) {
    AudioBankCommand c;
    c.opcode = op;
    c.bank_token = {token};
    return c;
}
NativePlayParameters parameters(std::int32_t role) {
    return {initial_program_control_factors(), initial_program_control_factors(),
            {{1,14336,false},{1,-16384,true},{1,14336,false},{1,-16384,false},2048},
            {1000}, 3, 0, {role, 0}};
}
bool audible(const std::vector<float>& pcm) {
    return std::any_of(pcm.begin(), pcm.end(), [](float x) { return x != 0; });
}
struct Fixture {
    std::optional<NativePlaybackOwner> owner;
    NativeSoundContext context;
    explicit Fixture(const char* path) {
        AudioBanksFailure bf;
        auto session = AudioBankSession::open(path, std::nullopt, bf);
        check(bool(session), "actual sound bank session");
        AudioProtocolFailure pf;
        auto protocol = AudioBankProtocol::create(std::move(*session), pf);
        check(bool(protocol), "actual bank protocol");
        BankServiceFailure sf;
        auto service = AudioBankService::create(std::move(*protocol), sf);
        check(bool(service), "actual bank Service");
        PlaybackFailure f;
        owner = NativePlaybackOwner::create(std::move(*service), 48000, f);
        check(bool(owner), "actual native playback Owner");
        context = make_context();
        load(first_bank);
    }
    NativeSoundContext make_context() {
        PlaybackFailure f;
        auto ctx = owner->create_sound_context({2}, f);
        check(bool(ctx), "actual sound context");
        return *ctx;
    }
    void load(std::uint32_t token) {
        PlaybackFailure f;
        auto request = command(AudioBankOpcode::load_sync, token);
        request.pathname = "announcr";
        auto loaded = owner->load_sync(request, {token}, f);
        check(loaded && loaded->bank.value == token, "actual ANNOUNCR bank");
    }
    NativePlayHandle play(const NativeSoundContext& ctx, std::uint32_t token, std::int32_t role) {
        PlaybackFailure f;
        auto result = owner->play(ctx, command(AudioBankOpcode::play, token), parameters(role), f);
        check(bool(result), "actual runtime copy and play");
        return *result;
    }
    ProgramStep start(const NativePlayHandle& handle) {
        PlaybackFailure f;
        auto result = owner->tick(handle, {1000}, f);
        check(result && result->kind == ProgramStepKind::sample_start && result->clip,
              "actual native voice starts");
        return *result;
    }
    PlaybackObservation observe(const NativePlayHandle& handle) {
        PlaybackFailure f;
        auto result = owner->observe(handle, f);
        check(bool(result), "live handle observation");
        return *result;
    }
};
void single(const char* path) {
    Fixture x(path);
    x.load(second_bank);
    PlaybackFailure f;
    auto old = x.play(x.context, first_bank, 0);
    auto step = x.start(old);
    auto mixed = x.owner->mix(512, f);
    check(mixed && audible(*mixed), "actual software PCM before forced retirement");
    auto position = x.observe(old).first_voice_source_index;
    const auto& samples = step.clip->pcm[0].samples;
    auto strong_tail_frame = [&] {
        if (!position || *position + 1 >= samples.size()) return false;
        const auto left = samples[static_cast<std::size_t>(*position)];
        const auto right = samples[static_cast<std::size_t>(*position) + 1];
        return (left > 512 && right > 512) || (left < -512 && right < -512);
    };
    for (unsigned frame = 0; frame < 512 && !strong_tail_frame(); ++frame) {
        check(bool(x.owner->mix(1, f)), "locate nonzero PCM at an actual live source position");
        position = x.observe(old).first_voice_source_index;
    }
    check(strong_tail_frame(), "selected PCM neighbors guarantee nonzero frozen interpolation");
    auto receipt = x.owner->force_retire(old, f);
    check(receipt && receipt->native_resources_released && receipt->voices == 1,
          "forced retirement releases clone and actual Service reference");
    check(!x.owner->observe(old, f) && f.kind == PlaybackError::retired_handle,
          "public play identity is retired independently of its output");
    auto retired = x.owner->observe_retired(receipt->outputs, f);
    check(retired && retired->native_resources_released && retired->voices == 1 &&
          retired->active_voices == 1 && retired->original_bank.value == first_bank &&
          retired->first_voice_source_index == position, "exact frozen software tail remains owned");
    auto summary = x.owner->observe_sound_context(x.context, f);
    check(summary && summary->programs == 0 && summary->retired_groups == 1 &&
          summary->retired_voices == 1 && summary->active_retired_voices == 1 &&
          summary->active_voices == 1, "context reports real retired and total active voices");
    check(!x.owner->release_sound_context(x.context, f) && f.kind == PlaybackError::active_playback,
          "context destruction refuses an active retired tail");
    check(!x.owner->close_when_idle(f) && f.kind == PlaybackError::active_playback,
          "idle close refuses an active retired tail");
    auto successor = x.play(x.context, second_bank, 0);
    check(x.observe(successor).output_owned, "successor acquires released exact slot");
    check(x.owner->unload(command(AudioBankOpcode::unload, first_bank), f) &&
          x.observe(successor).output_owned, "old bank unload succeeds before tail drain and preserves successor");
    check(!step.owner->normal_instances[0].cache_reference &&
          !step.clip->pcm[0].samples.empty(), "exported data keeps PCM without a cache acquisition");
    Fixture foreign(path);
    check(!foreign.owner->observe_retired(receipt->outputs, f) && f.kind == PlaybackError::foreign_owner,
          "a different Owner cannot observe this exact retired identity");
    auto tail_pcm = x.owner->mix(1, f);
    check(tail_pcm && audible(*tail_pcm), "retired tail emits actual nonzero PCM after old bank unload");
    auto still_live = x.owner->observe_retired(receipt->outputs, f);
    check(still_live && still_live->active_voices == 1 &&
          still_live->first_voice_source_index == position, "actual tail output keeps source position frozen");
    check(bool(x.owner->mix(32, f)), "actual retired software envelope drains");
    check(!x.owner->observe_retired(receipt->outputs, f) && f.kind == PlaybackError::retired_output,
          "completed tail is pruned with known identity");
    check(!x.owner->observe_retired(NativeRetiredOutputHandle{}, f) &&
          f.kind == PlaybackError::unknown_retired_output, "empty tail identity stays distinct");
    check(x.owner->stop(successor, f) && x.observe(successor).private_copy_bound,
          "ordinary mode zero remains a retained copy");
    check(x.owner->release_sound_context(x.context, f) && x.owner->close_when_idle(f),
          "idle shutdown succeeds after actual tail drain");
}
void context_scope(const char* path) {
    Fixture x(path);
    x.load(second_bank);
    auto other_context = x.make_context();
    PlaybackFailure f;
    auto a = x.play(x.context, first_bank, 0);
    auto b = x.play(x.context, first_bank, 1);
    auto unrelated = x.play(other_context, second_bank, 0);
    x.start(a);
    x.start(b);
    x.start(unrelated);
    check(bool(x.owner->mix(256, f)), "actual output in both contexts");
    check(x.owner->stop(a, f), "ordinary stop before context force");
    auto prior = x.observe(a);
    check(prior.output_owned && prior.private_copy_bound, "mode-zero stopped play retains clone and role");
    auto results = x.owner->force_retire_context_outputs(x.context, f);
    check(results && results->size() == 2, "context force revokes exactly two context plays");
    for (const auto& result : *results)
        check(result.native_resources_released && result.voices == 1, "each real context copy retired");
    check(x.observe(unrelated).output_owned && x.observe(unrelated).dispatch_live,
          "different context role and dispatch remain unchanged");
    auto current = x.owner->observe_sound_context(x.context, f);
    check(current && current->programs == 0 && current->retired_groups == 2 &&
          current->active_retired_voices == 2, "same context identity remains live with detached tails");
    auto again = x.play(x.context, second_bank, 0);
    check(x.observe(again).output_owned, "same context admits a new exact role");
    check(x.owner->unload(command(AudioBankOpcode::unload, first_bank), f) &&
          x.observe(again).output_owned, "old bank retirement cannot erase current role");
    check(x.owner->stop(unrelated, f) && x.owner->stop(again, f) &&
          bool(x.owner->mix(32, f)), "actual mixed release tails drain");
    check(x.owner->release_sound_context(x.context, f) &&
          x.owner->release_sound_context(other_context, f) && x.owner->close_when_idle(f),
          "both contexts and Service retire normally");
}
void pending_context(const char* path) {
    Fixture x(path);
    auto domain = x.owner->resource_domain();
    AudioResourceFailure rf;
    auto publication = domain->surround_movie(rf);
    check(bool(publication), "actual sample publication exclusion");
    std::optional<NativePlayHandle> handle;
    std::promise<void> done;
    auto complete = done.get_future();
    std::thread worker([&] {
        handle = x.play(x.context, first_bank, 0);
        done.set_value();
    });
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!domain->observe()[3].waiting) {
        check(std::chrono::steady_clock::now() < end, "actual preparation blocked on publication");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    PlaybackFailure f;
    check(!x.owner->force_retire_context_outputs(x.context, f) &&
          f.kind == PlaybackError::operation_active, "pending preparation stays owned and is not fabricated cancellation");
    check(publication->release(rf) &&
          complete.wait_for(std::chrono::seconds(10)) == std::future_status::ready,
          "admitted preparation still completes");
    worker.join();
    check(bool(handle) && x.owner->stop(*handle, f) &&
          x.owner->release_sound_context(x.context, f) && x.owner->close_when_idle(f),
          "pending case preserves normal retirement");
}
void aggregate_rollback(const char* path) {
    Fixture x(path);
    PlaybackFailure f;
    auto prefix = x.play(x.context, first_bank, 0);
    auto failing = x.play(x.context, first_bank, 1);
    auto near_end = x.start(failing);
    const auto step = std::uint64_t(near_end.clip->source.rate_value()) * 4096 / 48000;
    const auto safe = (near_end.clip->pcm[0].samples.size() - 3) * 4096 / step;
    check(bool(x.owner->mix(static_cast<std::size_t>(safe) - 512, f)), "advance failing voice near real sample end");
    auto old = x.play(x.context, first_bank, -1);
    x.start(old);
    check(bool(x.owner->mix(512, f)), "warm old output before forced freeze");
    auto retired = x.owner->force_retire(old, f);
    check(retired && retired->voices == 1, "retired group joins aggregate mix");
    x.start(prefix);
    const auto before_prefix = x.observe(prefix).first_voice_source_index;
    const auto before_failing = x.observe(failing).first_voice_source_index;
    auto before_tail = x.owner->observe_retired(retired->outputs, f);
    check(before_tail && before_tail->active_voices == 1, "retired tail active before failed mix");
    check(!x.owner->mix(512, f) && f.kind == PlaybackError::voice_failed &&
          f.voice.kind == HostVoiceError::unsupported_sample_end_control,
          "actual later live voice fails in aggregate transaction");
    auto after_tail = x.owner->observe_retired(retired->outputs, f);
    check(x.observe(prefix).first_voice_source_index == before_prefix &&
          x.observe(failing).first_voice_source_index == before_failing &&
          after_tail && after_tail->active_voices == 1 &&
          after_tail->first_voice_source_index == before_tail->first_voice_source_index,
          "aggregate failure advances neither live prefix nor retired release");
    check(x.owner->stop(prefix, f) && x.owner->stop(failing, f) &&
          bool(x.owner->mix(32, f)) && x.owner->release_sound_context(x.context, f) &&
          x.owner->close_when_idle(f), "cleanup after aggregate rollback");
}
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    single(argv[1]);
    context_scope(argv[1]);
    pending_context(argv[1]);
    aggregate_rollback(argv[1]);
    std::puts("PASS_NATIVE_FORCED_OUTPUT_RETIREMENT: 4 actual Owner output retirement groups; software tails only");
    return std::fflush(stdout) || std::ferror(stdout) ? 1 : 0;
}
