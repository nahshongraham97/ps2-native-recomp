#include "mka/movie_subtitles.hpp"
#include "mka/movie_container.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <intrin.h>
#pragma comment(lib, "bcrypt.lib")
#endif
using namespace mka::video;
namespace {
using Bytes = std::vector<std::uint8_t>;
void check(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "[subtitle-fail] %s\n", message); std::exit(1); }
}
Bytes text(std::string_view s) { Bytes out(s.begin(), s.end()); out.push_back(0); return out; }
void append(Bytes& out, const Bytes& b) { out.insert(out.end(), b.begin(), b.end()); }
Bytes tag(std::string_view name, const Bytes& body) {
    check(name.size() <= 8 && body.size() <= 0xfffffff, "synthetic serialized field extent");
    Bytes out(16, 0); std::copy(name.begin(), name.end(), out.begin());
    char length[8]; std::snprintf(length, sizeof(length), "%07X", unsigned(body.size()));
    std::copy_n(length, 7, out.begin() + 8); append(out, body); return out;
}
Bytes scalar(std::string_view name, std::string_view value) { return tag(name, text(value)); }
Bytes event(std::string_view start, std::string_view duration, const Bytes& content) {
    Bytes b = scalar("INTIME", start); append(b, scalar("DURTIME", duration));
    append(b, tag("CONTENT", content)); return tag("TIMEDAT", b);
}
Bytes channel(const Bytes& events, std::string_view number = "0", std::string_view unit = "1000") {
    Bytes b = scalar("CHNO", number); append(b, scalar("TUNIT", unit)); append(b, events);
    return tag("CHDAT", b);
}
Bytes group(const Bytes& channels, std::string_view type = "SUBTITLE") {
    Bytes b = tag("TYPE", text(type)); append(b, channels); return tag("USRINFS", b);
}
Bytes root(const Bytes& events, std::string_view number = "0", std::string_view unit = "1000") {
    return tag("CRITAGS", group(channel(events, number, unit)));
}
NativeMovieSubtitles parsed(Bytes bytes) {
    MovieSubtitleFailure f; auto d = NativeMovieSubtitles::parse(std::move(bytes), f);
    if (!d) std::fprintf(stderr, "parse error %s at %zu\n", movie_subtitle_error_name(f.kind), f.offset);
    check(bool(d) && f.kind == MovieSubtitleError::none, "valid parse"); return std::move(*d);
}
MovieSubtitleCursor cursor(const NativeMovieSubtitles& d, std::int32_t ch = 0, std::int64_t off = 0) {
    MovieSubtitleFailure f; auto c = d.cursor(ch, off, f); check(bool(c), "explicit cursor"); return std::move(*c);
}
MovieSubtitleQuery query(const NativeMovieSubtitles& d, MovieSubtitleCursor& c,
                         std::int64_t value, std::int64_t unit = 1000) {
    MovieSubtitleFailure f; auto q = d.query(c, {value, unit}, f);
    check(bool(q) && f.kind == MovieSubtitleError::none, "successful explicit-clock query"); return std::move(*q);
}
bool content_is(const MovieSubtitleQuery& q, const Bytes& b) {
    return q.content().size() == b.size() && std::equal(q.content().begin(), q.content().end(), b.begin());
}
void rejected(Bytes b, MovieSubtitleError expected) {
    MovieSubtitleFailure f; auto d = NativeMovieSubtitles::parse(std::move(b), f);
    check(!d && f.kind == expected, movie_subtitle_error_name(expected));
}
// Independent native128 oracle: compiler/hardware intrinsic, not the
// production base-2^32 multiplication transcription.
bool oracle_reached(std::int64_t boundary, std::int32_t event_unit, MovieSubtitleClock clock) {
    const auto a = std::uint64_t(std::max<std::int64_t>(clock.value, 0));
    const auto b = std::uint64_t(event_unit);
    const auto c = std::uint64_t(std::max<std::int64_t>(boundary, 0));
    const auto d = std::uint64_t(clock.unit);
#if defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__))
    unsigned __int64 ah = 0, ch = 0;
    const auto al = _umul128(a, b, &ah), cl = _umul128(c, d, &ch);
    return ah > ch || (ah == ch && al >= cl);
#elif defined(__SIZEOF_INT128__)
    return static_cast<unsigned __int128>(a) * b >= static_cast<unsigned __int128>(c) * d;
#else
    check(false, "independent128 comparison oracle unsupported on this test target"); return false;
#endif
}
void native_timeline_oracle() {
    const Bytes content = text("native timeline");
    constexpr std::int32_t event_unit = 2147483647;
    auto doc = parsed(root(event("1", "2", content), "0", "2147483647"));
    auto c = cursor(doc);
    std::uint64_t state = 0x937ab129e65c4d80ULL;
    std::size_t cases = 0;
    for (int i = 0; i < 2000; ++i) {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        c.offset = std::int64_t(state % 6000000000ULL) + 1;
        const auto factor = std::int64_t((state >> 17) % 1000000000ULL) + 1;
        const auto start = c.offset + 1, end = c.offset + 3;
        const auto tick = start * factor;
        for (const auto delta : {std::int64_t(-1), std::int64_t(0), std::int64_t(1), 2 * factor - 1, 2 * factor}) {
            MovieSubtitleClock clock{tick + delta, std::int64_t(event_unit) * factor};
            const bool expected = oracle_reached(start, event_unit, clock) && !oracle_reached(end, event_unit, clock);
            check(query(doc, c, clock.value, clock.unit).active == expected,
                  "exact native64 boundary against independent128 oracle");
            ++cases;
        }
    }
    c.offset = event_unit - 1;
    constexpr auto max = std::numeric_limits<std::int64_t>::max();
    check(query(doc, c, max, max).active, "128 product equality at native maximum clock unit/value");
    c.offset = max - 3;
    check(query(doc, c, max - 2, event_unit).active && !query(doc, c, max, event_unit).active,
          "representable native INT64 boundary endpoint exactness");
    std::printf("[subtitle-pass] native timeline %zu independent128 boundary cases\n", cases);
}
void synthetic() {
    const Bytes a{0xd3, 0xdc, '\\', 'n', 0}, b = text("second");
    auto d = parsed(root(event("7000", "1500", a))); auto c = cursor(d);
    check(!query(d, c, 6999).active && !c.last_event_identity(), "before first leaves initial cache");
    auto first = query(d, c, 7000);
    check(first.active && first.changed && content_is(first, a), "inclusive start and legacy bytes/NUL");
    const auto identity = c.last_event_identity();
    check(!query(d, c, 8499).changed, "same event unchanged");
    check(!query(d, c, 8500).active && c.last_event_identity() == identity, "exclusive end retains cache");
    check(!query(d, c, 7000).changed, "reactivation of cached identity unchanged");
    c.channel = 9;
    check(!query(d, c, 7000).active && c.last_event_identity() == identity, "unknown channel is empty, retains cache");
    c.channel = 0; c.offset = 500;
    check(!query(d, c, 7499).active && query(d, c, 7500).active, "explicit offset added in event units");
    check(query(d, c, 15, 2).active, "rational cross product without PTS conversion");
    MovieSubtitleFailure f;
    check(!d.query(c, {7500, 0}, f) && f.kind == MovieSubtitleError::invalid_time_unit &&
          c.last_event_identity() == identity, "invalid clock unit fails atomically");

    Bytes events = event("0", "100", a); append(events, event("10", "5", b));
    auto overlap = parsed(root(events)); auto oc = cursor(overlap);
    check(content_is(query(overlap, oc, 10), b), "later overlap selected");
    check(!query(overlap, oc, 15).active, "expired later overlap does not revive earlier long event");
    events = event("10", "20", a); append(events, event("10", "30", b));
    auto equal = parsed(root(events)); auto ec = cursor(equal);
    check(content_is(query(equal, ec, 10), b), "equal start chooses later file record");
    events = event("0", "100", a); append(events, event("30", "100", b));
    append(events, event("5", "100", text("must not search past future")));
    auto unsorted = parsed(root(events)); auto uc = cursor(unsorted);
    check(content_is(query(unsorted, uc, 10), a), "file traversal stops at first future record");

    auto negative = parsed(root(event("-5", "10", a))); auto nc = cursor(negative);
    check(query(negative, nc, -1).active, "negative boundary/current clamp to zero");
    check(!query(negative, nc, 5).active, "clamp preserves exact end predicate");
    auto past = parsed(root(event("-10", "5", a))); auto pc = cursor(past);
    check(!query(past, pc, -1).active, "both negative boundaries clamp, empty interval");
    auto negative_duration = parsed(root(event("10", "-1", a))); auto ndc = cursor(negative_duration);
    check(!query(negative_duration, ndc, 10).active, "signed source duration retained, not unsigned wrap");

    auto huge = parsed(root(event("2147483647", "1", a))); auto hc = cursor(huge);
    check(query(huge, hc, 2147483647).active, "serialized start plus duration can exceed legacy32 without wrap");
    hc.offset = 1;
    check(query(huge, hc, 2147483648LL).active, "native boundary exceeds INT32");
    auto minimum = parsed(root(event("-2147483648", "1", a))); auto mc = cursor(minimum);
    mc.offset = -1;
    check(!query(minimum, mc, 0).active, "native negative boundary extends below INT32 then clamps");
    auto wide = parsed(root(event("0", "100", a))); auto wc = cursor(wide, 0, 5000000000LL);
    check(query(wide, wc, 5000000000000000LL, 1000000000LL).active,
          "native64 clock ticks/unit/offset exceed SDK register width");
    const auto wide_identity = wc.last_event_identity();
    check(!query(wide, wc, 5000000100000000LL, 1000000000LL).active &&
          wc.last_event_identity() == wide_identity, "native64 exclusive end retains cache");
    wc.offset = std::numeric_limits<std::int64_t>::max();
    check(!wide.query(wc, {std::numeric_limits<std::int64_t>::max(), 1000}, f) && f.kind == MovieSubtitleError::boundary_overflow &&
          wc.last_event_identity() == wide_identity, "native64 end overflow rejects atomically");
    auto overflow_start = parsed(root(event("1", "1", a)));
    auto osc = cursor(overflow_start, 0, std::numeric_limits<std::int64_t>::max());
    check(!overflow_start.query(osc, {0, 1}, f) && f.kind == MovieSubtitleError::boundary_overflow,
          "native64 start overflow named");
    mc.offset = std::numeric_limits<std::int64_t>::min();
    check(!minimum.query(mc, {0, 1}, f) && f.kind == MovieSubtitleError::boundary_overflow,
          "native64 negative underflow named");

    // First matching scalar, channel, and type. Unused duplicate scalar payload
    // is opaque to semantic conversion; malformed first value still rejects.
    Bytes e = scalar("INTIME", "0"); append(e, scalar("INTIME", "not decimal"));
    append(e, scalar("DURTIME", "100")); append(e, tag("CONTENT", a));
    append(e, tag("CONTENT", b));
    Bytes channels = channel(tag("TIMEDAT", e)); append(channels, channel(event("0", "100", b)));
    Bytes groups = group(channel(event("0", "100", text("other"))), "OTHER");
    append(groups, group(channels)); append(groups, group(channel(event("0", "100", b))));
    auto duplicates = parsed(tag("CRITAGS", groups)); auto dc = cursor(duplicates);
    check(content_is(query(duplicates, dc, 0), a), "first matching type/channel/scalar/content");
    auto no_type = parsed(tag("CRITAGS", group(channel(event("0", "100", a)), "OTHER")));
    auto ntc = cursor(no_type); check(!query(no_type, ntc, 0).active, "no SUBTITLE type is successful empty");
    Bytes terminated = tag("USRINFE", {}); append(terminated, group(channel(event("0", "100", a))));
    auto stopped = parsed(tag("CRITAGS", terminated)); auto sc = cursor(stopped);
    check(!query(stopped, sc, 0).active, "USRINFE terminates source search");
    Bytes absent = scalar("INTIME", "0"); append(absent, scalar("DURTIME", "100"));
    auto missing_content = parsed(root(tag("TIMEDAT", absent))); auto acc = cursor(missing_content);
    check(!query(missing_content, acc, 0).active && !acc.last_event_identity(), "absent CONTENT leaves cache unchanged");
    auto empty = parsed(root(event("0", "100", {}))); auto emc = cursor(empty);
    auto emq = query(empty, emc, 0);
    check(emq.active && emq.changed && emq.content().empty(), "present zero-length CONTENT is active");

    // strncmp stops at matching NUL, and the eighth header bytes are ignored.
    Bytes padded = root(event(" +0 \t", "100", a));
    for (std::size_t i = 0; i + 16 <= padded.size(); ++i) {
        if (std::equal(padded.begin() + i, padded.begin() + i + 5, "TYPE\0")) {
            padded[i + 5] = 0xff; padded[i + 6] = 0xfe; padded[i + 7] = 'x'; break;
        }
    }
    padded[7] = 0x91; padded[15] = 0x92;
    auto pad = parsed(padded); auto padc = cursor(pad);
    check(content_is(query(pad, padc, 0), a), "first7 tag comparison and ignored eighth length byte");
    Bytes opaque = group(channel(event("0", "100", a)));
    const Bytes unknown{0xff, 0, 'x', '<', 0x82}; append(opaque, tag("SFVFTRS", unknown));
    auto od = parsed(tag("CRITAGS", opaque));
    bool retained = false;
    for (const auto& t : od.tags()) if (std::equal(t.raw_tag.begin(), t.raw_tag.begin() + 7, "SFVFTRS")) {
        auto bytes = od.raw_bytes().subspan(t.body_offset, t.body_size);
        retained = bytes.size() == unknown.size() && std::equal(bytes.begin(), bytes.end(), unknown.begin());
    }
    check(retained, "unknown SFVFTRS preserved as opaque body, no guessed recursion");

    rejected({}, MovieSubtitleError::truncated_header);
    rejected(Bytes(15, 0), MovieSubtitleError::truncated_header);
    auto bad = root(event("0", "100", a)); bad[8] = 'G';
    rejected(bad, MovieSubtitleError::invalid_hex_length);
    bad = root(event("0", "100", a)); bad.pop_back();
    rejected(bad, MovieSubtitleError::body_out_of_bounds);
    bad = root(event("0", "100", a)); bad.push_back(0);
    rejected(bad, MovieSubtitleError::trailing_root_bytes);
    rejected(tag("CRITAGS", Bytes{0}), MovieSubtitleError::truncated_header);
    rejected(tag("NOTROOT", {}), MovieSubtitleError::wrong_root);
    rejected(tag("CRITAGS", tag("USRINFS", {})), MovieSubtitleError::missing_type);
    rejected(tag("CRITAGS", tag("USRINFS", tag("TYPE", Bytes{'S'}))), MovieSubtitleError::unterminated_type);
    rejected(tag("CRITAGS", group(tag("CHDAT", scalar("CHNO", "0")))), MovieSubtitleError::missing_number);
    rejected(root(event("0x10", "100", a)), MovieSubtitleError::malformed_number);
    rejected(root(event("2147483648", "100", a)), MovieSubtitleError::number_out_of_range);
    rejected(root(event("-2147483649", "100", a)), MovieSubtitleError::number_out_of_range);
    rejected(root(event("0", "100", a), "0", "0"), MovieSubtitleError::invalid_time_unit);
    rejected(root(event("0", "100", a), "0", "-1"), MovieSubtitleError::invalid_time_unit);
    Bytes nonul = tag("INTIME", Bytes{'0'}); append(nonul, scalar("DURTIME", "100"));
    rejected(root(tag("TIMEDAT", nonul)), MovieSubtitleError::malformed_number);
    rejected(root(tag("TIMEDAT", scalar("INTIME", "0"))), MovieSubtitleError::missing_number);

    auto foreign = parsed(root(event("0", "100", a)));
    check(!foreign.query(c, {0, 1000}, f) && f.kind == MovieSubtitleError::foreign_cursor,
          "identical serialized bytes do not authorize a foreign cursor");
    auto moved = std::move(d);
    check(!d.query(c, {0, 1000}, f) && f.kind == MovieSubtitleError::moved_document, "moved document named error");
    c.offset = 0; check(query(moved, c, 7000).active, "cursor survives document move");
    auto copy = moved; check(query(copy, c, 7000).active, "shared immutable copy retains identity");
    auto moved_cursor = std::move(c);
    check(!copy.query(c, {7000, 1000}, f) && f.kind == MovieSubtitleError::foreign_cursor,
          "moved cursor cannot access its retired ownership");
    check(query(copy, moved_cursor, 7000).active, "moved cursor preserves original ownership/cache");
    std::optional<MovieSubtitleQuery> survivor;
    { auto temporary = parsed(root(event("0", "100", a))); auto tc = cursor(temporary); survivor = query(temporary, tc, 0); }
    check(survivor && content_is(*survivor, a), "result owns raw CONTENT after document/cursor teardown");

    const Bytes encoded{'<','E','N','G','>','\\','n',0xd3,0xdc,'<','>',0};
    auto language_doc = parsed(root(event("0", "100", encoded))); auto language_cursor = cursor(language_doc);
    auto language_query = query(language_doc, language_cursor, 0);
    auto body = extract_movie_subtitle_language(language_query, MovieSubtitleLanguage::english, f);
    check(body && *body == Bytes({'\\','n',0xd3,0xdc}), "owned language body preserves escape/legacy bytes");
    auto moved_language_query = std::move(language_query);
    check(language_query.content().empty() &&
          extract_movie_subtitle_language(moved_language_query, MovieSubtitleLanguage::english, f) == body,
          "query move keeps ownership only in destination");
    language_query = moved_language_query;
    check(!extract_movie_subtitle_language(language_query, MovieSubtitleLanguage::french, f) &&
          f.kind == MovieSubtitleError::missing_language, "no default language on missing marker");
    check(!extract_movie_subtitle_language(language_query, static_cast<MovieSubtitleLanguage>(-1), f) &&
          f.kind == MovieSubtitleError::unsupported_language, "unsupported locale named");
    auto inactive = query(language_doc, language_cursor, 100);
    check(!extract_movie_subtitle_language(inactive, MovieSubtitleLanguage::english, f) &&
          f.kind == MovieSubtitleError::inactive_content, "empty query not fake language success");
    auto malformed_language = [&](Bytes content, MovieSubtitleError expected) {
        auto doc = parsed(root(event("0", "100", content))); auto cur = cursor(doc);
        auto q = query(doc, cur, 0);
        check(!extract_movie_subtitle_language(q, MovieSubtitleLanguage::english, f) && f.kind == expected,
              movie_subtitle_error_name(expected));
    };
    malformed_language(Bytes{'<','E','N','G','>','x','<','>'}, MovieSubtitleError::unterminated_content);
    malformed_language(text("<ENG>no delimiter"), MovieSubtitleError::unterminated_language);
    malformed_language(Bytes{'<','E','N','G','>','x',0,'<','>'}, MovieSubtitleError::unterminated_language);
    auto empty_language = parsed(root(event("0", "100", text("<ENG><>")))); auto elc = cursor(empty_language);
    auto empty_body = extract_movie_subtitle_language(query(empty_language, elc, 0), MovieSubtitleLanguage::english, f);
    check(empty_body && empty_body->empty(), "explicit empty language body is valid");
    Bytes long_body = text(std::string("<ENG>") + std::string(4097, 'x') + "<>");
    auto long_language = parsed(root(event("0", "100", long_body))); auto llc = cursor(long_language);
    auto large_body = extract_movie_subtitle_language(query(long_language, llc, 0), MovieSubtitleLanguage::english, f);
    check(large_body && large_body->size() == 4097, "host-owned language body has no SDK scratch capacity");
    auto repeated_language = parsed(root(event("0", "100", text("<ENG>first<><ENG>second<>"))));
    auto rlc = cursor(repeated_language);
    auto first_body = extract_movie_subtitle_language(query(repeated_language, rlc, 0), MovieSubtitleLanguage::english, f);
    check(first_body && *first_body == Bytes({'f','i','r','s','t'}), "first language marker matches original strstr");

    Bytes many;
    for (int i = 0; i < 12000; ++i) append(many, event(std::to_string(i * 2), "1", a));
    auto large = parsed(root(many)); auto lc = cursor(large);
    auto lq = query(large, lc, 23998);
    check(lq.active && lq.intime == 23998 && lq.changed, "12000 host-owned events without fixed SDK pool");
    std::printf("[subtitle-pass] synthetic ownership/headers/selection/cache/malformed/12000-events\n");
}
#ifdef _WIN32
std::string sha256(std::span<const std::uint8_t> bytes) {
    BCRYPT_ALG_HANDLE algorithm = nullptr; BCRYPT_HASH_HANDLE hash = nullptr;
    check(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0, "SHA256 algorithm");
    ULONG size = 0, got = 0;
    check(BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&size), sizeof(size), &got, 0) >= 0,
          "SHA256 object extent");
    Bytes object(size), digest(32);
    check(BCryptCreateHash(algorithm, &hash, object.data(), size, nullptr, 0, 0) >= 0, "SHA256 create");
    for (std::size_t at = 0; at < bytes.size();) {
        auto n = static_cast<ULONG>(std::min<std::size_t>(bytes.size() - at, 1024 * 1024));
        check(BCryptHashData(hash, const_cast<PUCHAR>(bytes.data() + at), n, 0) >= 0, "SHA256 bytes"); at += n;
    }
    check(BCryptFinishHash(hash, digest.data(), ULONG(digest.size()), 0) >= 0, "SHA256 finish");
    check(BCryptDestroyHash(hash) >= 0 && BCryptCloseAlgorithmProvider(algorithm, 0) >= 0, "SHA256 teardown");
    std::string out; constexpr char hex[] = "0123456789abcdef";
    for (auto c : digest) { out += hex[c >> 4]; out += hex[c & 15]; } return out;
}
void real(const std::filesystem::path& path, std::string_view expected_asset, std::string_view expected_root) {
    std::ifstream in(path, std::ios::binary); check(bool(in), "explicit real asset open");
    Bytes bytes((std::istreambuf_iterator<char>(in)), {}); check(!in.bad(), "read explicit real asset");
    check(bytes.size() == 21565440 && sha256(bytes) == expected_asset, "full asset SHA256 pin");
    // This is a FIXTURE assembly of two independently recorded packet spans.
    // It is not the production PS parser/continuation implementation.
    check(std::equal(bytes.begin() + 0x180c, bytes.begin() + 0x1812, Bytes{0,0,1,0xbf,7,0xee}.begin()), "first pinned BF header");
    check(std::equal(bytes.begin() + 0x200c, bytes.begin() + 0x2012, Bytes{0,0,1,0xbf,1,0x67}.begin()), "second pinned BF header");
    Bytes tree(bytes.begin() + 0x1812, bytes.begin() + 0x2000);
    tree.insert(tree.end(), bytes.begin() + 0x2012, bytes.begin() + 0x2179);
    check(tree.size() == 2389 && sha256(tree) == expected_root, "full tree SHA256 pin");
    auto d = parsed(std::move(tree)); auto c = cursor(d);
    std::optional<std::size_t> previous;
    for (int i = 0; i < 7; ++i) {
        const auto start = 7000 + i * 3000;
        check(!query(d, c, start - 1).active, "actual pre-event inactive");
        auto q = query(d, c, start);
        check(q.active && q.changed && q.time_unit == 1000 && q.intime == start && q.duration == 1500,
              "actual seven event scalar/timing values");
        check(q.event_identity && q.event_identity != previous, "actual stable distinct event identities");
        previous = q.event_identity;
        check(q.content().size() == 163 && q.content().back() == 0 &&
              sha256(q.content()) == "c0b644081f15d11ed691d02c816239ca42c5ee1a620fca3998d2771ecfe376c5",
              "actual full CONTENT hash including NUL/legacy bytes");
        for (std::string_view key : {"<SPA>", "<ITA>", "<GER>", "<FRE>", "<ENG>"})
            check(std::search(q.content().begin(), q.content().end(), key.begin(), key.end()) != q.content().end(),
                  "actual raw language marker retained, no assumed text decoding");
        struct LanguageGolden { MovieSubtitleLanguage language; std::size_t length; const char* sha; };
        const LanguageGolden language_goldens[] = {
            {MovieSubtitleLanguage::spanish, 25, "64ce174fc26ac29edc42bd86ff8ba39409f9a8226da359f93b31da42b2738e14"},
            {MovieSubtitleLanguage::italian, 26, "0d89091a11ad74a412f6f4c25ce7b735e6e5509e4f27abe5a7e4b9c5153db3a8"},
            {MovieSubtitleLanguage::german, 23, "badd27d0511a604b642b2c83104228e99dbfffca1751ff3e24eb5b08cf35cc76"},
            {MovieSubtitleLanguage::french, 31, "0e97ab8a3f8ef455dc69ff0789bd892a6a3b3f131f1874dc2ec4247071678247"},
            {MovieSubtitleLanguage::english, 22, "c06c6ba74476448119b8b1c25bbb776d7be2a9b9806819a8fb8bd20837ff7bd2"}
        };
        MovieSubtitleFailure lf;
        for (const auto& golden : language_goldens) {
            auto body = extract_movie_subtitle_language(q, golden.language, lf);
            check(body && body->size() == golden.length && sha256(*body) == golden.sha,
                  "actual five language body independent SHA256 pins");
        }
        check(std::count_if(q.content().begin(), q.content().end(), [](auto v){ return v >= 128; }) == 2,
              "actual legacy non-UTF8 bytes retained");
        check(!query(d, c, start + 1499).changed && !query(d, c, start + 1500).active &&
              c.last_event_identity() == previous, "actual exclusive end and retained cursor cache");
    }
    std::printf("[subtitle-pass] explicit real asset/tree SHA256, seven events and raw language bytes\n");
}
void corpus(const std::filesystem::path& root_path) {
    // Exact relative associations from the independently pinned38-file native
    // container corpus. No directory search, basename or numeric-ID fallback.
    constexpr const char* keys[] = {
        "ATTRACTN.SFD", "ATTRACTW.SFD", "KRYPT/V_BLOOP2.SFD", "KRYPT/V_BLOOP3.SFD",
        "KRYPT/V_BLOOP4.SFD", "KRYPT/V_BLOOP5.SFD", "KRYPT/V_BLOOPR.SFD", "KRYPT/V_BLUES.SFD",
        "KRYPT/V_ERMACB.SFD", "KRYPT/V_KANO.SFD", "KRYPT/V_KONQIN.SFD", "KRYPT/V_LOAD1.SFD",
        "KRYPT/V_LOAD2.SFD", "KRYPT/V_MKAKON.SFD", "KRYPT/V_MKDACC.SFD", "KRYPT/V_MKHAND.SFD",
        "KRYPT/V_MOTOGI.SFD", "KRYPT/V_MOTORK.SFD", "KRYPT/V_MOTRBK.SFD", "KRYPT/V_MOTRFT.SFD",
        "KRYPT/V_RAIDON.SFD", "KRYPT/V_SCORP.SFD", "KRYPT/V_SHINOK.SFD", "KRYPT/V_SNDOUT.SFD",
        "KRYPT/V_SUBZER.SFD", "KRYPT/V_SZCCKY.SFD", "LOGON.SFD", "LOGOW.SFD",
        "OPENINGN.SFD", "OPENINGW.SFD", "QUADN.SFD", "QUADW.SFD",
        "SPYHUNN.SFD", "SPYHUNW.SFD", "STRANGN.SFD", "STRANGW.SFD", "TITLEN.SFD", "TITLEW.SFD"
    };
    std::vector<MovieAssetCatalogEntry> catalog;
    for (auto key : keys) catalog.push_back({key, key});
    MovieAssetFailure af;
    auto store = NativeMovieAssetStore::create(root_path, catalog, af);
    if (!store) std::fprintf(stderr, "[subtitle-corpus-fail] stage=catalog error=asset_catalog_failure kind=%d host=%d\n", int(af.kind), af.host_error.value());
    check(bool(store), "corpus exact38 relative catalog");
    struct Retained { std::string key; MovieSubtitleQuery query; std::string content_sha; };
    std::vector<Retained> retained;
    std::size_t payload_count = 0, root_count = 0, events_total = 0, active_total = 0;
    for (auto key : keys) {
        auto asset = store->open(key, af);
        if (!asset) std::fprintf(stderr, "[subtitle-corpus-fail] key=%s stage=open error=asset_open_failure kind=%d host=%d\n", key, int(af.kind), af.host_error.value());
        check(bool(asset), "corpus actual verified opened handle");
        check(asset->seek(17, mka::content::SeekOrigin::begin, af), "corpus explicit nonzero same-asset cursor");
        MovieContainerFailure cf;
        auto container = extract_movie_private2(*asset, cf);
        if (!container) std::fprintf(stderr, "[subtitle-corpus-fail] key=%s stage=container error=private2_extraction_failure kind=%d offset=%llu asset_error=%d\n",
            key, int(cf.kind), static_cast<unsigned long long>(cf.offset), int(cf.asset.kind));
        check(bool(container), "corpus native container extraction");
        check(asset->position(af) == 17, "corpus extraction restores same actual asset cursor");
        payload_count += container->private2.size();
        std::optional<std::size_t> start;
        for (std::size_t i = 0; i < container->private2.size(); ++i) {
            const auto& payload = container->private2[i].bytes;
            if (payload.size() >= 7 && std::equal(payload.begin(), payload.begin() + 7, "CRITAGS")) {
                if (start) std::fprintf(stderr, "[subtitle-corpus-fail] key=%s stage=assembly error=duplicate_root\n", key);
                check(!start, "fixture rejects duplicate CRITAGS roots"); start = i;
            }
        }
        if (!start) std::fprintf(stderr, "[subtitle-corpus-fail] key=%s stage=assembly error=missing_root\n", key);
        check(bool(start), "fixture rejects missing CRITAGS root");
        // FIXTURE-ONLY rule: append this root payload and subsequent private2
        // bodies in original order. Production framing does not claim generic
        // CRITAGS continuation semantics; exact root extents are checked below.
        Bytes tree;
        for (std::size_t i = *start; i < container->private2.size(); ++i) {
            const auto& packet = container->private2[i];
            append(tree, packet.bytes);
            std::printf("[subtitle-corpus-payload] key=%s packet=%llu payload=%llu bytes=%zu sha256=%s\n",
                key, static_cast<unsigned long long>(packet.packet_offset),
                static_cast<unsigned long long>(packet.payload_offset), packet.bytes.size(), sha256(packet.bytes).c_str());
        }
        const auto root_sha = sha256(tree); const auto root_bytes = tree.size();
        MovieSubtitleFailure sf;
        auto document = NativeMovieSubtitles::parse(std::move(tree), sf);
        if (!document) std::fprintf(stderr, "[subtitle-corpus-fail] key=%s stage=parse error=%s offset=%zu root_sha256=%s\n",
            key, movie_subtitle_error_name(sf.kind), sf.offset, root_sha.c_str());
        check(bool(document), "corpus actual owned root parse"); ++root_count;
        std::size_t event_count = 0;
        for (const auto& t : document->tags())
            if (std::equal(t.raw_tag.begin(), t.raw_tag.begin() + 7, "TIMEDAT")) ++event_count;
        events_total += event_count;
        auto c = document->cursor(0, 0, sf); check(bool(c), "corpus explicit channel0 offset0 cursor");
        // Test clock only, not a playback/PTS/wall-clock binding.
        auto selected = document->query(*c, {7000000000LL, 1000000000LL}, sf);
        if (!selected) std::fprintf(stderr, "[subtitle-corpus-fail] key=%s stage=query error=%s offset=%zu\n",
            key, movie_subtitle_error_name(sf.kind), sf.offset);
        check(bool(selected), "corpus explicit native64 synthetic-clock query");
        const bool title = std::string_view(key) == "TITLEN.SFD" || std::string_view(key) == "TITLEW.SFD";
        if ((event_count != (title ? 7u : 0u)) || selected->active != title)
            std::fprintf(stderr, "[subtitle-corpus-contradiction] key=%s events=%zu active=%d root_sha256=%s\n",
                         key, event_count, int(selected->active), root_sha.c_str());
        check(event_count == (title ? 7u : 0u) && selected->active == title,
              "observed corpus only two title roots contain seven active subtitle events");
        if (selected->active) {
            ++active_total;
            for (int i = 0; i < 7; ++i) {
                auto q = document->query(*c, {std::int64_t(7000 + i * 3000) * 1000000LL, 1000000000LL}, sf);
                check(q && q->active && q->intime == 7000 + i * 3000 && q->duration == 1500 && q->time_unit == 1000,
                      "corpus seven source event times using native64 synthetic clock");
                auto english = extract_movie_subtitle_language(*q, MovieSubtitleLanguage::english, sf);
                constexpr std::string_view expected = "\\n\\nPRESS START BUTTON";
                check(english && english->size() == expected.size() && std::equal(english->begin(), english->end(), expected.begin()),
                      "corpus exact raw English body, no encoding/layout conversion");
            }
        }
        const auto content_sha = sha256(selected->content());
        std::printf("[subtitle-corpus] key=%s source_bytes=%llu private2=%zu root_bytes=%zu root_sha256=%s events=%zu active=%d content_bytes=%zu content_sha256=%s error=none\n",
            key, static_cast<unsigned long long>(container->file_bytes), container->private2.size(), root_bytes,
            root_sha.c_str(), event_count, int(selected->active), selected->content().size(), content_sha.c_str());
        retained.push_back({key, std::move(*selected), content_sha});
        // Asset, container, document and cursor all retire at this iteration's
        // end; only the immutable query's own storage may retain CONTENT.
    }
    store.reset();
    for (const auto& item : retained) {
        check(sha256(item.query.content()) == item.content_sha, "query/content survives all asset/store/document/cursor teardown");
        if (item.query.active) {
            MovieSubtitleFailure sf;
            auto english = extract_movie_subtitle_language(item.query, MovieSubtitleLanguage::english, sf);
            constexpr std::string_view expected = "\\n\\nPRESS START BUTTON";
            check(english && english->size() == expected.size() && std::equal(english->begin(), english->end(), expected.begin()),
                  "owned language extraction after all source owners retire");
        } else check(item.query.content().empty() && !item.query.event_identity, "empty queries stay empty after source teardown");
    }
    check(root_count == 38 && payload_count == 78 && events_total == 14 && active_total == 2,
          "exact known corpus totals");
    std::printf("[subtitle-corpus-pass] assets=%zu private2=%zu roots=%zu events=%zu active_titles=%zu ownership=retained error=none; fixture-only continuation/synthetic clock; no font/playback/game claim\n",
                retained.size(), payload_count, root_count, events_total, active_total);
}
#endif
} // namespace
int main(int argc, char** argv) {
    synthetic();
    native_timeline_oracle();
    if (argc == 3 && std::string_view(argv[1]) == "--corpus") {
#ifdef _WIN32
        corpus(std::filesystem::u8path(argv[2])); return 0;
#else
        check(false, "corpus actual opened-handle/SHA256 fixture requires Windows");
#endif
    }
    if (argc == 1) { std::puts("[subtitle-scope] synthetic only; no real asset checked"); return 0; }
    check(argc == 3, "usage: test_movie_subtitles [explicit-TITLEN.SFD explicit-TITLEW.SFD | --corpus MOVIEPS2root]");
#ifdef _WIN32
    real(std::filesystem::u8path(argv[1]),
         "b184f25ad02e7ffc4ece847bc915c1f54e2eeddbf6afcd816be0108adadfa371",
         "bbe9842b4fdb0763b9393467376d54ec83fc199e52b8251566a2a60d229c4147");
    real(std::filesystem::u8path(argv[2]),
         "9a850166ddc5a8a13d153498d329c42fe479d848b56ab2784c6f371c6b3b0928",
         "3ba2d8aa47943c511560370b97eda2e5c9b8d6fae6ce7fe412b92ca79ccd5b2c");
#else
    check(false, "real SHA256 fixture requires Windows BCrypt; not exercised on this host");
#endif
    return 0;
}
