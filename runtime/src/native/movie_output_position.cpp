#include "mka/movie_output_position.hpp"
#include <limits>
#include <new>
#include <set>
#include <stdexcept>
#include <type_traits>

namespace mka::video {
static_assert(std::is_nothrow_move_constructible_v<MovieOutputPositionUpdate>);
struct MovieOutputPositionState {
    std::string key;
    std::shared_ptr<const MovieSourceObservationIdentity> identity;
    const std::thread::id owner = std::this_thread::get_id();
    MovieOutputPositionSnapshot value;
    std::set<std::uint64_t> selected_pictures;
};
namespace {
bool fail(MovieOutputPositionFailure& f, MovieOutputPositionError e, const char* operation) noexcept {
    f = {e, operation}; return false;
}
bool valid_selected(const MoviePictureSource& p) noexcept {
    return p.pes.stream_id >= 0xe0 && p.pes.stream_id <= 0xef &&
        p.pes.packet_offset <= p.pes.payload_offset && p.pes.payload_offset < p.pes.packet_end &&
        p.pes.es_begin <= p.picture_es_byte && p.picture_es_byte < p.pes.es_end &&
        p.packet_es_begin <= p.picture_es_byte && p.quarter_pts == p.pes.quarter_pts();
}
}
NativeMovieOutputPosition::NativeMovieOutputPosition(std::unique_ptr<MovieOutputPositionState> p) : state_(std::move(p)) {}
NativeMovieOutputPosition::~NativeMovieOutputPosition() = default;
NativeMovieOutputPosition::NativeMovieOutputPosition(NativeMovieOutputPosition&&) noexcept = default;
NativeMovieOutputPosition& NativeMovieOutputPosition::operator=(NativeMovieOutputPosition&& other) noexcept {
    if (this != &other) state_ = std::move(other.state_); return *this;
}
std::unique_ptr<NativeMovieOutputPosition> NativeMovieOutputPosition::create(std::string key,
        std::shared_ptr<const MovieSourceObservationIdentity> identity, MovieOutputPositionProfile profile,
        MovieOutputPositionFailure& f) noexcept {
    f = {};
    if (key.empty() || key.find('\0') != key.npos || !identity) {
        fail(f, MovieOutputPositionError::invalid_source, "movie output position source"); return {};
    }
    if (profile != MovieOutputPositionProfile::successful_frame_no_repeat_no_skip) {
        fail(f, MovieOutputPositionError::unsupported_profile, "movie output position explicit successful frame profile"); return {};
    }
    try {
        auto state = std::make_unique<MovieOutputPositionState>();
        state->key = std::move(key); state->identity = std::move(identity);
        return std::unique_ptr<NativeMovieOutputPosition>(new NativeMovieOutputPosition(std::move(state)));
    } catch (const std::bad_alloc&) { fail(f, MovieOutputPositionError::allocation, "movie output position owner allocation"); }
    return {};
}
bool NativeMovieOutputPosition::access(MovieOutputPositionFailure& f) const noexcept {
    f = {};
    if (!state_) return fail(f, MovieOutputPositionError::invalid_owner, "movie output position moved owner");
    if (state_->owner != std::this_thread::get_id())
        return fail(f, MovieOutputPositionError::wrong_thread, "movie output position owner thread");
    return true;
}
std::optional<MovieOutputPositionSnapshot> NativeMovieOutputPosition::snapshot(MovieOutputPositionFailure& f) const noexcept {
    if (!access(f)) return {}; return state_->value;
}
std::optional<MovieOutputPositionUpdate> NativeMovieOutputPosition::apply(std::string_view key,
        const std::shared_ptr<const MovieSourceObservationIdentity>& identity,
        const MovieOutputPositionWork& work, const std::optional<MoviePictureSource>& selected,
        bool flushing, MovieOutputPositionFailure& f) noexcept {
    if (!access(f)) return {};
    auto& s = *state_;
    if (key != s.key || !identity || identity != s.identity) {
        fail(f, MovieOutputPositionError::foreign_source, "movie output position foreign source epoch"); return {};
    }
    if (s.value.flushed) { fail(f, MovieOutputPositionError::closed, "movie output position after final flush"); return {}; }
    if (!work.work_id || work.work_id <= s.value.last_work_id) {
        fail(f, MovieOutputPositionError::replay, "movie output position work identity replay/order"); return {};
    }
    if (!work.admission_id || !work.current_header || work.current_header->source_key != key ||
            work.current_header->identity != identity) {
        fail(f, MovieOutputPositionError::invalid_work, "movie output position current admission header"); return {};
    }
    const auto& header = *work.current_header;
    if (header.coding.picture_structure != 3 || header.coding.repeat_first_field ||
            header.picture.coding_type < 1 || header.picture.coding_type > 3) {
        fail(f, MovieOutputPositionError::unsupported_profile, "movie output position field/repeat/unknown coding profile"); return {};
    }
    if (selected && (!valid_selected(*selected) || selected->pes.stream_id != header.pes.stream_id)) {
        fail(f, MovieOutputPositionError::invalid_selected, "movie output position selected source provenance"); return {};
    }
    if (selected && s.selected_pictures.contains(selected->picture_es_byte)) {
        fail(f, MovieOutputPositionError::replay, "movie output position selected picture already published"); return {};
    }
    if ((!flushing && s.value.successful_work == std::numeric_limits<std::uint64_t>::max()) ||
            (selected && s.value.published_pictures == std::numeric_limits<std::size_t>::max())) {
        fail(f, MovieOutputPositionError::overflow, "movie output position native count overflow"); return {};
    }
    try {
        auto next = s.value;
        MovieOutputPositionUpdate update{{}, next.successful_work, flushing};
        if (selected) {
            if (!next.first_output_work) next.first_output_work = next.successful_work;
            // The origin was latched at a previous/current successful work.
            if (*next.first_output_work > next.successful_work) {
                fail(f, MovieOutputPositionError::overflow, "movie output position invalid origin ordering"); return {};
            }
            const auto position = next.successful_work - *next.first_output_work;
            update.publication = MovieOutputPositionPublication{s.key, s.identity, work, *selected, position, flushing};
            next.published_position = position;
            ++next.published_pictures;
        }
        if (!flushing) ++next.successful_work;
        next.last_work_id = work.work_id; next.flushed = flushing;
        update.successful_work = next.successful_work;
        // All allocations before scalar commit. std::set insertion has strong
        // failure guarantee; return publication moves owned values without allocating.
        if (selected) s.selected_pictures.insert(selected->picture_es_byte);
        s.value = next;
        return std::optional<MovieOutputPositionUpdate>{std::move(update)};
    } catch (const std::bad_alloc&) { fail(f, MovieOutputPositionError::allocation, "movie output position publication allocation"); }
      catch (const std::length_error&) { fail(f, MovieOutputPositionError::overflow, "movie output position host extent"); }
    return {};
}
std::optional<MovieOutputPositionUpdate> NativeMovieOutputPosition::complete_work(std::string_view key,
        const std::shared_ptr<const MovieSourceObservationIdentity>& identity, const MovieOutputPositionWork& work,
        const std::optional<MoviePictureSource>& selected, MovieOutputPositionFailure& f) noexcept {
    return apply(key, identity, work, selected, false, f);
}
std::optional<MovieOutputPositionUpdate> NativeMovieOutputPosition::flush(std::string_view key,
        const std::shared_ptr<const MovieSourceObservationIdentity>& identity, const MovieOutputPositionWork& work,
        const std::optional<MoviePictureSource>& selected, MovieOutputPositionFailure& f) noexcept {
    return apply(key, identity, work, selected, true, f);
}
} // namespace mka::video
