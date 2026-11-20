#include "mka/playback_owner.hpp"
#include "mka/runtime_preparation.hpp"
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace mka::sound;
namespace {
void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}
struct Gate {
    std::mutex mutex;
    std::condition_variable changed;
    unsigned arrived = 0;
    bool released = false;
    void pause() {
        std::unique_lock lock(mutex);
        ++arrived;
        changed.notify_all();
        check(changed.wait_for(lock, std::chrono::seconds(10), [&] { return released; }), "worker gate released");
    }
    void await(unsigned count = 1) {
        std::unique_lock lock(mutex);
        check(changed.wait_for(lock, std::chrono::seconds(10), [&] { return arrived == count; }), "worker reached expected gate");
    }
    void release() {
        std::lock_guard lock(mutex);
        released = true;
        changed.notify_all();
    }
};
enum class Edge { before, after };
struct Injection { Edge edge = Edge::before; Gate* gate = nullptr; bool raise = false; };
thread_local Injection preparation, unloading, closing, loading, worker;

template<class Callable>
auto inject(const Injection& config, Callable&& call) {
    const auto at = [&](Edge edge) {
        if (edge != config.edge) return;
        if (config.gate) config.gate->pause();
        if (config.raise) throw std::runtime_error("test playback reservation Service edge");
    };
    at(Edge::before);
    auto result = call();
    at(Edge::after);
    return result;
}

AudioBankCommand command(AudioBankOpcode opcode, std::uint32_t token) {
    AudioBankCommand result;
    result.opcode = opcode;
    result.bank_token = {token};
    return result;
}
AudioBankCommand load(AudioBankOpcode opcode) {
    auto result = command(opcode, 0);
    result.pathname = "announcr";
    result.callback_token = 0x9175632a;
    return result;
}
NativePlayParameters parameters() {
    return {initial_program_control_factors(), initial_program_control_factors(),
        {{1,14336,false},{1,-16384,true},{1,14336,false},{1,-16384,false},2048}, {1000}, 3, 0};
}
NativePlaybackOwner create(const char* file) {
    AudioBanksFailure banks;
    auto session = AudioBankSession::open(file, std::nullopt, banks);
    check(bool(session), "actual MS2 session");
    AudioProtocolFailure protocol_failure;
    auto protocol = AudioBankProtocol::create(std::move(*session), protocol_failure);
    check(bool(protocol), "actual protocol owner");
    BankServiceFailure service_failure;
    auto service = AudioBankService::create(std::move(*protocol), service_failure);
    check(bool(service), "actual Service owner");
    PlaybackFailure f;
    auto playback = NativePlaybackOwner::create(std::move(*service), 48000, f);
    check(bool(playback), "actual playback registry");
    return std::move(*playback);
}
void load_sync(NativePlaybackOwner& owner, std::uint32_t token) {
    PlaybackFailure f;
    auto result = owner.load_sync(load(AudioBankOpcode::load_sync), {token}, f);
    check(result && result->bank.value == token, "real announcr bank load");
}
void check_pending(NativePlaybackOwner& owner, const NativeSoundContext& context,
                   const AudioBankCommand& unload, unsigned count) {
    PlaybackFailure f;
    auto pending = owner.observe_sound_context(context, f);
    check(pending && pending->pending_preparations == count && pending->programs == 0, "exact preparing membership");
    check(!owner.release_sound_context(context, f) && f.kind == PlaybackError::active_playback, "preparing context stays owned");
    check(!owner.unload(unload, f) && f.kind == PlaybackError::active_playback, "same bank remains busy before and after Service");
    check(!owner.close_when_idle(f) && f.kind == PlaybackError::operation_active, "close sees wrapper Activity outside Service");
}
}

// Explicit test-only GNU linker wrapping. The real native archive is unchanged.
#define SERVICE_WRAPS(name, symbol, type, args, invoke, config) \
    extern "C" type real_##name args __asm__("__real_" symbol); \
    extern "C" type wrap_##name args __asm__("__wrap_" symbol); \
    extern "C" type wrap_##name args { return inject(config, [&] { return real_##name invoke; }); }

SERVICE_WRAPS(prepare, "_ZN3mka5sound16AudioBankService23acquire_runtime_programERKNS0_16AudioBankCommandERNS0_18PreparationFailureERNS0_18BankServiceFailureE",
    std::unique_ptr<RuntimeProgramReference>,
    (AudioBankService* self, const AudioBankCommand& c, PreparationFailure& p, BankServiceFailure& f), (self, c, p, f), preparation)
SERVICE_WRAPS(unload, "_ZN3mka5sound16AudioBankService6unloadERKNS0_16AudioBankCommandERNS0_18BankServiceFailureE",
    bool, (AudioBankService* self, const AudioBankCommand& c, BankServiceFailure& f), (self, c, f), unloading)
SERVICE_WRAPS(close, "_ZN3mka5sound16AudioBankService15close_when_idleERNS0_18BankServiceFailureE",
    bool, (AudioBankService* self, BankServiceFailure& f), (self, f), closing)
SERVICE_WRAPS(load, "_ZN3mka5sound16AudioBankService9load_syncERKNS0_16AudioBankCommandENS0_13BankWireTokenERNS0_18BankServiceFailureE",
    std::optional<BankLoadOutcome>, (AudioBankService* self, const AudioBankCommand& c, BankWireToken token, BankServiceFailure& f), (self, c, token, f), loading)
SERVICE_WRAPS(run, "_ZN3mka5sound16AudioBankService7run_oneERNS0_18BankServiceFailureE",
    bool, (AudioBankService* self, BankServiceFailure& f), (self, f), worker)
#undef SERVICE_WRAPS

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    constexpr std::uint32_t token = 0xafcca521, other_token = 0x11977315;
    const auto play = command(AudioBankOpcode::play, token);
    const auto unload = command(AudioBankOpcode::unload, token);
    const auto params = parameters();
    PlaybackFailure f;
    {
        auto owner = create(argv[1]);
        for (Edge edge : {Edge::before, Edge::after}) {
            Gate gate;
            auto bank = edge == Edge::before ? token : other_token;
            std::thread loading_thread([&] { loading = {edge, &gate}; load_sync(owner, bank); });
            gate.await();
            check(!owner.close_when_idle(f) && f.kind == PlaybackError::operation_active, "load Activity spans either Service gap");
            gate.release();
            loading_thread.join();
        }
        check(owner.close_when_idle(f), "load Activities retire");
    }
    {
        auto owner = create(argv[1]);
        for (Edge edge : {Edge::before, Edge::after}) {
            Gate gate;
            auto bank = edge == Edge::before ? token : other_token;
            check(owner.submit_async(load(AudioBankOpcode::load_async), {bank}, f) == AsyncBankSubmission::queued, "real worker request");
            std::thread running([&] { worker = {edge, &gate}; PlaybackFailure run; check(owner.run_one(run), "real bank worker executes"); });
            gate.await();
            if (edge == Edge::after) {
                auto completion = owner.poll_completion(f);
                check(completion && completion->words[0] == 0 && completion->words[1] == bank,
                      "real Service completion drained while wrapper Activity remains");
            }
            check(!owner.close_when_idle(f) && f.kind == PlaybackError::operation_active, "worker Activity spans either Service gap");
            gate.release();
            running.join();
            if (edge == Edge::before) check(bool(owner.poll_completion(f)), "actual worker completion delivered");
        }
        check(owner.close_when_idle(f), "worker Activities retire");
    }
    {
        auto owner = create(argv[1]);
        load_sync(owner, token);
        auto context = owner.create_sound_context(f);
        check(bool(context), "explicit play context");
        for (Edge edge : {Edge::before, Edge::after}) {
            Gate gate;
            std::optional<NativePlayHandle> handle;
            std::thread playing([&] { preparation = {edge, &gate}; PlaybackFailure start; handle = owner.play(*context, play, params, start); });
            gate.await();
            check_pending(owner, *context, unload, 1);
            gate.release();
            playing.join();
            check(handle && owner.stop(*handle, f) && owner.release(*handle, f), "commit and retire exact gap reservation");
        }
        for (Edge edge : {Edge::before, Edge::after}) {
            preparation = {edge, nullptr, true};
            check(!owner.play(*context, play, params, f) && f.kind == PlaybackError::native_exception, "injected preparation failure reported");
            preparation = {};
            auto observation = owner.observe_sound_context(*context, f);
            check(observation && !observation->pending_preparations && !observation->programs, "failed unpublished reservation rolled back");
            auto retry = owner.play(*context, play, params, f);
            check(retry && owner.stop(*retry, f) && owner.release(*retry, f), "same bank prepares after exception");
        }
        constexpr unsigned count = 64;
        Gate growth;
        std::array<std::optional<NativePlayHandle>, count> handles;
        std::vector<std::thread> threads;
        threads.reserve(count);
        for (unsigned i = 0; i < count; ++i) {
            threads.emplace_back([&, i] { preparation = {Edge::before, &growth}; PlaybackFailure start; handles[i] = owner.play(*context, play, params, start); });
        }
        growth.await(count);
        check_pending(owner, *context, unload, count);
        growth.release();
        for (auto& thread : threads) thread.join();
        auto committed = owner.observe_sound_context(*context, f);
        check(committed && !committed->pending_preparations && committed->programs == count, "all vector-growth reservations commit");
        for (const auto& handle : handles) {
            check(handle && owner.stop(*handle, f) && owner.release(*handle, f), "each grown reservation owns an independent handle");
        }
        check(owner.release_sound_context(*context, f) && owner.unload(unload, f) && owner.close_when_idle(f), "all reservation Activities retire");
    }
    {
        auto owner = create(argv[1]);
        load_sync(owner, token);
        auto context = owner.create_sound_context(f);
        check(bool(context), "unload fixture context");
        Gate gate;
        std::thread unloading_thread([&] { unloading = {Edge::after, &gate}; PlaybackFailure retire; check(owner.unload(unload, retire), "actual unload commits"); });
        gate.await();
        check(!owner.play(*context, play, params, f) && f.kind == PlaybackError::bank_unloading, "same-bank play rejected until wrapper unload commits");
        check(!owner.unload(unload, f) && f.kind == PlaybackError::bank_unloading, "exact bank cannot unload twice in gap");
        check(!owner.close_when_idle(f) && f.kind == PlaybackError::operation_active, "unload Activity retains owner across gap");
        gate.release();
        unloading_thread.join();
        check(owner.release_sound_context(*context, f) && owner.close_when_idle(f), "actual unload and close retire");
    }
    {
        auto owner = create(argv[1]);
        auto context = owner.create_sound_context(f);
        check(bool(context), "close fixture context");
        Gate gate;
        std::thread closing_thread([&] { closing = {Edge::before, &gate}; PlaybackFailure retire; check(owner.close_when_idle(retire), "actual close commits"); });
        gate.await();
        check(!owner.play(*context, play, params, f) && f.kind == PlaybackError::closing, "play admission sealed before Service close");
        check(!owner.create_sound_context(f) && f.kind == PlaybackError::closing, "context admission sealed before Service close");
        check(!owner.load_sync(load(AudioBankOpcode::load_sync), {token}, f) && f.kind == PlaybackError::closing, "load admission sealed before Service close");
        gate.release();
        closing_thread.join();
        check(!owner.create_sound_context(f) && f.kind == PlaybackError::closed, "outer close committed after actual Service close");
    }
    {
        auto owner = create(argv[1]);
        load_sync(owner, token);
        load_sync(owner, other_token);
        auto context = owner.create_sound_context(f);
        check(bool(context), "uncertain unload fixture context");
        auto retained = owner.play(*context, play, params, f);
        check(retained && owner.stop(*retained, f), "inactive published handle retained during uncertain unload");
        auto domain = owner.resource_domain();
        unloading = {Edge::after, nullptr, true};
        check(!owner.unload(unload, f) && f.kind == PlaybackError::native_exception, "exception after actual Service unload");
        unloading = {};
        check(!owner.play(*context, play, params, f) && f.kind == PlaybackError::bank_state_unknown, "uncertain exact bank cannot reopen for play");
        check(!owner.unload(unload, f) && f.kind == PlaybackError::bank_state_unknown, "uncertain exact bank cannot be implicitly unloaded again");
        check(owner.resource_domain() == domain && owner.observe(*retained, f) && owner.release(*retained, f), "uncertain mutation preserves actual owner and inactive registry record");
        auto unrelated = owner.play(*context, command(AudioBankOpcode::play, other_token), params, f);
        check(unrelated && owner.stop(*unrelated, f) && owner.release(*unrelated, f), "unrelated exact bank can still prepare");
        check(owner.release_sound_context(*context, f) && owner.close_when_idle(f), "owner can explicitly close after exact-bank uncertainty");
    }
    {
        auto owner = create(argv[1]);
        auto domain = owner.resource_domain();
        closing = {Edge::after, nullptr, true};
        check(!owner.close_when_idle(f) && f.kind == PlaybackError::native_exception, "exception after actual Service close");
        closing = {};
        check(!owner.close_when_idle(f) && f.kind == PlaybackError::close_state_unknown, "uncertain close cannot implicitly reopen");
        check(!owner.create_sound_context(f) && f.kind == PlaybackError::close_state_unknown && owner.resource_domain() == domain,
              "uncertainty seals admission and retains resource owner");
    }
    std::puts("PASS native playback reservation pre/post-service gaps/64 slot growth/unpublished rollback/exact-bank and close uncertainty; injected CPU fixture only");
    return std::fflush(stdout) || std::ferror(stdout) ? 1 : 0;
}
