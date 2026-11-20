#include "mka/movie_output_position.hpp"
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>

using namespace mka::video;
namespace { int allocation_after = -1; }
void* operator new(std::size_t n) {
    if (allocation_after == 0) { allocation_after = -1; throw std::bad_alloc(); }
    if (allocation_after > 0) --allocation_after;
    if (auto* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
namespace {
void check(bool v, const char* why) { if (!v) throw std::runtime_error(why); }
constexpr auto profile = MovieOutputPositionProfile::successful_frame_no_repeat_no_skip;
MoviePictureSource selected(std::uint64_t byte) {
    MoviePictureSource p;
    p.picture_es_byte = byte;
    p.pes = {0xe0, 100, 110, 1000, 0, 890, 400, {}};
    p.quarter_pts = 100; return p;
}
std::shared_ptr<MovieSourceSyntaxPicture> header(const std::string& key,
        const std::shared_ptr<const MovieSourceObservationIdentity>& identity) {
    auto p = std::make_shared<MovieSourceSyntaxPicture>(); p->source_key = key; p->identity = identity;
    p->picture_es_byte = 800; p->picture.coding_type = 2; p->coding.picture_structure = 3;
    p->pes = selected(800).pes; return p;
}
std::unique_ptr<NativeMovieOutputPosition> owner(const std::string& key,
        const std::shared_ptr<const MovieSourceObservationIdentity>& identity) {
    MovieOutputPositionFailure f;
    auto p = NativeMovieOutputPosition::create(key, identity, profile, f);
    check(bool(p) && f.kind == MovieOutputPositionError::none, "explicit owner construction"); return p;
}
}
int main() { try {
    const std::string key = "LOGON.SFD";
    auto identity = std::make_shared<const MovieSourceObservationIdentity>();
    auto current = header(key, identity); MovieOutputPositionFailure f;
    auto p = owner(key, identity);
    check(p->snapshot(f) == MovieOutputPositionSnapshot{}, "fresh zero processing and absent publication");
    auto a = p->complete_work(key, identity, {1, 77, current}, {}, f);
    check(a && !a->publication && a->successful_work == 1, "explicit no-output still advances successful work");
    a = p->complete_work(key, identity, {2, 77, current}, selected(10), f);
    check(a && a->publication && a->publication->value == 0 && a->successful_work == 2,
        "first accepted output latches current processing and publishes0 before advance");
    auto held = *a->publication;
    check(held.work.current_header == current && held.selected.picture_es_byte == 10 &&
        held.work.current_header->picture_es_byte == 800, "current work header and selected pixels are distinct roles");
    a = p->complete_work(key, identity, {3, 77, current}, {}, f);
    check(a && !a->publication && p->snapshot(f)->published_position == 0, "no-output preserves last publication");
    a = p->complete_work(key, identity, {4, 77, current}, selected(30), f);
    check(a && a->publication->value == 2 && a->successful_work == 4, "no-output gap is not compressed into output ordinal");
    a = p->flush(key, identity, {5, 77, current}, selected(20), f);
    check(a && a->publication->value == 3 && a->publication->final_flush && a->successful_work == 4 && a->flushed,
        "explicit final selected flush publishes current position without processing advance");
    const auto terminal = p->snapshot(f);
    check(!p->complete_work(key, identity, {6, 77, current}, selected(40), f) &&
        f.kind == MovieOutputPositionError::closed && p->snapshot(f) == terminal, "final flush closes work ingress");
    p.reset(); current.reset();
    check(held.value == 0 && held.work.current_header->source_key == key && held.identity == identity,
        "owned publication and current header survive producer teardown");
    {
        auto q = owner(key, identity); auto h = header(key, identity);
        for (std::uint64_t i = 1; i <= 4; ++i) {
            auto result = q->complete_work(key, identity, {i, i, h}, selected(i * 10), f);
            check(result && result->publication->value == i - 1, "explicit successful-output sequence dense when inputs actually are");
        }
        auto end = q->flush(key, identity, {5, 4, h}, {}, f);
        check(end && !end->publication && end->successful_work == 4, "explicit no-output final control does not manufacture pixels");
    }
    {
        auto q = owner(key, identity); auto h = header(key, identity);
        q->complete_work(key, identity, {1, 1, h}, selected(10), f);
        const auto before = q->snapshot(f);
        auto rejects = [&](auto call, MovieOutputPositionError error) {
            check(!call() && f.kind == error, "named rejected work");
            check(q->snapshot(f) == before, "rejected operation leaves both origin/count/publication state unchanged");
        };
        auto foreign = std::make_shared<const MovieSourceObservationIdentity>();
        rejects([&] { return q->complete_work("OTHER", identity, {2, 1, h}, selected(20), f); }, MovieOutputPositionError::foreign_source);
        rejects([&] { return q->complete_work(key, foreign, {2, 1, h}, selected(20), f); }, MovieOutputPositionError::foreign_source);
        rejects([&] { return q->complete_work(key, identity, {1, 1, h}, selected(20), f); }, MovieOutputPositionError::replay);
        rejects([&] { return q->complete_work(key, identity, {2, 1, h}, selected(10), f); }, MovieOutputPositionError::replay);
        rejects([&] { return q->complete_work(key, identity, {2, 0, h}, selected(20), f); }, MovieOutputPositionError::invalid_work);
        h->coding.picture_structure = 1;
        rejects([&] { return q->complete_work(key, identity, {2, 1, h}, selected(20), f); }, MovieOutputPositionError::unsupported_profile);
        h->coding.picture_structure = 3; h->coding.repeat_first_field = true;
        rejects([&] { return q->flush(key, identity, {2, 1, h}, selected(20), f); }, MovieOutputPositionError::unsupported_profile);
        h->coding.repeat_first_field = false;
        auto bad = selected(20); bad.quarter_pts.reset();
        rejects([&] { return q->complete_work(key, identity, {2, 1, h}, bad, f); }, MovieOutputPositionError::invalid_selected);
        std::optional<MovieOutputPositionSnapshot> foreign_snapshot; MovieOutputPositionFailure thread_failure;
        std::thread thread([&] { foreign_snapshot = q->snapshot(thread_failure); }); thread.join();
        check(!foreign_snapshot && thread_failure.kind == MovieOutputPositionError::wrong_thread, "foreign thread rejected");
        NativeMovieOutputPosition moved(std::move(*q));
        check(!q->snapshot(f) && f.kind == MovieOutputPositionError::invalid_owner, "moved owner rejected");
        moved = std::move(moved); check(moved.snapshot(f) == before, "selfmove retains state");
    }
    {
        const std::string long_key(100, 'K'); auto h = header(long_key, identity); unsigned failures = 0;
        for (int point = 0;; ++point) {
            auto q = owner(long_key, identity); const auto before = q->snapshot(f);
            allocation_after = point;
            auto result = q->complete_work(long_key, identity, {1, 1, h}, selected(10), f);
            allocation_after = -1;
            if (result) break;
            check(f.kind == MovieOutputPositionError::allocation && q->snapshot(f) == before,
                "publication string/set allocation failure is atomic"); ++failures;
            check(q->complete_work(long_key, identity, {1, 1, h}, selected(10), f).has_value(),
                "failed publication did not consume work or selected identity");
        }
        check(failures >= 2, "genuine publication and replay-set allocation sites reached");
    }
    check(!NativeMovieOutputPosition::create(key, identity, MovieOutputPositionProfile::unspecified, f) &&
        f.kind == MovieOutputPositionError::unsupported_profile, "no default profile");
    check(!NativeMovieOutputPosition::create(key, {}, profile, f) &&
        f.kind == MovieOutputPositionError::invalid_source, "source epoch mandatory");
    std::puts("PASS native explicit-work output position ordering/identity/lifetime; no automatic decoder/admission binding");
    return 0;
} catch (const std::exception& e) { std::fprintf(stderr, "FAIL output position %s\n", e.what()); return 1; } }
