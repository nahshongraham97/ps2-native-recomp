#include "mka/movie_playback_session.hpp"
#include <bit>
#include <exception>
#include <new>

namespace mka::video {
namespace {
void named(MovieSessionFailure& f, MovieSessionError e, const char* operation) noexcept {
    f = {}; f.kind = e; f.operation = operation;
}
bool control_failure(MovieSessionFailure& f, MovieControlFailure value, const char* operation) noexcept {
    named(f, MovieSessionError::control, operation);
    f.control = value.kind == MovieControlError::none
        ? MovieControlFailure{MovieControlError::native_service, operation, 0} : value;
    return false;
}
}
struct NativeMoviePlaybackSession::CallGuard {
    NativeMoviePlaybackSession& session;
    bool entered = false;
    CallGuard(NativeMoviePlaybackSession& s, MovieSessionFailure& f, bool closed = false) : session(s) {
        if (s.access(f, closed)) { s.in_call_ = true; entered = true; }
    }
    ~CallGuard() { if (entered) session.in_call_ = false; }
    explicit operator bool() const noexcept { return entered; }
};
NativeMoviePlaybackSession::NativeMoviePlaybackSession(std::shared_ptr<NativeMovieGpuBackend> gpu,
        RetailMovieRequest request, std::unique_ptr<NativeMovieStreamPlayback> playback,
        NativeMovieControlContext& context)
    : gpu_(std::move(gpu)), request_(std::move(request)), playback_(std::move(playback)),
      controller_(std::make_unique<NativeMovieController>(*playback_, context)), context_(context) {}

std::unique_ptr<NativeMoviePlaybackSession> NativeMoviePlaybackSession::open(
        const NativeMovieAssetStore& store, const std::filesystem::path& libraries,
        std::int64_t id, MovieViewSelection view, std::int64_t language,
        MovieSessionPolicyFactory factory, std::unique_ptr<sound::NativeAudioDevice> audio,
        std::shared_ptr<NativeMovieGpuBackend> gpu, NativeMovieColorTable color,
        MoviePlaybackConfiguration config, NativeMovieControlContext& context, MovieSessionFailure& f) noexcept {
    f = {};
    try {
        if (!factory) { named(f, MovieSessionError::invalid_owner, "movie session requires source policy factory"); return {}; }
        MovieSourceFailure sf;
        auto source = NativeRetailMovieSource::open(store, libraries, id, view, language, sf);
        if (!source) { named(f, MovieSessionError::source, "movie session source open"); f.source = std::move(sf); return {}; }
        MovieControlFailure cf;
        std::unique_ptr<MoviePlaybackAudioOutput> output;
        std::optional<sound::AudioStreamFormat> actual;
        if (audio) {
            output = own_movie_playback_audio(std::move(audio), cf);
            if (!output) { control_failure(f, cf, "movie session audio adapter"); return {}; }
            actual = output->format(cf);
            if (!actual || cf.kind != MovieControlError::none) {
                control_failure(f, cf, "movie session actual audio format"); return {};
            }
        }
        auto policies = factory(*source->request(), source->observation_identity(), actual, f);
        if (!policies || f.kind != MovieSessionError::none) {
            if (f.kind == MovieSessionError::none)
                named(f, MovieSessionError::control, "movie session source policy factory rejected");
            return {};
        }
        auto input = own_movie_playback_source(std::move(source), cf);
        if (!input) { control_failure(f, cf, "movie session source adapter"); return {}; }
        return create_from_ports(std::move(input), std::move(policies), std::move(output),
            std::move(gpu), std::move(color), std::move(config), context, f);
    } catch (const std::bad_alloc&) { named(f, MovieSessionError::allocation, "movie session open allocation"); }
      catch (...) { named(f, MovieSessionError::control, "movie session open exception"); }
    return {};
}
std::unique_ptr<NativeMoviePlaybackSession> NativeMoviePlaybackSession::create_from_ports(
        std::unique_ptr<MoviePlaybackInput> input, std::unique_ptr<MoviePlaybackPolicies> policies,
        std::unique_ptr<MoviePlaybackAudioOutput> audio, std::shared_ptr<NativeMovieGpuBackend> gpu,
        NativeMovieColorTable color, MoviePlaybackConfiguration config,
        NativeMovieControlContext& context, MovieSessionFailure& f) noexcept {
    f = {};
    try {
        if (!gpu || !input) { named(f, MovieSessionError::invalid_owner, "movie session owned source/GPU"); return {}; }
        const auto base = context.current_base_volume();
        if (config.initial_base_volume && std::bit_cast<std::uint32_t>(*config.initial_base_volume) !=
                std::bit_cast<std::uint32_t>(base)) {
            control_failure(f, {MovieControlError::invalid_volume, "movie session conflicting initial base", 0},
                "movie session initial context join"); return {};
        }
        config.initial_base_volume = base;
        auto request = input->request();
        MovieControlFailure cf;
        auto stream = NativeMovieStreamPlayback::create(std::move(input), std::move(policies),
            std::move(audio), *gpu, std::move(color), std::move(config), cf);
        if (!stream) { control_failure(f, cf, "movie session stream create"); return {}; }
        return std::unique_ptr<NativeMoviePlaybackSession>(new NativeMoviePlaybackSession(
            std::move(gpu), std::move(request), std::move(stream), context));
    } catch (const std::bad_alloc&) { named(f, MovieSessionError::allocation, "movie session owner allocation"); }
      catch (...) { named(f, MovieSessionError::control, "movie session create exception"); }
    return {};
}
bool NativeMoviePlaybackSession::access(MovieSessionFailure& f, bool allow_closed) const noexcept {
    f = {};
    if (std::this_thread::get_id() != owner_) { named(f, MovieSessionError::wrong_thread, "movie session owner thread"); return false; }
    if (in_call_) { named(f, MovieSessionError::invalid_state, "movie session operation reentry"); return false; }
    if (closed_ && !allow_closed) { named(f, MovieSessionError::invalid_owner, "movie session closed"); return false; }
    return true;
}
const RetailMovieRequest* NativeMoviePlaybackSession::request(MovieSessionFailure& f) const noexcept {
    return access(f) ? &request_ : nullptr;
}
bool NativeMoviePlaybackSession::start(std::uint64_t fade, std::function<std::int64_t()> skip,
        MovieSessionFailure& f) noexcept {
    CallGuard call(*this, f); if (!call) return false;
    if (started_) { named(f, MovieSessionError::invalid_state, "movie session is single use"); return false; }
    try {
        context_.install_skip_if_present(std::move(skip));
        started_ = true;
        MovieControlFailure cf;
        if (!controller_->start(request_.logical_key, fade, cf)) return control_failure(f, cf, "movie session controller start");
        return true;
    } catch (...) { named(f, MovieSessionError::control, "movie session start exception"); return false; }
}
MovieControlResult NativeMoviePlaybackSession::poll() noexcept {
    MovieSessionFailure f; CallGuard call(*this, f);
    if (!call) return {MovieControlProgress::error, MovieControlCompletion::none,
        {f.kind == MovieSessionError::wrong_thread ? MovieControlError::wrong_thread : MovieControlError::invalid_state, f.operation, 0}, {}};
    if (!started_) return {MovieControlProgress::error, MovieControlCompletion::none,
        {MovieControlError::invalid_state, "movie session poll before start", 0}, {}};
    if (terminal_) return *terminal_;
    auto result = controller_->poll();
    if (result.progress != MovieControlProgress::ongoing) terminal_ = result;
    return result;
}
bool NativeMoviePlaybackSession::stop(MovieSessionFailure& f) noexcept {
    CallGuard call(*this, f); if (!call) return false;
    MovieControlFailure cf;
    if (!controller_->stop(cf)) return control_failure(f, cf, "movie session controller stop");
    if (!terminal_ && started_ && controller_->state() == MovieControlState::idle)
        terminal_ = MovieControlResult{MovieControlProgress::complete, MovieControlCompletion::caller_stop, {}, {}};
    return true;
}
bool NativeMoviePlaybackSession::snapshot(MoviePlaybackSnapshot& out, MovieSessionFailure& f) noexcept {
    CallGuard call(*this, f); if (!call) return false;
    MoviePlaybackSnapshot staged; MovieControlFailure cf;
    if (!playback_->snapshot(staged, cf)) return control_failure(f, cf, "movie session snapshot");
    out = std::move(staged); return true;
}
bool NativeMoviePlaybackSession::close_owned(MovieSessionFailure& f) noexcept {
    f = {};
    if (closed_) return true;
    MovieControlFailure cf;
    if (playback_ && !playback_->stop(cf)) control_failure(f, cf, "movie session resource stop");
    controller_.reset();
    playback_.reset(); // Includes last fence-retirement attempt, while GPU retained.
    closed_ = true;
    gpu_.reset();
    return f.kind == MovieSessionError::none;
}
bool NativeMoviePlaybackSession::close(MovieSessionFailure& f) noexcept {
    CallGuard call(*this, f, true); if (!call) return false;
    return close_owned(f);
}
NativeMoviePlaybackSession::~NativeMoviePlaybackSession() {
    if (std::this_thread::get_id() != owner_ || in_call_) std::terminate();
    in_call_ = true;
    MovieSessionFailure ignored; close_owned(ignored);
}
} // namespace mka::video
