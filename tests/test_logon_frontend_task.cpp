#include "native_movie_audio_ports.hpp"
#include "mka/logon_frontend_task.hpp"
#include <functional>

using namespace mka::native_logon;
namespace game = mka::game;
namespace input = mka::input;
namespace {
struct Probe final : game::NativeFrontendTask {
    std::function<game::TaskUpdate(game::NativeFrontendExecution&, game::FrontendSchedulerFailure&)> run;
    std::function<void()> cleanup;
    Probe(decltype(run) r, decltype(cleanup) c) : run(std::move(r)), cleanup(std::move(c)) {}
    ~Probe() override { if (cleanup) cleanup(); }
    game::TaskUpdate update(game::NativeFrontendExecution& e, game::FrontendSchedulerFailure& f) override { return run(e, f); }
};
struct Observed final : game::NativeFrontendTask {
    std::unique_ptr<NativeLogonFrontendTask> actual;
    unsigned& destroyed; FrontendTaskError& terminal; bool running = false;
    Observed(std::unique_ptr<NativeLogonFrontendTask> task, unsigned& d, FrontendTaskError& error)
        : actual(std::move(task)), destroyed(d), terminal(error) {}
    ~Observed() override { check(!running, "executing LOGON survives through update return"); actual.reset(); ++destroyed; }
    game::TaskUpdate update(game::NativeFrontendExecution& e, game::FrontendSchedulerFailure& f) override {
        running = true; auto result = actual->update(e, f); terminal = actual->failure().kind; running = false; return result;
    }
};
}

int main(int argc, char** argv) { try {
    check(argc == 5, "usage: test_native_logon_frontend DLLdir asset_root LOGON_relative MOVIER.ovl");
    MovieAssetFailure asset_failure;
    auto store = NativeMovieAssetStore::create(std::filesystem::u8path(argv[2]), {{"LOGON.SFD", argv[3]}}, asset_failure);
    check(bool(store), "actual LOGON source association");
    std::ifstream original(std::filesystem::u8path(argv[4]), std::ios::binary);
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(original)), {});
    check(bytes.size() >= 0x77dd8 + 6144, "actual color source extent");
    MovieColorError color_error;
    auto color = NativeMovieColorTable::decode(MovieColorRole::helper_default, bytes.data() + 0x77dd8, 6144, color_error);
    check(bool(color), "source color data");
    enum class Case { normal, skip, gate_during_block, mode7_after_world, world_failure,
        missing_clear, frontend_failure, factory_exception, world_exception,
        missing_dependencies, movie_failure, cancel, clear_without_maintenance,
        wrong_maintenance_priority, foreign_maintenance_receipt, handled_reentry };
    unsigned scenarios = 0;
    for (auto scenario : {Case::normal, Case::skip, Case::gate_during_block, Case::mode7_after_world,
            Case::world_failure, Case::missing_clear, Case::frontend_failure, Case::factory_exception,
            Case::world_exception, Case::missing_dependencies, Case::movie_failure, Case::cancel,
            Case::clear_without_maintenance, Case::wrong_maintenance_priority, Case::foreign_maintenance_receipt, Case::handled_reentry}) {
        ++scenarios;
        auto data = std::make_shared<Data>(); auto gpu = std::make_shared<GPU>();
        std::int64_t ticks = 0; unsigned profiles = 0, restores = 0, destroyed = 0;
        MovieServiceFailure sf;
        auto service = NativeMovieService::create_with_native_clock(1,
            [&, data](const RetailMovieRequest& request, std::shared_ptr<const MovieSourceObservationIdentity> id,
                const std::optional<sound::AudioStreamFormat>& actual, MovieSessionFailure& f) -> std::optional<MovieServicePolicyConfiguration> {
                f = {}; ++profiles; check(request.id == 0 && id == data->source && actual &&
                    *actual == sound::AudioStreamFormat{data->rate, data->channels, 3}, "actual source/client policy join"); return profile(request, *data);
            },
            [&, data, gpu](const RetailMovieRequest& request, std::int64_t language, MovieSessionPolicyFactory policies,
                NativeMovieControlContext& control, MovieSessionFailure& f) -> std::unique_ptr<NativeMoviePlaybackSession> {
                MovieSourceFailure why;
                auto source = NativeRetailMovieSource::open(*store, std::filesystem::u8path(argv[1]), request.id, request.requested_view, language, why);
                if (!source) { f.kind = MovieSessionError::source; f.source = std::move(why); return {}; }
                auto audio = source->audio_present(why); check(audio && *audio, "actual LOGON audio presence");
                data->source = source->observation_identity(); MovieControlFailure cf;
                auto source_input = own_movie_playback_source(std::move(source), cf); check(bool(source_input), "actual source ingress");
                std::deque<MovieReadResult> reads;
                for (;;) {
                    auto r = source_input->next(cf); check(r.status == MovieReadStatus::frame && cf.kind == MovieControlError::none, "actual prefill metadata");
                    const auto* a = std::get_if<MovieAudioFrame>(&r.frame);
                    if (a) { check(a->channels.size() == 2 && a->sample_rate > 0, "explicit fixture two-plane routing");
                        data->rate = a->sample_rate; data->channels = static_cast<std::uint32_t>(a->channels.size()); data->layout = a->channel_layout; data->capacity = std::max(1u, data->rate / 20); }
                    const bool found = a != nullptr; reads.push_back(std::move(r)); ++data->prefetched; if (found) break;
                }
                auto concrete = policies(request, data->source, sound::AudioStreamFormat{data->rate, data->channels, 3}, f); if (!concrete) return {};
                MoviePlaybackConfiguration config; config.audio = MoviePlaybackAudioPresence::required;
                config.client_sample_rate = data->rate; config.client_channels = data->channels; config.client_channel_mask = 3;
                config.layout = {1, 640, 448, MovieAspectMode::explicit_display_aspect, 4, 3, MovieWindowPolicy::centered_fit};
                config.filter = MovieFilter::nearest; config.queued_byte_budget = 8 * 1024 * 1024;
                return NativeMoviePlaybackSession::create_from_ports(std::make_unique<BufferedInput>(std::move(source_input), std::move(reads), data),
                    std::move(concrete), std::make_unique<Audio>(data), gpu, *color, config, control, f);
            }, [&, data](std::string_view key, std::shared_ptr<const MovieSourceObservationIdentity> id, MovieHostClockFailure& f) {
                return NativeMovieHostClock::create_with_reader(key, std::move(id), 1000,
                    [&] { return std::optional<std::int64_t>{ticks}; }, f);
            }, sf);
        check(bool(service), "actual native Service");
        input::ControllerInput host; check(host.publish(0, input::digital_state(0, true)) && host.publish(1, input::digital_state(0, true)), "host samples");
        input::GameActionFailure af; const std::map<std::uint32_t, std::uint32_t> maps{{11, 0x800}, {6, 0x40}};
        auto actions = input::NativeGameActionInput::create({{0, maps, true, true}, {1, maps, true, false}}, false, af);
        check(actions && actions->add_unassigned_record(2, af), "owned explicit native game roles");
        input::GameInputTaskFailure tf; auto input_tasks = input::NativeGameInputTasks::create(tf); check(bool(input_tasks), "native task restore owner");
        game::GameTransitionFailure gf;
        auto flow = game::NativeGameTransitionState::create({{3, 21}, 2, 3, 9}, gf); check(bool(flow), "explicit game state ingress");
        std::vector<int> events; events.reserve(128); unsigned sequence = 1;
        game::FrontendSchedulerFailure failure;
        auto scheduler = game::NativeFrontendScheduler::create(*actions, 0, failure); check(bool(scheduler), "actual native scheduler");
        auto ui = input_tasks->create_task(input::GameInputTaskKind::kind208b, false, tf); check(bool(ui), "owned existing UI restore state");
        FrontendEffects effects;
        effects.prepare_game_world = [&](EffectFailure& f) { f = {}; events.push_back(1); return true; };
        effects.surround_movie = [&](EffectFailure& f) { f = {}; events.push_back(2); return true; };
        effects.finish_surround_movie = [&](EffectFailure& f) { f = {}; ++restores; events.push_back(3); return true; };
        NativeLogonFrontendTask* running_task = nullptr;
        effects.teardown_game_world = [&](game::NativeFrontendExecution& execution, EffectFailure& f) -> std::optional<game::FrontendTaskToken> {
            f = {}; events.push_back(4); check(destroyed == 0 && restores == 1, "world follows movie cleanup while current task lives");
            auto current = flow->snapshot(gf); check(current && current->contexts == std::vector<std::int32_t>{1} &&
                current->transition_selector == 0 && current->active_render_view_count == 3, "actual context/selector published before world; views after world");
            auto saved = input_tasks->get(*ui, tf); check(saved && saved->saved_restore == (scenario != Case::gate_during_block), "same source suspension state reaches existing owned task");
            if (scenario == Case::world_failure) { f = {"fixture unimplemented world body", 94}; return {}; }
            if (scenario == Case::world_exception) throw std::runtime_error("fixture world exception");
            if (scenario == Case::missing_clear) return {};
            if (scenario == Case::handled_reentry) {
                game::FrontendSchedulerFailure nested;
                check(running_task && running_task->update(execution, nested) == game::TaskUpdate::error &&
                    nested.kind == game::FrontendSchedulerError::invalid_state && running_task->failure().kind == FrontendTaskError::none,
                    "handled nested update rejects locally without poisoning the active outer task");
            }
            check(execution.clear_for_transition(failure), "actual source-point native task clear");
            check(!input_tasks->get(*ui, tf) && tf.kind == input::GameInputTaskError::stale_task, "clearing actual UI task releases its restore record");
            if (scenario == Case::clear_without_maintenance) return {};
            std::unique_ptr<game::NativeFrontendTask> maintenance = std::make_unique<Probe>(
                [&](auto&, auto& why) { why = {}; events.push_back(16); return game::TaskUpdate::yield; }, [&] { events.push_back(-16); });
            std::optional<game::FrontendTaskToken> receipt;
            if (scenario == Case::foreign_maintenance_receipt) {
                auto foreign = game::NativeFrontendScheduler::create(*actions, 0, failure); check(bool(foreign), "actual foreign native scheduler");
                receipt = foreign->enqueue(maintenance, 16, false, failure);
            } else receipt = execution.enqueue(maintenance, scenario == Case::wrong_maintenance_priority ? 17 : 16, false, failure);
            check(receipt.has_value(), "authentic required fixture maintenance registration receipt");
            if (scenario == Case::mode7_after_world) check(flow->set_main_mode(7, gf) && flow->set_active_render_view_count(5, gf), "actual world changes the SAME main-mode/view owner");
            events.push_back(5); return receipt;
        };
        effects.frontend_task = [&](EffectFailure& f) -> std::unique_ptr<game::NativeFrontendTask> {
            f = {}; events.push_back(6); check(sequence == 1, "transition never resets or executes the existing frontend sequence");
            auto current = flow->snapshot(gf); check(current && current->active_render_view_count == (scenario == Case::mode7_after_world ? 5 : 1), "conditional view count reads mode AFTER world");
            if (scenario == Case::frontend_failure) { f = {"fixture next frontend body unavailable", 95}; return {}; }
            if (scenario == Case::factory_exception) throw std::runtime_error("fixture next frontend factory exception");
            return std::make_unique<Probe>([&](auto&, auto& why) { why = {}; check(sequence == 1, "frontend deferred to next main iteration"); ++sequence; events.push_back(32); return game::TaskUpdate::finished; }, [&] { events.push_back(-32); });
        };
        if (scenario == Case::missing_dependencies) effects.frontend_task = {};
        FrontendTaskFailure task_failure;
        auto actual = NativeLogonFrontendTask::create(*service, *actions, host, *input_tasks, *flow,
            {MovieViewSelection::normal, 0, 1}, std::move(effects), task_failure);
        if (scenario == Case::missing_dependencies) { check(!actual && task_failure.kind == FrontendTaskError::missing_effect && profiles == 0 && events.empty(), "missing real game dependencies reject before acquisition"); continue; }
        check(bool(actual), "actual native queued LOGON caller"); FrontendTaskError terminal = FrontendTaskError::none;
        running_task = actual.get();
        std::unique_ptr<game::NativeFrontendTask> task = std::make_unique<Observed>(std::move(actual), destroyed, terminal);
        auto token = scheduler->enqueue(task, 32, false, failure); check(bool(token) && !task, "actual task ownership transferred");
        std::unique_ptr<game::NativeFrontendTask> old_ui = std::make_unique<Probe>(
            [&](auto&, auto& why) { why = {}; events.push_back(99); return game::TaskUpdate::finished; },
            [&] { check(input_tasks->retire(*ui, tf), "actual UI task destructor retires owned restore state"); events.push_back(-99); });
        check(scheduler->enqueue(old_ui, 64, false, failure).has_value(), "pending game task behind blocking caller");
        bool armed = false, done = false; game::FrontendDispatchResult last;
        for (unsigned polls = 0; polls < 20000 && !done; ++polls) {
            data->advance(); ticks += 10;
            if (data->started && !armed) {
                armed = true;
                if (scenario == Case::skip) check(host.publish(0, input::digital_state(0x40, true)), "actual native mapped movie skip");
                if (scenario == Case::gate_during_block) check(actions->set_suspend_inhibit(true, af) && scheduler->set_suspension_count(7, failure), "change new-task gates during already-admitted blocking call");
                if (scenario == Case::movie_failure) data->clock_error = true;
                if (scenario == Case::cancel) { check(scheduler->cancel(*token, failure), "owner cancels blocked native task"); check(destroyed == 1 && restores == 1 && !service->request_view(sf), "cancel destroys movie owner/restores once"); done = true; break; }
            }
            last = scheduler->dispatch(failure);
            check(std::find(events.begin(), events.end(), 99) == events.end(), "ordinary game tasks never interleave inside original blocking movie call");
            if (last.progress == game::FrontendDispatchProgress::blocked) { check(destroyed == 0 && profiles == 1, "blocked task/pass stays owned"); continue; }
            done = true;
        }
        check(done, "finite original LOGON native lifecycle");
        if (scenario == Case::cancel) { scheduler.reset(); continue; }
        const bool error = scenario == Case::world_failure || scenario == Case::world_exception || scenario == Case::missing_clear ||
            scenario == Case::frontend_failure || scenario == Case::factory_exception || scenario == Case::movie_failure ||
            scenario == Case::clear_without_maintenance || scenario == Case::wrong_maintenance_priority || scenario == Case::foreign_maintenance_receipt;
        if (error) {
            check(last.progress == game::FrontendDispatchProgress::error && failure.kind != game::FrontendSchedulerError::none && terminal != FrontendTaskError::none, "actual native task failures are named");
            if (scenario == Case::world_failure) check(failure.task_code == 94 && terminal == FrontendTaskError::world, "preserve actual world failure cause");
            if (scenario == Case::frontend_failure) check(failure.task_code == 95 && terminal == FrontendTaskError::frontend, "preserve actual frontend factory cause");
            if (scenario == Case::movie_failure) check(failure.task_code == 72 && restores == 1 && terminal == FrontendTaskError::movie, "preserve actual clock failure and cleanup");
            if (scenario == Case::clear_without_maintenance || scenario == Case::wrong_maintenance_priority || scenario == Case::foreign_maintenance_receipt)
                check(terminal == FrontendTaskError::missing_maintenance && flow->snapshot(gf)->active_render_view_count == 3 &&
                    std::find(events.begin(), events.end(), 6) == events.end(), "missing/wrong/foreign maintenance cannot pass registration gate or call next frontend factory");
            if (scenario == Case::frontend_failure || scenario == Case::factory_exception) check(!actions->snapshot(af)->front().dispatch_blocked, "source resume still executes after frontend allocation/factory failure");
            scheduler.reset(); continue;
        }
        check(last.progress == game::FrontendDispatchProgress::transition && failure.kind == game::FrontendSchedulerError::none && destroyed == 1 && restores == 1 && sequence == 1, "actual transition aborts pass and retires current only after return");
        check(std::find(events.begin(), events.end(), 16) == events.end() && std::find(events.begin(), events.end(), 32) == events.end(), "new source tasks never run in abandoned pass");
        check(!actions->snapshot(af)->front().dispatch_blocked && !service->request_view(sf), "source resume and actual Service teardown precede handoff");
        if (scenario == Case::skip) check(actions->intro_skip() && data->total_accepted < 243201, "actual skip fades before normal drain");
        else check(data->total_accepted == 243201 && gpu->presents == 152 && gpu->retired == 152, "actual normal LOGON resource/control completion through deterministic ports");
        if (scenario == Case::gate_during_block) {
            auto gated = scheduler->dispatch(failure); check(gated.progress == game::FrontendDispatchProgress::pass_complete && gated.updates == 0 && sequence == 1, "new-task gates apply after handoff");
            check(actions->set_suspend_inhibit(false, af) && scheduler->set_suspension_count(0, failure), "explicit native gate release");
        }
        auto next = scheduler->dispatch(failure); check(next.progress == game::FrontendDispatchProgress::pass_complete && sequence == 2, "application explicitly begins the next source iteration");
        auto m = std::find(events.begin(), events.end(), 16), n = std::find(events.begin(), events.end(), 32);
        check(m != events.end() && n != events.end() && m < n, "native source business priority16 precedes32");
        scheduler.reset();
    }
    check(scenarios == 16, "all declared queued caller scenarios executed");
    std::puts("PASS native queued LOGON blocking/transition/game state/owned jobs; deterministic ports and explicit game-effect probes"); return 0;
} catch (const std::exception& e) { std::fprintf(stderr, "FAIL native queued LOGON %s\n", e.what()); return 1; } }
