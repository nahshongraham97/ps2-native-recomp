#include "mka/movie_subtitles.hpp"
#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
#include <string_view>

namespace mka::video {
struct MovieSubtitleStorage {
    struct Event {
        std::size_t identity;
        std::int32_t start, duration;
        std::optional<std::size_t> content_tag;
    };
    struct Channel {
        std::int32_t number, unit;
        std::vector<Event> events;
    };
    std::vector<std::uint8_t> bytes;
    std::vector<MovieSubtitleTag> tags;
    std::vector<Channel> channels;
};
namespace {
using Storage = MovieSubtitleStorage;
bool fail(MovieSubtitleFailure& f, MovieSubtitleError e, std::size_t at) {
    f = {e, at}; return false;
}
// Original strncmp(tag, literal, 7): stop at a matching NUL; byte8 is opaque.
bool tag_is(const MovieSubtitleTag& t, std::string_view name) noexcept {
    for (std::size_t i = 0; i < 7; ++i) {
        const auto expected = i < name.size() ? static_cast<unsigned char>(name[i]) : 0;
        if (t.raw_tag[i] != expected) return false;
        if (!expected) return true;
    }
    return true;
}
bool header(Storage& s, std::size_t at, std::size_t end,
            std::optional<std::size_t> parent, std::size_t& index, MovieSubtitleFailure& f) {
    if (at > end || end - at < 16) return fail(f, MovieSubtitleError::truncated_header, at);
    std::size_t length = 0;
    for (std::size_t i = 0; i < 7; ++i) {
        const auto c = s.bytes[at + 8 + i];
        unsigned digit;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else return fail(f, MovieSubtitleError::invalid_hex_length, at + 8 + i);
        length = length * 16 + digit;
    }
    const auto body = at + 16;
    if (length > end - body) return fail(f, MovieSubtitleError::body_out_of_bounds, at);
    MovieSubtitleTag t{};
    std::copy_n(s.bytes.begin() + at, 8, t.raw_tag.begin());
    t.header_offset = at; t.body_offset = body; t.body_size = length; t.parent_tag = parent;
    index = s.tags.size(); s.tags.push_back(t); return true;
}
bool children(Storage& s, std::size_t parent, std::vector<std::size_t>& out,
              MovieSubtitleFailure& f) {
    // Copy before adding tags: vector growth must not invalidate a parent ref.
    const auto p = s.tags[parent];
    const auto end = p.body_offset + p.body_size;
    for (auto at = p.body_offset; at < end;) {
        std::size_t index;
        if (!header(s, at, end, parent, index, f)) return false;
        out.push_back(index);
        at = s.tags[index].body_offset + s.tags[index].body_size;
    }
    return true;
}
std::optional<std::size_t> first(const Storage& s, const std::vector<std::size_t>& children,
                               std::string_view name) {
    for (auto i : children) {
        if (tag_is(s.tags[i], "USRINFE")) break;
        if (tag_is(s.tags[i], name)) return i;
    }
    return {};
}
bool ascii_space(unsigned char c) { return c == ' ' || (c >= 9 && c <= 13); }
bool number(const Storage& s, const std::vector<std::size_t>& children,
            std::string_view name, std::size_t parent, std::int32_t& out,
            MovieSubtitleFailure& f) {
    auto found = first(s, children, name);
    if (!found) return fail(f, MovieSubtitleError::missing_number, s.tags[parent].body_offset);
    const auto t = s.tags[*found];
    const auto bytes = std::span(s.bytes).subspan(t.body_offset, t.body_size);
    // Bounded decimal scalar: retain %d sign/ASCII whitespace, reject trailing
    // junk and absent terminator instead of reading beyond the serialized field.
    auto nul = std::find(bytes.begin(), bytes.end(), 0);
    if (nul == bytes.end()) return fail(f, MovieSubtitleError::malformed_number, t.body_offset);
    std::size_t p = 0, end = static_cast<std::size_t>(nul - bytes.begin());
    while (p < end && ascii_space(bytes[p])) ++p;
    bool negative = false;
    if (p < end && (bytes[p] == '+' || bytes[p] == '-')) negative = bytes[p++] == '-';
    const auto begin_digits = p;
    std::uint64_t value = 0;
    const auto limit = std::uint64_t(std::numeric_limits<std::int32_t>::max()) + (negative ? 1 : 0);
    while (p < end && bytes[p] >= '0' && bytes[p] <= '9') {
        const auto digit = unsigned(bytes[p++] - '0');
        if (value > (limit - digit) / 10)
            return fail(f, MovieSubtitleError::number_out_of_range, t.body_offset);
        value = value * 10 + digit;
    }
    if (p == begin_digits) return fail(f, MovieSubtitleError::malformed_number, t.body_offset);
    while (p < end && ascii_space(bytes[p])) ++p;
    if (p != end) return fail(f, MovieSubtitleError::malformed_number, t.body_offset);
    out = static_cast<std::int32_t>(negative ? -static_cast<std::int64_t>(value)
                                           : static_cast<std::int64_t>(value));
    return true;
}
bool parse_group(Storage& s, std::size_t group, const std::vector<std::size_t>& group_children,
                 MovieSubtitleFailure& f) {
    (void)group;
    for (auto ch : group_children) {
        if (tag_is(s.tags[ch], "USRINFE")) break;
        if (!tag_is(s.tags[ch], "CHDAT")) continue;
        std::vector<std::size_t> fields;
        if (!children(s, ch, fields, f)) return false;
        Storage::Channel channel{};
        if (!number(s, fields, "CHNO", ch, channel.number, f) ||
            !number(s, fields, "TUNIT", ch, channel.unit, f)) return false;
        if (channel.unit <= 0) return fail(f, MovieSubtitleError::invalid_time_unit, s.tags[ch].body_offset);
        for (auto ev : fields) {
            if (tag_is(s.tags[ev], "USRINFE")) break;
            if (!tag_is(s.tags[ev], "TIMEDAT")) continue;
            std::vector<std::size_t> event_fields;
            if (!children(s, ev, event_fields, f)) return false;
            Storage::Event event{};
            event.identity = s.tags[ev].body_offset;
            if (!number(s, event_fields, "INTIME", ev, event.start, f) ||
                !number(s, event_fields, "DURTIME", ev, event.duration, f)) return false;
            event.content_tag = first(s, event_fields, "CONTENT");
            channel.events.push_back(event);
        }
        s.channels.push_back(std::move(channel));
    }
    return true;
}
bool parse_storage(Storage& s, MovieSubtitleFailure& f) {
    std::size_t root;
    if (!header(s, 0, s.bytes.size(), {}, root, f)) return false;
    if (!tag_is(s.tags[root], "CRITAGS")) return fail(f, MovieSubtitleError::wrong_root, 0);
    if (s.tags[root].body_size != s.bytes.size() - 16)
        return fail(f, MovieSubtitleError::trailing_root_bytes, 16 + s.tags[root].body_size);
    std::vector<std::size_t> root_fields;
    if (!children(s, root, root_fields, f)) return false;
    for (auto group : root_fields) {
        if (tag_is(s.tags[group], "USRINFE")) break;
        if (!tag_is(s.tags[group], "USRINFS")) continue;
        std::vector<std::size_t> fields;
        if (!children(s, group, fields, f)) return false;
        auto type = first(s, fields, "TYPE");
        if (!type) return fail(f, MovieSubtitleError::missing_type, s.tags[group].body_offset);
        const auto t = s.tags[*type];
        auto begin = s.bytes.begin() + t.body_offset, end = begin + t.body_size;
        auto nul = std::find(begin, end, 0);
        if (nul == end) return fail(f, MovieSubtitleError::unterminated_type, t.body_offset);
        constexpr std::string_view name = "SUBTITLE";
        if (std::size_t(nul - begin) == name.size() && std::equal(begin, nul, name.begin()))
            return parse_group(s, group, fields, f); // first matching type only
    }
    return true;
}
// Native boundaries extend the old SDK's representable normal domain. Only
// actual signed64 overflow rejects; no legacy wrap or register-width cap.
bool add(std::int64_t a, std::int64_t b, std::int64_t& out) {
    constexpr auto min = std::numeric_limits<std::int64_t>::min();
    constexpr auto max = std::numeric_limits<std::int64_t>::max();
    if ((b > 0 && a > max - b) || (b < 0 && a < min - b)) return false;
    out = a + b; return true;
}
struct Product { std::uint64_t hi, lo; };
Product multiply(std::uint64_t a, std::uint64_t b) {
    // Base-2^32 schoolbook product. Each partial addition fits uint64; retaining
    // both limbs makes comparisons exact even when the product exceeds uint64.
    constexpr std::uint64_t mask = 0xffffffff;
    auto t = (a & mask) * (b & mask);
    const auto low_word = t & mask;
    const auto carry = t >> 32;
    t = (a >> 32) * (b & mask) + carry;
    const auto middle_word = t & mask;
    const auto high_word = t >> 32;
    t = (a & mask) * (b >> 32) + middle_word;
    return {(a >> 32) * (b >> 32) + high_word + (t >> 32), (t << 32) | low_word};
}
bool reached(std::int64_t boundary, std::int32_t unit, MovieSubtitleClock clock) {
    // Original value clamp and cross-product ordering; native positive64 units
    // need a full128 product, not signed64 overflow or floating point division.
    const auto left = multiply(std::uint64_t(std::max<std::int64_t>(clock.value, 0)), std::uint64_t(unit));
    const auto right = multiply(std::uint64_t(std::max<std::int64_t>(boundary, 0)), std::uint64_t(clock.unit));
    return left.hi > right.hi || (left.hi == right.hi && left.lo >= right.lo);
}
} // namespace

const char* movie_subtitle_error_name(MovieSubtitleError e) noexcept {
    switch (e) {
#define MKA_SUBTITLE_ERROR(x) case MovieSubtitleError::x: return #x;
    MKA_SUBTITLE_ERROR(none) MKA_SUBTITLE_ERROR(moved_document) MKA_SUBTITLE_ERROR(foreign_cursor)
    MKA_SUBTITLE_ERROR(truncated_header) MKA_SUBTITLE_ERROR(invalid_hex_length)
    MKA_SUBTITLE_ERROR(body_out_of_bounds) MKA_SUBTITLE_ERROR(wrong_root)
    MKA_SUBTITLE_ERROR(trailing_root_bytes) MKA_SUBTITLE_ERROR(missing_type)
    MKA_SUBTITLE_ERROR(unterminated_type) MKA_SUBTITLE_ERROR(missing_number)
    MKA_SUBTITLE_ERROR(malformed_number) MKA_SUBTITLE_ERROR(number_out_of_range)
    MKA_SUBTITLE_ERROR(invalid_time_unit) MKA_SUBTITLE_ERROR(boundary_overflow)
    MKA_SUBTITLE_ERROR(allocation_failed) MKA_SUBTITLE_ERROR(host_capacity)
    MKA_SUBTITLE_ERROR(inactive_content) MKA_SUBTITLE_ERROR(unsupported_language)
    MKA_SUBTITLE_ERROR(unterminated_content) MKA_SUBTITLE_ERROR(missing_language)
    MKA_SUBTITLE_ERROR(unterminated_language)
#undef MKA_SUBTITLE_ERROR
    }
    return "unknown_movie_subtitle_error";
}
std::optional<NativeMovieSubtitles> NativeMovieSubtitles::parse(std::vector<std::uint8_t> bytes,
                                                              MovieSubtitleFailure& f) {
    f = {};
    try {
        auto s = std::make_shared<Storage>(); s->bytes = std::move(bytes);
        if (!parse_storage(*s, f)) return {};
        return NativeMovieSubtitles(std::move(s));
    } catch (const std::bad_alloc&) { f = {MovieSubtitleError::allocation_failed, 0}; }
      catch (const std::length_error&) { f = {MovieSubtitleError::host_capacity, 0}; }
    return {};
}
std::span<const std::uint8_t> NativeMovieSubtitles::raw_bytes() const noexcept {
    return storage_ ? std::span<const std::uint8_t>(storage_->bytes) : std::span<const std::uint8_t>{};
}
std::span<const MovieSubtitleTag> NativeMovieSubtitles::tags() const noexcept {
    return storage_ ? std::span<const MovieSubtitleTag>(storage_->tags) : std::span<const MovieSubtitleTag>{};
}
std::span<const std::uint8_t> MovieSubtitleQuery::content() const noexcept {
    return owner_ && active ? std::span<const std::uint8_t>(owner_->bytes).subspan(content_offset_, content_size_)
                            : std::span<const std::uint8_t>{};
}
std::optional<std::vector<std::uint8_t>> extract_movie_subtitle_language(
    const MovieSubtitleQuery& query, MovieSubtitleLanguage language, MovieSubtitleFailure& f) {
    f = {};
    std::string_view marker;
    switch (language) {
    case MovieSubtitleLanguage::english: marker = "<ENG>"; break;
    case MovieSubtitleLanguage::french: marker = "<FRE>"; break;
    case MovieSubtitleLanguage::german: marker = "<GER>"; break;
    case MovieSubtitleLanguage::spanish: marker = "<SPA>"; break;
    case MovieSubtitleLanguage::italian: marker = "<ITA>"; break;
    default: f = {MovieSubtitleError::unsupported_language, 0}; return {};
    }
    if (!query.active) { f = {MovieSubtitleError::inactive_content, 0}; return {}; }
    const auto content = query.content();
    const auto nul = std::find(content.begin(), content.end(), 0);
    if (nul == content.end()) { f = {MovieSubtitleError::unterminated_content, 0}; return {}; }
    const auto match = std::search(content.begin(), nul, marker.begin(), marker.end());
    if (match == nul) { f = {MovieSubtitleError::missing_language, 0}; return {}; }
    const auto begin = match + marker.size();
    const auto end = std::find(begin, nul, '<');
    if (end == nul) { f = {MovieSubtitleError::unterminated_language, std::size_t(begin - content.begin())}; return {}; }
    try { return std::vector<std::uint8_t>(begin, end); }
    catch (const std::bad_alloc&) { f = {MovieSubtitleError::allocation_failed, 0}; }
    catch (const std::length_error&) { f = {MovieSubtitleError::host_capacity, 0}; }
    return {};
}
std::optional<MovieSubtitleCursor> NativeMovieSubtitles::cursor(std::int32_t channel, std::int64_t offset,
                                                               MovieSubtitleFailure& f) const {
    f = {};
    if (!storage_) { f = {MovieSubtitleError::moved_document, 0}; return {}; }
    return MovieSubtitleCursor(storage_, channel, offset);
}
std::optional<MovieSubtitleQuery> NativeMovieSubtitles::query(MovieSubtitleCursor& cursor,
                                                             MovieSubtitleClock clock,
                                                             MovieSubtitleFailure& f) const {
    f = {};
    if (!storage_) { f = {MovieSubtitleError::moved_document, 0}; return {}; }
    if (cursor.owner_ != storage_) { f = {MovieSubtitleError::foreign_cursor, 0}; return {}; }
    if (clock.unit <= 0) { f = {MovieSubtitleError::invalid_time_unit, 0}; return {}; }
    MovieSubtitleQuery result;
    result.owner_ = storage_;
    const Storage::Channel* channel = nullptr;
    for (const auto& ch : storage_->channels) if (ch.number == cursor.channel) { channel = &ch; break; }
    if (!channel) return result;
    const Storage::Event* selected = nullptr;
    for (const auto& ev : channel->events) {
        std::int64_t start;
        if (!add(ev.start, cursor.offset, start)) { f = {MovieSubtitleError::boundary_overflow, ev.identity}; return {}; }
        if (!reached(start, channel->unit, clock)) break; // exact sequential stop, not sorted search
        selected = &ev;
    }
    if (!selected) return result;
    std::int64_t end, shifted_end;
    if (!add(selected->start, selected->duration, end) || !add(end, cursor.offset, shifted_end)) {
        f = {MovieSubtitleError::boundary_overflow, selected->identity}; return {};
    }
    if (reached(shifted_end, channel->unit, clock) || !selected->content_tag) return result;
    const auto& content = storage_->tags[*selected->content_tag];
    result.active = true; result.changed = cursor.last_ != selected->identity;
    result.event_identity = selected->identity;
    result.time_unit = channel->unit; result.intime = selected->start; result.duration = selected->duration;
    result.content_offset_ = content.body_offset; result.content_size_ = content.body_size;
    cursor.last_ = selected->identity; // publish cache only after successful active result
    return result;
}
} // namespace mka::video
