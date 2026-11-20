#include "mka/movie_admission_feed.hpp"
#include <algorithm>
#include <array>
#include <exception>
#include <limits>
#include <map>
#include <new>
#include <stdexcept>
#include <type_traits>

namespace mka::video {
struct MovieAdmissionFeedState {
    NativeMovieHeaderBoundary boundary;
    MovieAdmissionCurrent current;
    MovieAdmissionCurrent inner_current,outer_current;
    bool inner_active=false,inner_progressive=false,inner_repeat=false,inner_top=false;
    bool outer_progressive=false,outer_repeat=false,outer_top=false;
    std::map<std::uint64_t, MovieVideoPrefixObserved> prefixes;
    std::optional<std::uint64_t> last_prefix;
    std::shared_ptr<const MovieCodecInputReceipt> last_input_receipt;
    std::array<std::uint8_t, 3> consumed_tail{};
    std::size_t tail_size = 0;
    std::uint64_t sequence = 0;
    bool pending = false, program_end = false, failed = false;
    std::optional<MovieVideoPrefixObserved> batch_origin;
    std::uint64_t batch_serial=0,batch_units=0,batch_offered_end=0;
    std::optional<MovieVideoPrefixObserved> batch_timestamp_prefix;
    std::optional<std::uint64_t> batch_timestamp_offset;
    bool progressive = false, repeat = false, top = false;
    bool in_call = false;
    const std::thread::id owner = std::this_thread::get_id();
    MovieAdmissionFeedState(NativeMovieHeaderBoundary b, MovieAdmissionCurrent c)
        : boundary(std::move(b)), current(std::move(c)) { inner_current.source=current.source; }
};
namespace {
static_assert(std::is_nothrow_copy_assignable_v<MovieAdmissionCurrent>);
static_assert(std::is_nothrow_move_assignable_v<MovieAdmissionCurrent>);
struct Guard { bool& flag; explicit Guard(bool& f) : flag(f) { flag = true; } ~Guard() { flag = false; } };
bool fail(MovieAdmissionFeedFailure& f, MovieAdmissionFeedError e) noexcept { f.kind = e; return false; }
bool pes_valid(const MovieVideoPrefixObserved& p) noexcept {
    const auto at = p.prefix.es_prefix_first_byte;
    return p.pes.stream_id >= 0xe0 && p.pes.stream_id <= 0xef &&
        p.pes.packet_offset < p.pes.payload_offset && p.pes.payload_offset < p.pes.packet_end &&
        p.pes.es_begin <= at && at < p.pes.es_end && p.quarter_pts == p.pes.quarter_pts();
}
}
NativeMovieAdmissionFeed::NativeMovieAdmissionFeed(std::unique_ptr<MovieAdmissionFeedState> p) : state_(std::move(p)) {}
NativeMovieAdmissionFeed::~NativeMovieAdmissionFeed() { if (state_ && state_->in_call) std::terminate(); }
NativeMovieAdmissionFeed::NativeMovieAdmissionFeed(NativeMovieAdmissionFeed&&) noexcept = default;
NativeMovieAdmissionFeed& NativeMovieAdmissionFeed::operator=(NativeMovieAdmissionFeed&& other) noexcept {
    if (this != &other) state_ = std::move(other.state_); return *this;
}
std::unique_ptr<NativeMovieAdmissionFeed> NativeMovieAdmissionFeed::create(std::string_view key,
        std::shared_ptr<const MovieSourceObservationIdentity> identity, std::uint64_t origin,
        MovieAdmissionFeedFailure& f) noexcept {
    f = {};
    try {
        auto boundary = NativeMovieHeaderBoundary::create(key, identity, origin, f.boundary);
        if (!boundary) { fail(f, f.boundary.kind == MovieHeaderBoundaryError::allocation
            ? MovieAdmissionFeedError::allocation : MovieAdmissionFeedError::invalid_source); return {}; }
        MovieAdmissionCurrent current;
        current.source = std::make_shared<const MovieAdmissionEpoch>(MovieAdmissionEpoch{std::string(key), std::move(identity)});
        return std::unique_ptr<NativeMovieAdmissionFeed>(new NativeMovieAdmissionFeed(
            std::make_unique<MovieAdmissionFeedState>(std::move(*boundary), std::move(current))));
    } catch (const std::bad_alloc&) { fail(f, MovieAdmissionFeedError::allocation); }
      catch (const std::length_error&) { fail(f, MovieAdmissionFeedError::host_capacity); }
    return {};
}
bool NativeMovieAdmissionFeed::access(MovieAdmissionFeedFailure& f, bool allow_failed) const noexcept {
    f = {};
    if (!state_) return fail(f, MovieAdmissionFeedError::invalid_owner);
    if (state_->owner != std::this_thread::get_id()) return fail(f, MovieAdmissionFeedError::wrong_thread);
    if (state_->in_call) return fail(f, MovieAdmissionFeedError::reentry);
    if (state_->failed && !allow_failed) return fail(f, MovieAdmissionFeedError::failed_owner);
    return true;
}
bool NativeMovieAdmissionFeed::begin_inner_parse(MovieAdmissionFeedFailure& f) noexcept {
    if(!access(f))return false;Guard guard(state_->in_call);auto& s=*state_;
    if(s.inner_active||s.batch_origin)return fail(f,MovieAdmissionFeedError::scope);
    auto next=s.inner_current;
    auto merge=[](auto& into,const auto& from){if(from&&(!into||from->origin.prefix.es_prefix_first_byte>into->origin.prefix.es_prefix_first_byte))into=from;};
    merge(next.sequence,s.current.sequence);merge(next.sequence_extension,s.current.sequence_extension);
    merge(next.display_extension,s.current.display_extension);merge(next.gop,s.current.gop);
    merge(next.picture,s.current.picture);merge(next.coding,s.current.coding);
    merge(next.quant_matrix,s.current.quant_matrix);merge(next.userdata,s.current.userdata);
    auto progressive=s.inner_progressive,repeat=s.inner_repeat,top=s.inner_top;
    if(next.sequence_extension)progressive=std::get<MovieSequenceExtension>(next.sequence_extension->unit.payload).progressive_sequence;
    if(next.coding){const auto& coding=std::get<MoviePictureCodingExtension>(next.coding->unit.payload);repeat=coding.repeat_first_field;top=coding.top_field_first;}
    constexpr std::array<std::uint8_t,8> flags{0,0,1,1,0,0,2,4};next.info54=flags[4*unsigned(progressive)+2*unsigned(repeat)+unsigned(top)];next.info57=repeat;
    s.outer_current=s.current;s.outer_progressive=s.progressive;s.outer_repeat=s.repeat;s.outer_top=s.top;
    s.current=std::move(next);s.progressive=progressive;s.repeat=repeat;s.top=top;s.inner_active=true;return true;
}
bool NativeMovieAdmissionFeed::end_inner_parse(MovieAdmissionFeedFailure& f) noexcept {
    if(!access(f,true))return false;Guard guard(state_->in_call);auto& s=*state_;
    if(!s.inner_active)return fail(f,MovieAdmissionFeedError::scope);
    s.inner_current=s.current;s.inner_progressive=s.progressive;s.inner_repeat=s.repeat;s.inner_top=s.top;
    s.current=std::move(s.outer_current);s.progressive=s.outer_progressive;s.repeat=s.outer_repeat;s.top=s.outer_top;s.inner_active=false;return true;
}
bool NativeMovieAdmissionFeed::push(const MovieSourceObservationBatch& batch, MovieAdmissionFeedFailure& f) noexcept {
    if (!access(f)) return false;
    Guard guard(state_->in_call);
    auto& s = *state_;
    if (batch.source_key != s.current.source->source_key || !batch.identity || batch.identity != s.current.source->identity)
        return fail(f, MovieAdmissionFeedError::foreign_source);
    if (batch.events.empty()) return fail(f, MovieAdmissionFeedError::invalid_order);
    auto view = s.boundary.snapshot(f.boundary);
    if (!view) return fail(f, MovieAdmissionFeedError::allocation);
    try {
        auto sequence = s.sequence; auto end = view->es_end; auto last = s.last_prefix; auto ended = s.program_end;auto accepted=s.last_input_receipt;
        std::vector<std::uint8_t> extra;
        std::map<std::uint64_t, MovieVideoPrefixObserved> additions;
        const auto read = [&](std::uint64_t at) -> std::uint8_t {
            if (at < view->es_begin) return s.consumed_tail[static_cast<std::size_t>(at - (view->es_begin - s.tail_size))];
            if (at < view->es_end) return view->bytes[static_cast<std::size_t>(at - view->es_begin)];
            return extra[static_cast<std::size_t>(at - view->es_end)];
        };
        for (const auto& event : batch.events) {
            if (sequence == UINT64_MAX || event.sequence != sequence + 1) return fail(f, MovieAdmissionFeedError::invalid_order);
            ++sequence;
            if (const auto* raw = std::get_if<MovieVideoEsObserved>(&event.value)) {
                if (ended || raw->es_begin != end || raw->es_end < end || raw->es_end - end != raw->bytes.size())
                    return fail(f, MovieAdmissionFeedError::invalid_order);
                if (raw->bytes.size() > extra.max_size() - extra.size()) return fail(f, MovieAdmissionFeedError::host_capacity);
                extra.insert(extra.end(), raw->bytes.begin(), raw->bytes.end()); end = raw->es_end;
            } else if (const auto* p = std::get_if<MovieVideoPrefixObserved>(&event.value)) {
                const auto at = p->prefix.es_prefix_first_byte;
                if (ended || !pes_valid(*p) || (last && at <= *last) || at < view->es_begin - s.tail_size ||
                        at > end || end - at < 4) return fail(f, MovieAdmissionFeedError::invalid_prefix);
                std::uint32_t word = 0; for (unsigned i = 0; i < 4; ++i) word = (word << 8) | read(at + i);
                if ((word >> 8) != 1 || word != p->prefix.raw_codeword || p->prefix.classification != movie_prefix_classification(word))
                    return fail(f, MovieAdmissionFeedError::invalid_prefix);
                last = at;
                // A delayed raw prefix inside an already consumed structured
                // header is provenance only, never a new parser unit.
                if (at >= view->es_begin) additions.emplace(at, *p);
            } else if (const auto* input = std::get_if<MovieCodecInputAccepted>(&event.value)) {
                f.input_receipt=check_movie_input_receipt(input->receipt,batch.source_key,batch.identity,accepted,end,ended,view->initial_es_begin);
                if(f.input_receipt!=MovieInputReceiptError::none)return fail(f,MovieAdmissionFeedError::invalid_order);
                accepted=input->receipt;
            } else if (const auto* control = std::get_if<MovieProgramEndObservation>(&event.value)) {
                if (ended || control->byte_extent != 4 || control->file_offset > UINT64_MAX - 4 ||
                        control->trailing_padding_bytes > UINT64_MAX - control->file_offset - 4)
                    return fail(f, MovieAdmissionFeedError::invalid_order);
                ended = true;
            }
            // Decoded picture observations carry no parser-entry/admission effect.
        }
        MovieVideoEsObserved raw{view->es_end, end, std::move(extra)};
        if (!s.boundary.append(batch.source_key, batch.identity, raw, f.boundary))
            return fail(f, f.boundary.kind == MovieHeaderBoundaryError::allocation
                ? MovieAdmissionFeedError::allocation : MovieAdmissionFeedError::range);
        s.prefixes.merge(additions); s.sequence = sequence; s.last_prefix = last; s.program_end = ended;s.last_input_receipt=std::move(accepted);
        return true;
    } catch (const std::bad_alloc&) { return fail(f, MovieAdmissionFeedError::allocation); }
      catch (const std::length_error&) { return fail(f, MovieAdmissionFeedError::host_capacity); }
}
bool NativeMovieAdmissionFeed::consume(std::uint64_t count, MovieHeaderConsumeKind kind, MovieAdmissionFeedFailure& f) noexcept {
    auto& s = *state_; auto view = s.boundary.snapshot(f.boundary);
    if (!view) return fail(f, MovieAdmissionFeedError::allocation);
    if (count > view->bytes.size()) return fail(f, MovieAdmissionFeedError::range);
    if (!count) return true;
    const auto next = view->es_begin + count;
    const auto n = count >= 3 ? std::size_t(3) : std::min<std::size_t>(3, s.tail_size + static_cast<std::size_t>(count));
    std::array<std::uint8_t, 3> tail{};
    for (std::size_t i = 0; i < n; ++i) {
        const auto at = next - n + i;
        tail[i] = at >= view->es_begin ? view->bytes[static_cast<std::size_t>(at - view->es_begin)]
            : s.consumed_tail[static_cast<std::size_t>(at - (view->es_begin - s.tail_size))];
    }
    if (!s.boundary.consume(s.current.source->source_key, s.current.source->identity, kind, count, f.boundary))
        return fail(f, MovieAdmissionFeedError::range);
    s.consumed_tail = tail; s.tail_size = n;
    s.prefixes.erase(s.prefixes.begin(), s.prefixes.lower_bound(next)); s.pending = false; return true;
}
std::optional<MovieAdmissionCandidate> NativeMovieAdmissionFeed::parse_next(MovieAdmissionFeedFailure& f) noexcept {
    if (!access(f)) return {};
    Guard guard(state_->in_call);
    if(state_->batch_origin){fail(f,MovieAdmissionFeedError::scope);return {};}
    return parse_next_owned(f);
}
std::optional<MovieAdmissionCandidate> NativeMovieAdmissionFeed::parse_next_owned(MovieAdmissionFeedFailure& f) noexcept {
    auto& s = *state_; auto view = s.boundary.snapshot(f.boundary);
    if (!view) { fail(f, MovieAdmissionFeedError::allocation); return {}; }
    MovieAdmissionCandidate result; result.es_begin = view->es_begin; result.current = s.current;
    if (s.pending) { result.kind = MovieAdmissionCandidateKind::blocked; return result; }
    if (view->bytes.size() < 4) return result;
    const auto origin = s.prefixes.find(view->es_begin);
    if (origin == s.prefixes.end()) {
        if (view->bytes[0] || view->bytes[1] || view->bytes[2] != 1) result.kind = MovieAdmissionCandidateKind::other;
        return result;
    }
    result.prefix = origin->second;
    const auto code = origin->second.prefix.raw_codeword & 255;
    if (origin->second.prefix.classification & 2) {
        if (!s.current.picture || !s.current.coding || s.current.coding->origin.prefix.es_prefix_first_byte <
                s.current.picture->origin.prefix.es_prefix_first_byte) {
            fail(f, MovieAdmissionFeedError::invalid_order); s.failed = true; return {};
        }
        result.kind = MovieAdmissionCandidateKind::first_slice; s.pending = true; return result;
    }
    if (!origin->second.prefix.classification || (code >= 1 && code <= 0xaf)) {
        result.kind = MovieAdmissionCandidateKind::other; return result;
    }
    auto next = s.current;
    if ((code == 0xb8 && next.gop_parse_entries == UINT64_MAX) ||
            (code == 0xb3 && next.sequence_parse_entries == UINT64_MAX)) {
        fail(f, MovieAdmissionFeedError::overflow); return {};
    }
    if (code == 0xb8) ++next.gop_parse_entries;
    if (code == 0xb3) ++next.sequence_parse_entries;
    // Primary kind is a parser-entry effect, including source need-more/error.
    if (code == 0xb3 || code == 0xb7) next.last_primary_kind = 1;
    else if (code == 0xb8) next.last_primary_kind = 2;
    else if (code == 0) next.last_primary_kind = 3;
    auto parsed = parse_movie_header_unit(view->bytes);
    if (parsed.status == MovieHeaderUnitStatus::need_more) {
        s.current = next; result.current = next; return result;
    }
    if (parsed.status != MovieHeaderUnitStatus::parsed || !parsed.unit) {
        f.unit = parsed.failure;
        if (parsed.failure.kind == MovieHeaderUnitError::allocation) { fail(f, MovieAdmissionFeedError::allocation); return {}; }
        if (parsed.failure.kind == MovieHeaderUnitError::host_capacity) { fail(f, MovieAdmissionFeedError::host_capacity); return {}; }
        s.current = next; s.failed = true; fail(f, MovieAdmissionFeedError::parser); return {};
    }
    try {
        const auto amount = parsed.unit->consumed_bytes;
        if (amount < 4 || amount > view->bytes.size()) { fail(f, MovieAdmissionFeedError::range); return {}; }
        auto unit = std::make_shared<const MovieCurrentHeaderUnit>(MovieCurrentHeaderUnit{
            origin->second, view->es_begin + amount, std::move(*parsed.unit)});
        auto progressive = s.progressive, repeat = s.repeat, top = s.top;
        if (std::holds_alternative<MovieSequenceHeader>(unit->unit.payload)) { next.sequence = unit; next.last_primary_kind = 1; }
        else if (const auto* extension = std::get_if<MovieSequenceExtension>(&unit->unit.payload)) { next.sequence_extension = unit; progressive = extension->progressive_sequence; }
        else if (std::holds_alternative<MovieDisplayExtension>(unit->unit.payload)) next.display_extension = unit;
        else if (std::holds_alternative<MovieGopHeader>(unit->unit.payload)) {
            if (next.successful_gops == UINT64_MAX) { fail(f, MovieAdmissionFeedError::overflow); return {}; }
            next.gop = unit; ++next.successful_gops; next.last_primary_kind = 2;
        } else if (std::holds_alternative<MoviePictureHeader>(unit->unit.payload)) { next.picture = unit; next.last_primary_kind = 3; }
        else if (const auto* coding = std::get_if<MoviePictureCodingExtension>(&unit->unit.payload)) { next.coding = unit; repeat = coding->repeat_first_field; top = coding->top_field_first; }
        else if (std::holds_alternative<MovieQuantMatrixExtension>(unit->unit.payload)) next.quant_matrix = unit;
        else if (std::holds_alternative<MovieUserDataHeader>(unit->unit.payload)) next.userdata = unit;
        else if (std::holds_alternative<MovieSequenceEndHeader>(unit->unit.payload)) next.last_primary_kind = 1;
        constexpr std::array<std::uint8_t, 8> flags{0, 0, 1, 1, 0, 0, 2, 4};
        next.info54 = flags[4 * unsigned(progressive) + 2 * unsigned(repeat) + unsigned(top)]; next.info57 = repeat;
        result.kind = MovieAdmissionCandidateKind::parsed_header; result.consumed_bytes = amount;
        result.parsed = std::move(unit); result.current = next;
        if (!consume(amount, MovieHeaderConsumeKind::parser, f)) return {};
        s.current = std::move(next); s.progressive = progressive; s.repeat = repeat; s.top = top;
        return result;
    } catch (const std::bad_alloc&) { fail(f, MovieAdmissionFeedError::allocation); }
      catch (const std::length_error&) { fail(f, MovieAdmissionFeedError::host_capacity); }
    return {};
}
std::optional<MovieOuterHeaderBatchResult> NativeMovieAdmissionFeed::parse_outer_batch(MovieAdmissionFeedFailure& f) noexcept {
    if(!access(f))return {};Guard guard(state_->in_call);auto& s=*state_;
    if(s.inner_active||(s.pending&&!s.batch_origin)){fail(f,MovieAdmissionFeedError::scope);return {};}
    auto view=s.boundary.snapshot(f.boundary);if(!view){fail(f,MovieAdmissionFeedError::allocation);return {};}
    if(!s.batch_origin){
        if(view->bytes.size()<4){
            if(s.program_end&&!view->bytes.empty()){s.failed=true;f.unit.kind=MovieHeaderUnitError::incomplete_prefix;fail(f,MovieAdmissionFeedError::parser);return {};}
            return MovieOuterHeaderBatchResult{};
        }
        auto origin=s.prefixes.find(view->es_begin);
        if(origin==s.prefixes.end()){
            if(!view->bytes[0]&&!view->bytes[1]&&view->bytes[2]==1&&!s.program_end)return MovieOuterHeaderBatchResult{};
            fail(f,MovieAdmissionFeedError::invalid_prefix);return {};
        }
        if((origin->second.prefix.classification&2)||!origin->second.prefix.classification){fail(f,MovieAdmissionFeedError::invalid_order);return {};}
        if(s.batch_serial==UINT64_MAX){fail(f,MovieAdmissionFeedError::overflow);return {};}
        s.batch_origin=origin->second;s.batch_units=0;s.batch_offered_end=view->es_end;s.batch_timestamp_prefix.reset();s.batch_timestamp_offset.reset();
        // Search original raw BYTES first. Prefix metadata may arrive later;
        // its absence cannot turn a real raw picture query into timestamp absence.
        for(std::size_t i=0;i+4<=view->bytes.size();++i)
            if(!view->bytes[i]&&!view->bytes[i+1]&&view->bytes[i+2]==1&&
               (movie_prefix_classification(0x100u|view->bytes[i+3])&4)){s.batch_timestamp_offset=view->es_begin+i;break;}
    }
    if(s.batch_timestamp_offset&&!s.batch_timestamp_prefix){
        auto actual=s.prefixes.find(*s.batch_timestamp_offset);
        if(actual==s.prefixes.end()){
            if(s.program_end){s.failed=true;fail(f,MovieAdmissionFeedError::invalid_prefix);return {};}
            return MovieOuterHeaderBatchResult{};
        }
        s.batch_timestamp_prefix=actual->second;
    }
    try {
        for(;;){
            auto unit=parse_next_owned(f);if(!unit)return {};
            if(unit->kind==MovieAdmissionCandidateKind::need_more){
                if(s.program_end){s.failed=true;f.unit.kind=MovieHeaderUnitError::incomplete_header;fail(f,MovieAdmissionFeedError::parser);return {};}
                return MovieOuterHeaderBatchResult{};
            }
            auto progress=MovieOuterHeaderBatchProgress::need_more;
            if(unit->kind==MovieAdmissionCandidateKind::parsed_header){
                if(s.batch_units==UINT64_MAX){s.failed=true;fail(f,MovieAdmissionFeedError::overflow);return {};}
                ++s.batch_units;
                if(unit->parsed&&std::holds_alternative<MovieSequenceEndHeader>(unit->parsed->unit.payload))progress=MovieOuterHeaderBatchProgress::sequence_end;
                else continue;
            }else if(unit->kind==MovieAdmissionCandidateKind::first_slice)progress=MovieOuterHeaderBatchProgress::first_slice;
            else {s.failed=true;fail(f,MovieAdmissionFeedError::invalid_order);return {};}
            auto after=s.boundary.snapshot(f.boundary);if(!after){s.failed=true;fail(f,MovieAdmissionFeedError::allocation);return {};}
            if(!s.batch_units||after->es_begin<=s.batch_origin->prefix.es_prefix_first_byte){s.failed=true;fail(f,MovieAdmissionFeedError::invalid_order);return {};}
            auto current=std::make_shared<const MovieAdmissionCurrent>(s.current);
            auto batch=std::shared_ptr<const MovieOuterHeaderBatch>(new MovieOuterHeaderBatch(
                *s.batch_origin,progress==MovieOuterHeaderBatchProgress::first_slice?unit->prefix:std::nullopt,
                after->es_begin,s.batch_serial+1,s.batch_units,std::move(current),s.batch_offered_end,s.batch_timestamp_prefix));
            ++s.batch_serial;s.batch_origin.reset();s.batch_units=0;
            return MovieOuterHeaderBatchResult{progress,std::move(batch)};
        }
    }catch(const std::bad_alloc&){s.failed=true;fail(f,MovieAdmissionFeedError::allocation);return {};}
     catch(const std::length_error&){s.failed=true;fail(f,MovieAdmissionFeedError::host_capacity);return {};}
}
std::optional<MoviePictureBodyExtent> NativeMovieAdmissionFeed::picture_body_extent(MovieAdmissionFeedFailure& f) const noexcept {
    if (!access(f)) return {};
    Guard guard(state_->in_call);
    const auto& s = *state_;
    if (!s.pending || !s.current.picture || !s.current.coding) { fail(f, MovieAdmissionFeedError::invalid_order); return {}; }
    auto view = s.boundary.snapshot(f.boundary);
    if (!view) { fail(f, MovieAdmissionFeedError::allocation); return {}; }
    auto first = s.prefixes.find(view->es_begin);
    if (first == s.prefixes.end() || (first->second.prefix.raw_codeword & 255) != 1) { fail(f, MovieAdmissionFeedError::invalid_order); return {}; }
    MoviePictureBodyExtent out;
    out.es_begin = view->es_begin; out.es_end = view->es_begin;
    out.first_slice = first->second; out.current = s.current;
    const auto& bytes = view->bytes;
    std::size_t at = 0;
    for (;;) {
        const auto code = bytes[at + 3];
        if (code < 1 || code > 0xaf) {
            out.state = MoviePictureBodyExtentState::boundary_found; out.es_end = view->es_begin + at; out.next_header = s.prefixes.at(out.es_end); return out;
        }
        ++out.slice_count;
        std::size_t next = at + 4;
        while (next + 4 <= bytes.size() && (bytes[next] || bytes[next + 1] || bytes[next + 2] != 1)) ++next;
        if (next + 4 <= bytes.size()) {
            if (!s.prefixes.contains(view->es_begin + next)) {
                if (s.program_end) { fail(f, MovieAdmissionFeedError::invalid_prefix); return {}; }
                return out; // Metadata may still arrive while input is open.
            }
            if (next == at + 4) { fail(f, MovieAdmissionFeedError::parser); return {}; }
            at = next; continue;
        }
        if (!s.program_end) return out;
        if (bytes.size() == at + 4 || (bytes.size() >= 3 && bytes[bytes.size()-3] == 0 && bytes[bytes.size()-2] == 0 && bytes.back() == 1)) {
            fail(f, MovieAdmissionFeedError::parser); return {};
        }
        out.state = MoviePictureBodyExtentState::boundary_found; out.es_end = view->es_end; out.ended_at_program_end = true; return out;
    }
}
std::optional<MoviePictureBodyExtent> NativeMovieAdmissionFeed::accepted_picture_body(
        std::shared_ptr<const MovieCodecInputReceipt> receipt, MovieAdmissionFeedFailure& f) const noexcept {
    if (!access(f)) return {};
    Guard guard(state_->in_call);
    const auto& s = *state_;
    if (!receipt) { f.input_receipt=MovieInputReceiptError::null_receipt; fail(f,MovieAdmissionFeedError::invalid_source);return {}; }
    if (receipt->source_key()!=s.current.source->source_key||receipt->source_identity()!=s.current.source->identity) {
        f.input_receipt=MovieInputReceiptError::foreign_source;fail(f,MovieAdmissionFeedError::foreign_source);return {};
    }
    if (!s.pending||!s.current.picture||!s.current.coding||!s.last_input_receipt) {fail(f,MovieAdmissionFeedError::invalid_order);return {};}
    if (receipt->stream_id()!=s.last_input_receipt->stream_id()||receipt->stream_id()!=s.current.picture->origin.pes.stream_id||receipt->stream_id()!=s.current.coding->origin.pes.stream_id||receipt->input_id()>s.last_input_receipt->input_id()||
        receipt->es_begin()>=receipt->es_end()||receipt->es_end()>s.last_input_receipt->es_end()) {fail(f,MovieAdmissionFeedError::invalid_order);return {};}
    auto view=s.boundary.snapshot(f.boundary);if(!view){fail(f,MovieAdmissionFeedError::allocation);return {};}
    const auto picture=s.current.picture->origin.prefix.es_prefix_first_byte,coding=s.current.coding->origin.prefix.es_prefix_first_byte;
    if (picture<receipt->es_begin()||picture>=receipt->es_end()||coding<=picture||coding>=receipt->es_end()||
        s.current.coding->consumed_end>view->es_begin||view->es_begin<receipt->es_begin()||view->es_begin>=receipt->es_end()||receipt->es_end()>view->es_end) {fail(f,MovieAdmissionFeedError::range);return {};}
    auto first=s.prefixes.find(view->es_begin);
    if(first==s.prefixes.end()||(first->second.prefix.raw_codeword&255)!=1||first->second.pes.stream_id!=receipt->stream_id()){fail(f,MovieAdmissionFeedError::invalid_prefix);return {};}
    MoviePictureBodyExtent out;out.es_begin=view->es_begin;out.es_end=receipt->es_end();out.first_slice=first->second;out.current=s.current;
    const auto limit=static_cast<std::size_t>(receipt->es_end()-view->es_begin);const auto& bytes=view->bytes;if(limit<4){fail(f,MovieAdmissionFeedError::range);return {};}std::size_t at=0;
    for(;;){
        const auto observed=s.prefixes.find(view->es_begin+at);
        if(observed==s.prefixes.end()){fail(f,MovieAdmissionFeedError::invalid_prefix);return {};}
        const auto code=bytes[at+3];
        if(code==0xb7&&at+4==limit&&out.slice_count){out.es_end=view->es_begin+at;out.next_header=observed->second;break;}
        if(code<1||code>0xaf){fail(f,MovieAdmissionFeedError::unsupported);return {};}
        ++out.slice_count;std::size_t next=at+4;
        while(next+4<=limit&&(bytes[next]||bytes[next+1]||bytes[next+2]!=1))++next;
        if(next+4<=limit){if(next==at+4){fail(f,MovieAdmissionFeedError::parser);return {};}at=next;continue;}
        if(limit==at+4||(limit>=3&&bytes[limit-3]==0&&bytes[limit-2]==0&&bytes[limit-1]==1)){fail(f,MovieAdmissionFeedError::parser);return {};}
        break;
    }
    out.state=MoviePictureBodyExtentState::boundary_found;return out;
}
std::optional<MovieHeaderBoundaryProposal> NativeMovieAdmissionFeed::proposal(MovieAdmissionFeedFailure& f) noexcept {
    if (!access(f)) return {};
    Guard guard(state_->in_call);
    auto value = state_->boundary.propose(f.boundary);
    if (!value) fail(f, MovieAdmissionFeedError::range); return value;
}
bool NativeMovieAdmissionFeed::skip(std::uint64_t amount, MovieAdmissionFeedFailure& f) noexcept {
    if (!access(f)) return false; Guard guard(state_->in_call);
    if(state_->batch_origin)return fail(f,MovieAdmissionFeedError::scope);
    return consume(amount, MovieHeaderConsumeKind::skip, f);
}
std::optional<MovieAdmissionFeedSnapshot> NativeMovieAdmissionFeed::snapshot(MovieAdmissionFeedFailure& f) const noexcept {
    if (!access(f, true)) return {};
    Guard guard(state_->in_call);
    auto view = state_->boundary.snapshot(f.boundary);
    if (!view) { fail(f, MovieAdmissionFeedError::allocation); return {}; }
    return MovieAdmissionFeedSnapshot{state_->current, state_->sequence, view->es_begin, view->es_end,
        view->bytes.size(), state_->pending, state_->program_end, state_->failed,state_->last_input_receipt,state_->inner_active,bool(state_->batch_origin)};
}
} // namespace mka::video
