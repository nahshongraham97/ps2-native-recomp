#include "mka/playback_owner.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <utility>

using namespace mka::sound;
namespace {
void check(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::abort(); }
}
template<class Predicate> void await(Predicate predicate, const char* message) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!predicate()) {
        check(std::chrono::steady_clock::now() < deadline, message);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
AudioBankCommand load() {
    AudioBankCommand result; result.opcode = AudioBankOpcode::load_sync; result.pathname = "announcr";
    return result;
}
AudioBankCommand command(AudioBankOpcode opcode, std::uint32_t token) {
    AudioBankCommand result; result.opcode = opcode; result.bank_token = {token}; result.selector = 0;
    return result;
}
NativePlayParameters parameters() {
    // Explicit existing software-fixture values, not retail profile selection or fidelity proof.
    return {initial_program_control_factors(), initial_program_control_factors(),
        {{1,14336,false},{1,-16384,true},{1,14336,false},{1,-16384,false},2048}, {1000}, 3};
}
NativePlaybackOwner owner(const std::filesystem::path& original,
                          const std::shared_ptr<AudioResourceDomain>& domain) {
    AudioBanksFailure banks;
    auto session = AudioBankSession::open(original, std::nullopt, domain, banks);
    check(session && session->resource_domain() == domain, "actual MS2 session retains explicit resource domain");
    AudioProtocolFailure protocol_failure;
    auto protocol = AudioBankProtocol::create(std::move(*session), protocol_failure);
    check(protocol && protocol->resource_domain() == domain, "actual protocol retains same domain");
    BankServiceFailure service_failure;
    auto service = AudioBankService::create(std::move(*protocol), service_failure);
    check(service && service->resource_domain() == domain, "actual Service retains same domain");
    PlaybackFailure failure;
    auto result = NativePlaybackOwner::create(std::move(*service), 48000, failure);
    check(result && result->resource_domain() == domain, "actual playback owner retains same Service domain");
    return std::move(*result);
}
bool nonzero(const std::vector<float>& samples) {
    return std::any_of(samples.begin(), samples.end(), [](float value) { return value != 0; });
}
}
int main(int argc, char** argv) {
    if (argc != 2) { std::fprintf(stderr, "usage: test_playback_reservation <pinned_original_MS2>\n"); return 2; }
    AudioResourceFailure resource_failure;
    auto domain = AudioResourceDomain::create(resource_failure);
    check(bool(domain), "explicit native sound resource domain");
    auto sound = owner(std::filesystem::path(argv[1]), domain);
    auto foreign = owner(std::filesystem::path(argv[1]), domain);
    PlaybackFailure failure;
    auto existing_context = sound.create_sound_context(failure);
    auto pending_context = sound.create_sound_context(failure);
    auto foreign_context = foreign.create_sound_context(failure);
    check(existing_context && pending_context && foreign_context, "explicit distinct context identities");
    NativeSoundContext unknown;
    check(!sound.observe_sound_context(unknown, failure) && failure.kind == PlaybackError::unknown_context,
          "empty context has named unknown identity");
    check(!sound.observe_sound_context(*foreign_context, failure) && failure.kind == PlaybackError::foreign_context &&
          !foreign.observe_sound_context(*existing_context, failure) && failure.kind == PlaybackError::foreign_context,
          "context identity never crosses owner boundary");
    auto retired_context = sound.create_sound_context(failure);
    check(retired_context && sound.release_sound_context(*retired_context, failure) &&
          !sound.observe_sound_context(*retired_context, failure) && failure.kind == PlaybackError::retired_context &&
          !sound.release_sound_context(*retired_context, failure) && failure.kind == PlaybackError::retired_context,
          "retired context cannot resolve or retire twice");
    constexpr std::uint32_t first = 0x8137ab01, second = 0xc592ab01;
    const auto params = parameters();
    check(!sound.play(unknown, command(AudioBankOpcode::play, first), params, failure) && failure.kind == PlaybackError::unknown_context,
          "unknown context cannot reserve a play");
    check(!sound.play(*retired_context, command(AudioBankOpcode::play, first), params, failure) && failure.kind == PlaybackError::retired_context,
          "retired context cannot reserve a play");
    auto loaded_first = sound.load_sync(load(), {first}, failure);
    auto loaded_second = sound.load_sync(load(), {second}, failure);
    check(loaded_first && loaded_first->bank.value == first && loaded_second && loaded_second->bank.value == second,
          "two exact real ANNOUNCR bank identities load through actual Service");
    auto existing = sound.play(*existing_context, command(AudioBankOpcode::play, first), params, failure);
    check(bool(existing), "existing real announcer program prepares before movie interval");
    auto started = sound.tick(*existing, {1000}, failure);
    check(started && started->kind == ProgramStepKind::sample_start && started->clip &&
          started->clip->pcm.size() == 1 && started->clip->pcm[0].samples.size() == 34720,
          "real ANNOUNCR selector0 sample owns exact existing native data");
    auto before = sound.mix(2048, failure);
    check(before && before->size() == 4096 && nonzero(*before), "existing real voice produces nonzero native PCM");
    auto previous = sound.observe(*existing, failure);
    check(previous && previous->active_voices == 1 && previous->first_voice_source_index,
          "existing voice source position observed before pending preparation");

    auto movie = domain->surround_movie(resource_failure);
    check(bool(movie), "actual movie publication exclusion lease acquired");
    std::optional<NativePlayHandle> prepared;
    PlaybackFailure preparation_failure;
    std::atomic<unsigned> returned{0};
    std::thread worker([&] {
        prepared = sound.play(*pending_context, command(AudioBankOpcode::play, second), params, preparation_failure);
        returned.fetch_add(1, std::memory_order_release);
    });
    await([&] { return domain->observe()[3].waiting == 1; }, "fresh real second-bank preparation must reach publication wait");
    check(returned.load(std::memory_order_acquire) == 0, "queued observation never pretends preparation completed");
    auto pending = sound.observe_sound_context(*pending_context, failure);
    auto live = sound.observe_sound_context(*existing_context, failure);
    check(pending && pending->pending_preparations == 1 && pending->programs == 0 && pending->active_voices == 0 &&
          live && live->pending_preparations == 0 && live->programs == 1 && live->active_voices == 1,
          "same owner exposes pending reservation and isolated existing context");
    check(!sound.unload(command(AudioBankOpcode::unload, second), failure) && failure.kind == PlaybackError::active_playback,
          "pending actual preparation blocks exact bank unload without registry deadlock");
    check(!sound.release_sound_context(*pending_context, failure) && failure.kind == PlaybackError::active_playback,
          "pending context cannot drop actual preparation ownership");
    check(!sound.close_when_idle(failure) && failure.kind == PlaybackError::operation_active,
          "owner activity covers blocked preparation and rejects close promptly");
    auto during = sound.mix(512, failure);
    auto advanced = sound.observe(*existing, failure);
    check(during && during->size() == 1024 && nonzero(*during) && advanced && advanced->active_voices == 1 &&
          advanced->first_voice_source_index && *advanced->first_voice_source_index > *previous->first_voice_source_index,
          "publication exclusion neither silences nor stalls existing real voice mixing");

    // Churn explicit contexts while an existing preparation owns its stable
    // reservation. One real failed Service lookup also exercises rollback; no
    // assumption about retained failed slots or vector capacity is made here.
    for (unsigned index = 0; index < 64; ++index) {
        auto context = sound.create_sound_context(failure);
        check(bool(context), "dynamic context registry grows while real preparation is pending");
        if (index == 0) {
            auto rejected = sound.play(*context, command(AudioBankOpcode::play, 0xdeadbeef), params, failure);
            check(!rejected && failure.kind == PlaybackError::service_failed &&
                  failure.service.kind == BankServiceError::unknown_bank_token,
                  "unpublished reservation rolls back after actual unknown bank fails by name");
        }
        auto empty = sound.observe_sound_context(*context, failure);
        check(empty && empty->pending_preparations == 0 && empty->programs == 0 && sound.release_sound_context(*context, failure),
              "failed temporary reservation retires its own activity and context");
    }
    pending = sound.observe_sound_context(*pending_context, failure);
    check(pending && pending->pending_preparations == 1 && returned.load(std::memory_order_acquire) == 0 &&
          domain->observe()[3].depth == 1 && domain->observe()[3].waiting == 1,
          "context churn and failed reservation preserve exact blocked preparation and movie lease");
    check(movie->release(resource_failure), "same-owner movie release admits actual pending publication");
    await([&] { return returned.load(std::memory_order_acquire) == 1; }, "actual blocked preparation must finish after exclusion release");
    worker.join();
    check(prepared && preparation_failure.kind == PlaybackError::none && returned.load() == 1,
          "one actual worker commits exact surviving reservation after concurrent context churn");
    pending = sound.observe_sound_context(*pending_context, failure);
    auto ready = sound.observe(*prepared, failure);
    check(pending && pending->pending_preparations == 0 && pending->programs == 1 && ready && ready->voices == 0 &&
          ready->command_index == 0 && ready->deadline.milliseconds == 1000,
          "pending becomes actual program once, without fabricated voice-start acknowledgement");
    auto fresh_start = sound.tick(*prepared, {1000}, failure);
    check(fresh_start && fresh_start->kind == ProgramStepKind::sample_start && fresh_start->clip &&
          fresh_start->clip->pcm[0].samples.size() == 34720, "newly committed actual ANNOUNCR program starts real sample");
    check(sound.stop(*existing, failure) && sound.stop(*prepared, failure), "explicit per-play normal stops own both voices");
    check(bool(sound.mix(32, failure)) && sound.observe(*existing, failure)->active_voices == 0 &&
          sound.observe(*prepared, failure)->active_voices == 0, "explicit fixture release envelope reaches inactive native voice");
    check(sound.release(*existing, failure) && sound.release(*prepared, failure), "inactive actual play entries retire");
    check(sound.release_sound_context(*existing_context, failure), "idle existing context retires explicitly");
    auto moved = std::move(sound);
    check(!sound.observe_sound_context(*pending_context, failure) && failure.kind == PlaybackError::moved_owner &&
          !sound.resource_domain(), "moved-from owner rejects context without stealing retained state");
    auto moved_context = moved.observe_sound_context(*pending_context, failure);
    check(moved_context && moved_context->programs == 0 && moved_context->pending_preparations == 0 && moved.resource_domain() == domain,
          "same explicit context and resource domain survive owner move");
    check(moved.release_sound_context(*pending_context, failure) && !moved.observe_sound_context(*pending_context, failure) &&
          failure.kind == PlaybackError::retired_context, "moved owner retains exact context retirement identity");
    check(moved.unload(command(AudioBankOpcode::unload, first), failure) && moved.unload(command(AudioBankOpcode::unload, second), failure) &&
          moved.close_when_idle(failure), "actual owner operations retire and both real banks unload cleanly");
    check(foreign.release_sound_context(*foreign_context, failure) && foreign.close_when_idle(failure), "independent owner context remains valid");
    for (const auto& state : domain->observe()) check(state.depth == 0 && state.waiting == 0, "all actual native resource work retired");
    std::puts("PASS actual ANNOUNCR pending play reservation/context identity/registry growth/nonzero concurrent voice/resource domain; no device/game/PCM fidelity claim");
    return std::fflush(stdout) || std::ferror(stdout) ? 1 : 0;
}
