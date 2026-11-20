#include "mka/playback_owner.hpp"
#include "mka/program_host_clock.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <functional>
#include <thread>
#include <cstdio>
#include <cstdlib>

using namespace mka::sound;
namespace {
void check(bool ok, const char* reason) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", reason);
        std::abort();
    }
}
constexpr std::uint32_t first_bank = 0xaa127321, second_bank = 0xbb347321;
AudioBankCommand command(AudioBankOpcode opcode, std::uint32_t token) {
    AudioBankCommand result;
    result.opcode = opcode;
    result.bank_token = {token};
    return result;
}
NativeActivationParameters parameters(std::int32_t selector, std::int16_t priority = 0,
                                      std::uint16_t flags = 0) {
    return {initial_program_control_factors(), initial_program_control_factors(),
            {{1,14336,false},{1,-16384,true},{1,14336,false},{1,-16384,false},2048},
            3, flags, {selector, priority}};
}
struct SourceClock {
    std::atomic<std::int64_t> ticks{10000};
    std::atomic<std::size_t> reads{0};
    std::atomic<bool> fail{false};
    std::function<void()> on_read;
    std::optional<std::int64_t> read(ProgramHostClockFailure& failure) {
        ++reads;
        if (on_read) on_read();
        if (fail) { failure.kind = ProgramHostClockError::query; return std::nullopt; }
        return ticks.load();
    }
};
struct Fixture {
    SourceClock reader;
    std::shared_ptr<NativeProgramHostClock> clock;
    std::optional<NativePlaybackOwner> owner;
    NativeSoundContext context;
    explicit Fixture(const char* path) {
        ProgramHostClockFailure cf;
        clock = NativeProgramHostClock::create_with_reader(1000, [this](ProgramHostClockFailure& failure) { return reader.read(failure); }, cf);
        check(bool(clock), "explicit native host clock");
        AudioBanksFailure bf;
        auto session = AudioBankSession::open(path, std::nullopt, bf);
        check(bool(session), "actual archive session");
        AudioProtocolFailure pf;
        auto protocol = AudioBankProtocol::create(std::move(*session), pf);
        check(bool(protocol), "actual bank protocol");
        BankServiceFailure sf;
        auto service = AudioBankService::create(std::move(*protocol), sf);
        check(bool(service), "actual bank Service");
        PlaybackFailure f;
        owner = NativePlaybackOwner::create_with_clock(std::move(*service), 48000, clock, f);
        check(bool(owner), "actual playback Owner with shared clock");
        auto owned = owner->create_sound_context(NativeSoundContextConfiguration{2}, f);
        check(bool(owned), "actual explicitly configured context");
        context = *owned;
        load(first_bank);
    }
    void load(std::uint32_t token) {
        AudioBankCommand request;
        request.opcode = AudioBankOpcode::load_sync;
        request.pathname = "announcr";
        PlaybackFailure f;
        auto result = owner->load_sync(request, {token}, f);
        check(result && result->bank.value == token, "actual ANNOUNCR bank publication");
    }
    NativePlayHandle activate(std::int32_t selector, std::int16_t priority = 0,
                              std::uint16_t flags = 0, std::uint32_t token = first_bank) {
        PlaybackFailure f;
        auto result = owner->activate(context, command(AudioBankOpcode::play, token),
                                      parameters(selector, priority, flags), f);
        check(bool(result), "actual native output activation");
        return *result;
    }
    PlaybackObservation observe(const NativePlayHandle& handle) {
        PlaybackFailure f;
        auto result = owner->observe(handle, f);
        check(bool(result), "actual play observation");
        return *result;
    }
    void finish() {
        PlaybackFailure f;
        check(owner->release_sound_context(context, f), "actual context retirement");
        check(owner->close_when_idle(f), "actual idle Service close");
    }
};
void selectors_and_priority(const char* path) {
    Fixture f(path);
    PlaybackFailure failure;
    const auto play = command(AudioBankOpcode::play, first_bank);
    check(!f.owner->activate(f.context, play, parameters(2), failure) &&
          failure.kind == PlaybackError::invalid_output_selector, "explicit boundary is exclusive");
    check(!f.owner->activate(f.context, play, parameters(-2), failure) &&
          failure.kind == PlaybackError::unsupported_output_selector, "selector minus two stays unbound");
    auto zero = f.activate(0, -2, 0x80);
    auto one = f.activate(1);
    check(f.observe(zero).output_role == 0 && f.observe(one).output_role == 1,
          "explicit role zero and one are valid");
    check(!f.owner->activate(f.context, play, parameters(0, -3), failure) &&
          failure.kind == PlaybackError::output_priority_refused, "signed lower priority refused");
    check(!f.owner->activate(f.context, play, parameters(0, -2, 0x80), failure) &&
          failure.kind == PlaybackError::output_priority_refused, "new flag80 refuses equal priority");
    auto tied = f.activate(0, -2);
    auto replaced = f.observe(zero);
    check(replaced.replaced && !replaced.output_owned && !replaced.dispatch_live &&
          replaced.private_copy_bound, "incumbent flag80 does not veto equal-priority replacement");
    auto greater = f.activate(0, -1, 0x80);
    check(f.observe(tied).replaced && f.observe(greater).output_owned &&
          f.observe(greater).priority == -1, "higher signed priority wins despite new flag80");
    check(f.owner->release(zero, failure) && f.owner->release(tied, failure) &&
          f.observe(greater).output_owned, "old exact identity release keeps successor slot");
    check(f.owner->stop(greater, failure) && f.owner->stop(one, failure),
          "mode-zero stop for idle explicit owners");
    check(f.observe(greater).output_owned && !f.observe(greater).dispatch_live,
          "mode-zero stop leaves exact role owned but detaches live dispatch");
    f.finish();
}
void automatic_growth(const char* path) {
    Fixture f(path);
    PlaybackFailure failure;
    std::vector<NativePlayHandle> handles;
    for (std::size_t n = 0; n < 50; ++n) {
        handles.push_back(f.activate(-1));
        const auto p = f.observe(handles.back());
        check(p.output_role == n + 2 && p.output_owned && p.dispatch_live,
              "auto chooses lowest free role and grows past source table");
    }
    check(f.owner->stop(handles[3], failure) && f.observe(handles[3]).output_owned,
          "ordinary stop alone does not free an automatic role");
    check(f.owner->release(handles[3], failure), "final release frees exact auto role");
    auto reused = f.activate(-1);
    check(f.observe(reused).output_role == 5, "automatic policy reuses lowest free role");
    for (std::size_t n = 0; n < handles.size(); ++n)
        if (n != 3) check(f.owner->stop(handles[n], failure), "stop pending automatic owner");
    check(f.owner->stop(reused, failure), "stop reused automatic owner");
    f.finish();
}
void clock_after_preparation(const char* path) {
    Fixture f(path);
    auto domain = f.owner->resource_domain();
    AudioResourceFailure resource_failure;
    auto exclusion = domain->surround_movie(resource_failure);
    check(bool(exclusion), "actual movie publication exclusion");
    const auto reads = f.reader.reads.load();
    std::promise<void> done;
    auto completed = done.get_future();
    std::optional<NativePlayHandle> result;
    std::thread worker([&] {
        PlaybackFailure failure;
        result = f.owner->activate(f.context, command(AudioBankOpcode::play, first_bank),
                                   parameters(0), failure);
        done.set_value();
    });
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (domain->observe()[3].waiting == 0) {
        check(std::chrono::steady_clock::now() < until, "actual preparation reaches publication wait");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    check(f.reader.reads.load() == reads, "activation does not sample before blocked preparation");
    f.reader.ticks += 137;
    check(exclusion->release(resource_failure), "release real publication exclusion");
    check(completed.wait_for(std::chrono::seconds(10)) == std::future_status::ready,
          "activation finishes after publication");
    worker.join();
    check(result && f.reader.reads.load() == reads + 1, "one post-preparation activation sample");
    const auto deadline = f.observe(*result).deadline;
    ProgramHostClockFailure cf;
    const auto sample = f.clock->read(cf);
    PlaybackFailure failure;
    const auto start = f.owner->tick(*result, deadline, failure);
    check(sample && start && start->kind == ProgramStepKind::sample_start &&
          deadline.milliseconds ==
              sample->milliseconds + static_cast<std::uint32_t>(start->command.delay_milliseconds()),
          "owner uses post-preparation clock sample and actual first-command delay");
    check(f.owner->stop(*result, failure) && bool(f.owner->mix(32, failure)),
          "post-preparation clock test retires its actual output");
    f.finish();
}
void shared_update_and_failure(const char* path) {
    Fixture f(path);
    PlaybackFailure failure;
    auto first = f.activate(0);
    auto second = f.activate(1);
    check(f.observe(first).deadline.milliseconds == f.observe(second).deadline.milliseconds,
          "same sampler position yields the same actual activation deadline");
    f.reader.ticks += 100;
    f.reader.fail = true;
    const auto pending = f.owner->observe_sound_context(f.context, failure);
    check(pending && pending->programs == 2 && pending->pending_preparations == 0,
          "two published actual preparations");
    check(!f.owner->activate(f.context, command(AudioBankOpcode::play, first_bank),
                             parameters(-1), failure) &&
          failure.kind == PlaybackError::clock_failed, "clock failure cannot publish another handle");
    auto after = f.owner->observe_sound_context(f.context, failure);
    check(after && after->programs == 2 && after->pending_preparations == 0,
          "failed activation removes pending owner and keeps prior owners");
    SoundContextUpdate update;
    check(!f.owner->update_ordinary_context(f.context, update, failure) &&
          failure.kind == PlaybackError::clock_failed, "update names actual clock failure");
    check(f.observe(first).command_index == 0 && f.observe(second).command_index == 0 &&
          f.observe(first).voices == 0 && f.observe(second).voices == 0,
          "failed update advances no cursor and starts no voice");
    f.reader.fail = false;
    const auto reads = f.reader.reads.load();
    check(f.owner->update_ordinary_context(f.context, update, failure) &&
          f.reader.reads == reads + 1 && update.steps.size() == 2,
          "one sample for both actual context entries");
    for (const auto& step : update.steps)
        check(step.event && !step.failure && step.event->kind == ProgramStepKind::sample_start &&
              step.event->observed && step.event->observed->milliseconds == update.observed.milliseconds,
              "ordinary pass starts both actual outputs on the same clock word");
    auto audio = f.owner->mix(512, failure);
    check(audio && std::any_of(audio->begin(), audio->end(), [](float x) { return x != 0; }),
          "shared update is connected to actual software voices");
    check(f.owner->stop(first, failure) && f.owner->stop(second, failure) &&
          bool(f.owner->mix(32, failure)), "real software tails retire");
    f.finish();
}
void dispatches_all_due_commands(const char* path) {
    Fixture f(path);
    auto handle = f.activate(0);
    f.reader.ticks += 3000;
    const auto reads = f.reader.reads.load();
    PlaybackFailure failure;
    SoundContextUpdate update;
    check(f.owner->update_ordinary_context(f.context, update, failure) &&
          f.reader.reads.load() == reads + 1 && update.steps.size() == 2,
          "clock leap dispatches both due commands with one sample");
    check(update.steps[0].event && !update.steps[0].failure &&
          update.steps[0].event->kind == ProgramStepKind::sample_start &&
          update.steps[0].event->command_index == 0 &&
          update.steps[1].event && !update.steps[1].failure &&
          update.steps[1].event->kind == ProgramStepKind::program_stop &&
          update.steps[1].event->command_index == 1,
          "actual first kind1 and terminal kind7 run in order within the same pass");
    for (const auto& step : update.steps)
        check(step.event->observed &&
              step.event->observed->milliseconds == update.observed.milliseconds,
              "every due command receives the same sampled clock word");
    auto stopped = f.observe(handle);
    check(stopped.executor_finished && !stopped.dispatch_live &&
          stopped.output_owned && stopped.private_copy_bound && stopped.voices == 1,
          "due stop detaches live dispatch while retaining role, copy and actual output");
    check(f.owner->update_ordinary_context(f.context, update, failure) &&
          update.steps.empty(), "finished play is absent from subsequent ordinary dispatch");
    check(bool(f.owner->mix(32, failure)), "real stopped output retires through mixer");
    f.finish();
}
void reader_reentry(const char* path) {
    Fixture f(path);
    std::size_t observed_busy = 0;
    f.reader.on_read = [&] {
        PlaybackFailure reentry;
        auto context = f.owner->observe_sound_context(f.context, reentry);
        check(!context && reentry.kind == PlaybackError::operation_active,
              "clock provider reads owner through a named operation gate");
        ++observed_busy;
    };
    auto handle = f.activate(0);
    check(observed_busy == 1 && f.observe(handle).dispatch_live,
          "activation samples and commits after named reader reentry");
    f.reader.ticks += 100;
    SoundContextUpdate update;
    PlaybackFailure failure;
    check(f.owner->update_ordinary_context(f.context, update, failure) &&
          observed_busy == 2 && !update.steps.empty() &&
          update.steps.front().event &&
          update.steps.front().event->kind == ProgramStepKind::sample_start,
          "ordinary update samples and starts output after named reader reentry");
    f.reader.on_read = {};
    check(f.owner->stop(handle, failure) && bool(f.owner->mix(32, failure)),
          "reentry fixture retires its actual output");
    f.finish();
}
void replacement_tail(const char* path) {
    Fixture f(path);
    f.load(second_bank);
    PlaybackFailure failure;
    SoundContextUpdate update;
    auto old = f.activate(0, 0, 0, first_bank);
    f.reader.ticks += 100;
    check(f.owner->update_ordinary_context(f.context, update, failure) &&
          update.steps.size() == 1 && update.steps[0].event &&
          update.steps[0].event->kind == ProgramStepKind::sample_start, "old owner starts a real voice");
    check(bool(f.owner->mix(512, failure)), "advance incumbent software voice");
    const auto position = f.observe(old).first_voice_source_index;
    f.reader.fail = true;
    check(!f.owner->activate(f.context, command(AudioBankOpcode::play, second_bank),
                             parameters(0, 1), failure) &&
          failure.kind == PlaybackError::clock_failed, "replacement clock failure names actual failure");
    auto retained = f.observe(old);
    check(retained.explicitly_stopped && !retained.dispatch_live && !retained.replaced &&
          retained.output_owned && retained.private_copy_bound && retained.active_voices == 1 &&
          retained.first_voice_source_index == position,
          "failed replacement leaves the incumbent stopped and owning its exact role and tail");
    auto state = f.owner->observe_sound_context(f.context, failure);
    check(state && state->programs == 1 && state->pending_preparations == 0,
          "failed replacement publishes no successor or pending reservation");
    f.reader.fail = false;
    auto next = f.activate(0, 1, 0, second_bank);
    const auto detached = f.observe(old);
    check(detached.replaced && !detached.dispatch_live && !detached.output_owned &&
          detached.private_copy_bound && detached.active_voices == 1 &&
          detached.first_voice_source_index == position, "replacement detaches but retains real software tail");
    check(f.observe(next).output_role == 0 && f.observe(next).output_owned,
          "successor owns same logical role");
    check(!f.owner->unload(command(AudioBankOpcode::unload, first_bank), failure) &&
          failure.kind == PlaybackError::active_playback, "active retained old tail blocks exact bank unload");
    check(f.owner->update_ordinary_context(f.context, update, failure) &&
          update.steps.size() == 1 && update.steps[0].event &&
          update.steps[0].event->kind == ProgramStepKind::sample_start &&
          f.observe(next).active_voices == 1, "new owner alone dispatches on ordinary live list");
    check(bool(f.owner->mix(32, failure)) && f.observe(old).active_voices == 0 &&
          f.observe(old).first_voice_source_index == position, "old release freezes position and reaches idle");
    check(f.owner->unload(command(AudioBankOpcode::unload, first_bank), failure) &&
          f.observe(next).output_owned, "exact old bank retirement leaves successor role intact");
    check(f.owner->stop(next, failure) && bool(f.owner->mix(32, failure)),
          "successor software tail retires");
    f.finish();
}
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: test_program_output_activation <pinned_original_MS2>\n");
        return 2;
    }
    selectors_and_priority(argv[1]);
    automatic_growth(argv[1]);
    clock_after_preparation(argv[1]);
    shared_update_and_failure(argv[1]);
    dispatches_all_due_commands(argv[1]);
    reader_reentry(argv[1]);
    replacement_tail(argv[1]);
    std::puts("PASS_NATIVE_PROGRAM_OUTPUT_ACTIVATION: 7 actual role/clock/replacement groups; ordinary native output only");
    return std::fflush(stdout) || std::ferror(stdout) ? 1 : 0;
}

