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
struct Bits {std::vector<std::uint8_t> bytes;unsigned bit=0;explicit Bits(unsigned code):bytes{0,0,1,std::uint8_t(code)}{}void put(unsigned n,std::uint32_t value){for(unsigned i=0;i<n;++i){if(!bit)bytes.push_back(0);bytes.back()|=((value>>(n-1-i))&1u)<<(7-bit);bit=(bit+1)%8;}}};
std::shared_ptr<MovieCurrentHeaderUnit> parsed_unit(Bits b,std::uint64_t at){
    auto result=parse_movie_header_unit(b.bytes);check(bool(result.unit),"literal current source unit");
    auto unit=std::make_shared<MovieCurrentHeaderUnit>();unit->origin.prefix={at,0x100u|b.bytes[3],movie_prefix_classification(0x100u|b.bytes[3])};
    unit->origin.pes=selected(800).pes;unit->origin.quarter_pts=unit->origin.pes.quarter_pts();
    unit->consumed_end=at+result.unit->consumed_bytes;unit->unit=std::move(*result.unit);return unit;
}
void set_coding(const std::shared_ptr<MovieAdmissionCurrent>& p,unsigned structure,bool repeat){
    Bits b(0xb5);b.put(4,8);for(unsigned i=0;i<4;++i)b.put(4,1);b.put(2,0);b.put(2,structure);
    for(unsigned i=0;i<9;++i)b.put(1,i==8||(i==6&&repeat));b.put(1,0);p->coding=parsed_unit(std::move(b),808);
}
std::shared_ptr<MovieAdmissionCurrent> header(const std::string& key,
        const std::shared_ptr<const MovieSourceObservationIdentity>& identity) {
    auto p=std::make_shared<MovieAdmissionCurrent>();
    p->source=std::make_shared<MovieAdmissionEpoch>(MovieAdmissionEpoch{key,identity});
    Bits b(0);b.put(10,0);b.put(3,1);b.put(16,0xffff);b.put(1,0);p->picture=parsed_unit(std::move(b),800);
    set_coding(p,3,false);return p;
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
        held.work.current_header->picture->origin.prefix.es_prefix_first_byte == 800, "current work header and selected pixels are distinct roles");
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
    check(held.value == 0 && held.work.current_header->source->source_key == key && held.identity == identity,
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
        set_coding(h,1,false);
        rejects([&] { return q->complete_work(key, identity, {2, 1, h}, selected(20), f); }, MovieOutputPositionError::unsupported_profile);
        set_coding(h,3,true);
        rejects([&] { return q->flush(key, identity, {2, 1, h}, selected(20), f); }, MovieOutputPositionError::unsupported_profile);
        set_coding(h,3,false);
        auto actual=h->coding;auto stale=std::make_shared<MovieCurrentHeaderUnit>(*actual);stale->origin.prefix.es_prefix_first_byte=799;h->coding=stale;
        rejects([&] { return q->complete_work(key, identity, {2, 1, h}, selected(20), f); }, MovieOutputPositionError::invalid_work);
        auto forged=std::make_shared<MovieCurrentHeaderUnit>(*actual);std::get<MoviePictureCodingExtension>(forged->unit.payload).repeat_first_field=true;h->coding=forged;
        rejects([&] { return q->complete_work(key, identity, {2, 1, h}, selected(20), f); }, MovieOutputPositionError::invalid_work);
        h->coding=actual;
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
        check(failures >= 4, "genuine publication and replay-set allocation sites reached");
    }
    check(!NativeMovieOutputPosition::create(key, identity, MovieOutputPositionProfile::unspecified, f) &&
        f.kind == MovieOutputPositionError::unsupported_profile, "no default profile");
    check(!NativeMovieOutputPosition::create(key, {}, profile, f) &&
        f.kind == MovieOutputPositionError::invalid_source, "source epoch mandatory");
    std::puts("PASS native explicit-work output position ordering/identity/lifetime; no automatic decoder/admission binding");
    return 0;
} catch (const std::exception& e) { std::fprintf(stderr, "FAIL output position %s\n", e.what()); return 1; } }
