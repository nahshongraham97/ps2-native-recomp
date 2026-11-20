#include "mka/movie_playback_session.hpp"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <stdexcept>

using namespace mka::video;
namespace {
void check(bool value, const char* why) { if (!value) throw std::runtime_error(why); }
struct Trace {
    std::vector<std::string> events;
    unsigned stops = 0, metadata = 0, reads = 0, submissions = 0;
    bool allow_end = false, stop_error = false;
};
struct Input final : MoviePlaybackInput {
    RetailMovieRequest value;
    Trace& trace;
    std::shared_ptr<const MovieSourceObservationIdentity> identity = std::make_shared<const MovieSourceObservationIdentity>();
    explicit Input(Trace& t) : trace(t) {
        MovieCatalogFailure f;
        auto r = RetailMovieCatalog{}.resolve(0, MovieViewSelection::normal, 0, f);
        check(bool(r), "fixture semantic retail request"); value = std::move(*r);
    }
    ~Input() override { trace.events.push_back("input_destroy"); }
    const RetailMovieRequest& request() const noexcept override { return value; }
    std::shared_ptr<const MovieSourceObservationIdentity> observation_identity() const noexcept override { return identity; }
    MovieReadResult next(MovieControlFailure& f) override {
        f = {}; ++trace.reads;
        if (trace.reads > 1) return {MovieReadStatus::end, {}};
        MovieVideoFrame frame; frame.width = 2; frame.height = 2;
        frame.planes[0] = {16, 32, 64, 128}; frame.planes[1] = {128}; frame.planes[2] = {128};
        return {MovieReadStatus::frame, std::move(frame)};
    }
};
struct Policies final : MoviePlaybackPolicies {
    Trace& trace;
    explicit Policies(Trace& t) : trace(t) {}
    ~Policies() override { trace.events.push_back("policy_destroy"); }
    bool validate_observation_source(std::shared_ptr<const MovieSourceObservationIdentity> p, MovieControlFailure& f) override { f = {}; return bool(p); }
    bool observe_source(const MovieSourceObservationBatch&, MovieControlFailure& f) override { f = {}; return true; }
    bool validate_audio_format(const std::optional<mka::sound::AudioStreamFormat>& format, MovieControlFailure& f) override { f = {}; return !format; }
    bool begin(const RetailMovieRequest& r, std::int32_t gain, MovieControlFailure& f) override { f = {}; check(r.logical_key == "LOGON.SFD" && gain == 0, "explicit fixture initial gain/request"); trace.events.push_back("begin"); return true; }
    bool pump_metadata(MovieControlFailure& f) override { f = {}; ++trace.metadata; return true; }
    bool pause(bool value, MovieControlFailure& f) override { f = {}; trace.events.push_back(value ? "pause" : "resume"); return true; }
    bool ready(const MoviePlaybackSnapshot&, bool& ready, MovieControlFailure& f) override { f = {}; ready = true; return true; }
    bool eligible(const MovieVideoFrame&, const MoviePlaybackSnapshot&, bool& yes, MovieControlFailure& f) override { f = {}; yes = true; return true; }
    bool video_submitted(const MovieGpuMetadata&, MovieControlFailure& f) override { f = {}; ++trace.submissions; return true; }
    bool prepare_audio(const MovieAudioFrame&, std::int32_t, std::vector<float>&, MovieControlFailure& f) override { f = {MovieControlError::native_service, "unexpected fixture audio", 0}; return false; }
    bool audio_accepted(const MoviePlaybackAudioAcceptance&, MovieControlFailure& f) override { f = {MovieControlError::native_service, "unexpected fixture acceptance", 0}; return false; }
    bool set_gain(std::int32_t gain, MovieControlFailure& f) override { f = {}; trace.events.push_back("gain:" + std::to_string(gain)); return true; }
    bool end_admitted(const MoviePlaybackSnapshot&, bool& yes, MovieControlFailure& f) override { f = {}; yes = trace.allow_end; return true; }
    bool stop(MovieControlFailure& f) override { ++trace.stops; trace.events.push_back("stop"); f = trace.stop_error ? MovieControlFailure{MovieControlError::native_service, "fixture stop failure", 7} : MovieControlFailure{}; return !trace.stop_error; }
};
struct Frame final : MovieGpuFrame {
    MovieGpuMetadata data;
    std::uint32_t w, h;
    explicit Frame(const MovieRgbaFrame& f) : w(f.width), h(f.height) { data.conversion_role = f.conversion_role; }
    std::uint64_t identity() const noexcept override { return 1; }
    std::uint32_t width() const noexcept override { return w; }
    std::uint32_t height() const noexcept override { return h; }
    const MovieGpuMetadata& metadata() const noexcept override { return data; }
};
struct Fence final : MovieGpuFence { explicit Fence(NativeMoviePresentPlan p) : MovieGpuFence(std::move(p)) {} };
struct GPU final : NativeMovieGpuBackend {
    Trace& trace;
    explicit GPU(Trace& t) : trace(t) {}
    ~GPU() override { trace.events.push_back("gpu_destroy"); }
    MovieGpuUpload upload(const MovieRgbaFrame& f) override { return {MovieGpuError::none, std::make_shared<Frame>(f)}; }
    MovieGpuSubmission present(const NativeMoviePresentPlan& p, const MovieHostLayout&) override { trace.events.push_back("present"); return {MovieGpuError::none, std::make_shared<Fence>(p)}; }
    MovieFenceResult poll(const std::shared_ptr<MovieGpuFence>&) override { return {MovieFenceState::pending, MovieGpuError::none}; }
    void retire(std::shared_ptr<MovieGpuFence> f) noexcept override { if (f) trace.events.push_back("retire"); }
};
MoviePlaybackConfiguration configuration() {
    MoviePlaybackConfiguration c; c.audio = MoviePlaybackAudioPresence::absent;
    c.layout = {1, 640, 448, MovieAspectMode::explicit_display_aspect, 4, 3, MovieWindowPolicy::centered_fit};
    c.filter = MovieFilter::nearest; c.queued_byte_budget = 1024; c.initial_base_volume = 1.0f; return c;
}
std::unique_ptr<NativeMoviePlaybackSession> make(Trace& t, const NativeMovieColorTable& color,
        NativeMovieControlContext& context, std::weak_ptr<GPU>& weak) {
    auto gpu = std::make_shared<GPU>(t); weak = gpu; MovieSessionFailure f;
    auto session = NativeMoviePlaybackSession::create_from_ports(std::make_unique<Input>(t),
        std::make_unique<Policies>(t), {}, gpu, color, configuration(), context, f);
    check(bool(session) && f.kind == MovieSessionError::none, "actual stream/controller session create");
    return session;
}
}
int main(int argc, char** argv) { try {
    check(argc == 2, "usage: test_native_movie_playback_session original_MOVIER.ovl");
    std::ifstream source(std::filesystem::u8path(argv[1]), std::ios::binary);
    check(bool(source), "explicit original color table source");
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(source)), {});
    check(bytes.size() >= 0x77dd8 + 6144, "original table extent");
    MovieColorError ce; auto color = NativeMovieColorTable::decode(MovieColorRole::helper_default,
        bytes.data() + 0x77dd8, 6144, ce); check(bool(color), "exact existing color-table decoder");
    {
        Trace t; NativeMovieControlContext context(1.0f); MovieSessionFailure f;
        MovieAssetFailure af;
        auto store = NativeMovieAssetStore::create(std::filesystem::absolute(std::filesystem::u8path(argv[1])).parent_path(), {}, af);
        check(bool(store), "read-only empty explicit catalog for source failure ordering");
        unsigned calls = 0;
        MovieSessionPolicyFactory factory = [&](const RetailMovieRequest&,
                std::shared_ptr<const MovieSourceObservationIdentity>,
                const std::optional<mka::sound::AudioStreamFormat>&, MovieSessionFailure&) {
            ++calls; return std::make_unique<Policies>(t);
        };
        auto gpu = std::make_shared<GPU>(t);
        check(!NativeMoviePlaybackSession::open(*store, "absent-library", 33, MovieViewSelection::normal,
            0, factory, {}, gpu, *color, configuration(), context, f) &&
            f.kind == MovieSessionError::source && f.source.kind == MovieSourceError::catalog && calls == 0,
            "semantic invalid request rejects before policy factory or codec");
        check(!NativeMoviePlaybackSession::open(*store, "absent-library", 0, MovieViewSelection::normal,
            0, factory, {}, gpu, *color, configuration(), context, f) &&
            f.kind == MovieSessionError::source && f.source.kind == MovieSourceError::asset &&
            f.source.asset_failure.kind == MovieAssetError::unknown_key && calls == 0,
            "missing exact source preserves nested error before factory or codec");
    }
    {
        Trace t; NativeMovieControlContext context(1.0f); MovieSessionFailure f;
        auto gpu = std::make_shared<GPU>(t); auto c = configuration(); c.initial_base_volume = 0.5f;
        check(!NativeMoviePlaybackSession::create_from_ports(std::make_unique<Input>(t),
            std::make_unique<Policies>(t), {}, gpu, *color, c, context, f) &&
            f.control.kind == MovieControlError::invalid_volume && t.stops == 0,
            "conflicting initial context rejected before policy/control");
        c.initial_base_volume.reset();
        auto owner = NativeMoviePlaybackSession::create_from_ports(std::make_unique<Input>(t),
            std::make_unique<Policies>(t), {}, gpu, *color, c, context, f);
        check(bool(owner) && owner->start(0, {}, f), "missing duplicate config value uses actual context base");
        check(owner->close(f), "context join cleanup");
    }
    {
        Trace t; NativeMovieControlContext context(1.0f); std::weak_ptr<GPU> weak;
        auto session = make(t, *color, context, weak); MovieSessionFailure f;
        check(!weak.expired() && session->request(f)->logical_key == "LOGON.SFD", "retained GPU and owned semantic request");
        check(session->poll().failure.kind == MovieControlError::invalid_state && t.reads == 0, "no source pump before start");
        unsigned probes = 0;
        check(session->start(4, [&]() -> std::int64_t {
            MovieSessionFailure nested; MoviePlaybackSnapshot snapshot;
            check(!session->snapshot(snapshot, nested) && nested.kind == MovieSessionError::invalid_state, "snapshot callback reentry rejected");
            check(!session->stop(nested) && nested.kind == MovieSessionError::invalid_state, "stop callback reentry rejected");
            ++probes; return 1;
        }, f), "semantic start");
        check(t.reads == 0 && !session->start(4, {}, f) && f.kind == MovieSessionError::invalid_state, "single use and pause before read");
        check(session->poll().progress == MovieControlProgress::ongoing && probes == 1 && t.submissions == 1, "controller presentation then exact skip");
        check(session->stop(f) && t.stops == 0, "retail stop while fading preserves fade");
        for (unsigned i = 0; i < 4; ++i) check(session->poll().progress == MovieControlProgress::ongoing, "last fade write still ongoing");
        auto end = session->poll(); check(end.progress == MovieControlProgress::complete && end.completion == MovieControlCompletion::skipped, "fade completion via real controller");
        const auto count = t.metadata; check(session->poll().completion == MovieControlCompletion::skipped && t.metadata == count, "terminal poll stable without repump");
        check(std::count(t.events.begin(), t.events.end(), "retire") == 1 && t.stops == 1, "fence retired once at completion stop");
        check(session->close(f) && weak.expired() && session->close(f), "explicit close releases retained GPU idempotently");
        check(!session->request(f) && f.kind == MovieSessionError::invalid_owner, "closed request rejected");
        const auto gpu = std::find(t.events.begin(), t.events.end(), "gpu_destroy");
        check(std::find(t.events.begin(), gpu, "retire") != gpu && std::find(t.events.begin(), gpu, "input_destroy") != gpu && std::find(t.events.begin(), gpu, "policy_destroy") != gpu, "stream resources released before GPU");
    }
    {
        Trace t; NativeMovieControlContext context(1.0f); std::weak_ptr<GPU> weak;
        auto session = make(t, *color, context, weak); MovieSessionFailure f;
        MovieSessionFailure foreign; bool accepted = true;
        std::thread other([&] { accepted = session->start(0, {}, foreign); }); other.join();
        check(!accepted && foreign.kind == MovieSessionError::wrong_thread && t.events.empty(), "foreign thread rejected before any policy call");
        check(session->start(0, []() -> std::int64_t { return 0x100000001LL; }, f), "exact wide skip start");
        check(session->poll().progress == MovieControlProgress::ongoing, "wide callback is not exact skip1");
        check(session->poll().progress == MovieControlProgress::ongoing, "decoder EOF cannot replace pending fence/end policy");
        MoviePlaybackSnapshot s; check(session->snapshot(s, f) && s.outstanding_fences == 1, "actual pending fence owned");
        t.stop_error = true; check(!session->close(f) && f.control.native_code == 7 && weak.expired(), "cleanup failure preserved while all resources released");
        check(std::count(t.events.begin(), t.events.end(), "retire") == 1, "failure cleanup retires pending fence");
    }
    {
        Trace t; NativeMovieControlContext context(1.0f); std::weak_ptr<GPU> weak;
        { auto session = make(t, *color, context, weak); MovieSessionFailure f;
          check(session->start(0, {}, f) && session->poll().progress == MovieControlProgress::ongoing, "destructor cleanup setup"); }
        check(weak.expired() && std::count(t.events.begin(), t.events.end(), "retire") == 1, "destructor stops stream before GPU release");
    }
    std::puts("PASS native playback session real stream/controller lifecycle with deterministic ports; no devices or retail policy admission");
    return 0;
} catch (const std::exception& e) { std::fprintf(stderr, "FAIL session %s\n", e.what()); return 1; } }
