#include "mka/movie_kernel_policies.hpp"
#include <exception>
#include <new>

namespace mka::video {
namespace {
bool fail(MovieControlFailure& f, const char* op, std::int64_t code = 0) noexcept {
    if (f.kind == MovieControlError::none) f = {MovieControlError::native_service, op, code};
    return false;
}
bool same_request(const RetailMovieRequest& a, const RetailMovieRequest& b) noexcept {
    return a.id == b.id && a.logical_key == b.logical_key &&
        a.logical_extent.width == b.logical_extent.width && a.logical_extent.height == b.logical_extent.height &&
        a.requested_view == b.requested_view && a.selected_view == b.selected_view &&
        a.route == b.route && a.route_extra == b.route_extra &&
        a.subtitle_language == b.subtitle_language && a.subtitle_tag == b.subtitle_tag;
}
bool valid_picture(const MoviePictureSource& p) noexcept {
    const auto& s = p.pes;
    return s.stream_id >= 0xe0 && s.stream_id <= 0xef &&
        s.packet_offset < s.payload_offset && s.payload_offset < s.packet_end &&
        s.es_begin < s.es_end && p.packet_es_begin <= p.picture_es_byte &&
        p.picture_es_byte >= s.es_begin && p.picture_es_byte < s.es_end &&
        p.quarter_pts == s.quarter_pts();
}
}

NativeMovieKernelPolicies::NativeMovieKernelPolicies(RetailMovieRequest r,
        MoviePlaybackAudioPresence presence, std::unique_ptr<NativeMoviePcmBinding> pcm,
        std::unique_ptr<NativeMovieFrameEligibility> eligibility,
        std::unique_ptr<MoviePlaybackPolicyContext> context)
    : request_(std::move(r)), audio_presence_(presence), pcm_(std::move(pcm)),
      eligibility_(std::move(eligibility)), context_(std::move(context)) {}

std::unique_ptr<NativeMovieKernelPolicies> NativeMovieKernelPolicies::create(
        RetailMovieRequest request, MoviePlaybackAudioPresence presence,
        std::unique_ptr<NativeMoviePcmBinding> pcm, MovieFrameEligibilityConfig config,
        std::unique_ptr<MoviePlaybackPolicyContext> context, MovieControlFailure& f) noexcept {
    f = {};
    if (!context) { fail(f, "movie policies missing source timeline/lifecycle context"); return {}; }
    if (request.logical_key.empty() || request.logical_key.find('\0') != std::string::npos ||
        !request.logical_extent.width || !request.logical_extent.height || request.route != MovieRoute::blocking) {
        fail(f, "movie policies invalid owned retail request"); return {};
    }
    if ((presence != MoviePlaybackAudioPresence::absent && presence != MoviePlaybackAudioPresence::required) ||
        (presence == MoviePlaybackAudioPresence::required) != bool(pcm)) {
        fail(f, "movie policies missing explicit audio presence/profile"); return {};
    }
    if (pcm && pcm->source_key() != request.logical_key) {
        fail(f, "movie policies PCM source identity mismatch"); return {};
    }
    MovieFrameEligibilityFailure ef;
    auto eligibility = NativeMovieFrameEligibility::create(config, ef);
    if (!eligibility) { fail(f, "movie policies ordinary eligibility configuration", static_cast<std::int64_t>(ef.kind)); return {}; }
    try {
        return std::unique_ptr<NativeMovieKernelPolicies>(new NativeMovieKernelPolicies(
            std::move(request), presence, std::move(pcm), std::move(eligibility), std::move(context)));
    } catch (const std::bad_alloc&) { fail(f, "movie policies native owner allocation"); return {}; }
}
NativeMovieKernelPolicies::~NativeMovieKernelPolicies() {
    if (std::this_thread::get_id() != owner_ || in_call_) std::terminate();
    MovieControlFailure f;
    stop(f);
}
template<class F> bool NativeMovieKernelPolicies::invoke(MovieControlFailure& f, bool needs_active, F&& body) noexcept {
    f = {};
    if (std::this_thread::get_id() != owner_) {
        f = {MovieControlError::wrong_thread, "movie policies owner thread", 0}; return false;
    }
    if (in_call_) { f = {MovieControlError::invalid_state, "movie policies non-reentrant operation", 0}; return false; }
    if (needs_active && (!active_ || closed_)) {
        f = {MovieControlError::invalid_state, "movie policies inactive single-use owner", 0}; return false;
    }
    in_call_ = true;
    struct Guard { bool& busy; ~Guard() { busy = false; } } guard{in_call_};
    try {
        if (!body()) return fail(f, "movie policies required provider failed");
        return f.kind == MovieControlError::none;
    } catch (const std::bad_alloc&) { return fail(f, "movie policies host allocation"); }
      catch (...) { return fail(f, "movie policies provider exception"); }
}
bool NativeMovieKernelPolicies::validate_audio_format(const std::optional<sound::AudioStreamFormat>& format,
        MovieControlFailure& f) noexcept {
    return invoke(f, false, [&] {
        if (used_ || closed_) return fail(f, "movie policies format join after initialization");
        if (pcm_) {
            if (!format || *format != pcm_->client_format())
                return fail(f, "movie policies PCM profile differs from actual opened client format");
        } else if (format) return fail(f, "movie policies actual audio in explicitly absent source");
        format_joined_ = true;
        return true;
    });
}
bool NativeMovieKernelPolicies::begin(const RetailMovieRequest& request, std::int32_t gain, MovieControlFailure& f) noexcept {
    return invoke(f, false, [&] {
        if (used_ || closed_) return fail(f, "movie policies source is single use");
        if (!format_joined_) return fail(f, "movie policies actual client format not joined");
        if (!same_request(request_, request)) return fail(f, "movie policies full request identity mismatch");
        used_ = true; cleanup_needed_ = true;
        if (!context_->begin(request_, gain, f) || f.kind != MovieControlError::none) return false;
        gain_ = gain; active_ = true; return true;
    });
}
bool NativeMovieKernelPolicies::pump_metadata(MovieControlFailure& f) noexcept {
    return invoke(f, true, [&] { return context_->pump_metadata(f); });
}
bool NativeMovieKernelPolicies::pause(bool paused, MovieControlFailure& f) noexcept {
    return invoke(f, false, [&] {
        if (closed_) return fail(f, "movie policies pause after cleanup");
        cleanup_needed_ = true;
        return context_->pause(paused, f);
    });
}
bool NativeMovieKernelPolicies::ready(const MoviePlaybackSnapshot& s, bool& out, MovieControlFailure& f) noexcept {
    return invoke(f, true, [&] {
        bool value = false;
        if (!context_->ready(s, value, f) || f.kind != MovieControlError::none) return false;
        out = value; return true;
    });
}
bool NativeMovieKernelPolicies::eligible(const MovieVideoFrame& frame, const MoviePlaybackSnapshot& s,
        bool& out, MovieControlFailure& f) noexcept {
    return invoke(f, true, [&] {
        if (!frame.codec_timing.picture_source || !valid_picture(*frame.codec_timing.picture_source))
            return fail(f, "movie policies missing selected picture provenance");
        auto decision = context_->decision(frame, s, f);
        if (!decision || f.kind != MovieControlError::none) return fail(f, "movie policies missing ordinary decision inputs");
        if (decision->source_key != request_.logical_key) return fail(f, "movie policies decision source identity mismatch");
        if (decision->selected_picture != *frame.codec_timing.picture_source)
            return fail(f, "movie policies selected picture identity mismatch");
        if (!valid_picture(decision->rate_record_picture) || decision->associated_sequence_rate_code < 0 ||
            decision->associated_sequence_rate_code > 15)
            return fail(f, "movie policies missing associated parser rate provenance");
        if (decision->ordinary_clock.denominator <= 0) return fail(f, "movie policies missing ordinary clock");
        if (decision->adjusted_target.denominator <= 0) return fail(f, "movie policies missing adjusted target");
        MovieFrameEligibilityFailure ef;
        auto result = eligibility_->evaluate(decision->ordinary_clock, decision->adjusted_target,
            decision->associated_sequence_rate_code, ef);
        if (!result) return fail(f, "movie policies ordinary eligibility failed", static_cast<std::int64_t>(ef.kind));
        // Everything that can reject provider input has completed before the
        // adaptive state commits. These remaining copies are non-allocating.
        last_ = *result;
        pending_picture_ = result->eligible ? std::optional<MoviePictureSource>(decision->selected_picture) : std::nullopt;
        out = result->eligible; return true;
    });
}
bool NativeMovieKernelPolicies::video_submitted(const MovieGpuMetadata& metadata, MovieControlFailure& f) noexcept {
    return invoke(f, true, [&] {
        if (!pending_picture_ || metadata.codec_timing.picture_source != pending_picture_)
            return fail(f, "movie policies submission is not admitted selected picture");
        if (!context_->video_submitted(metadata, f) || f.kind != MovieControlError::none) return false;
        pending_picture_.reset(); return true;
    });
}
bool NativeMovieKernelPolicies::prepare_audio(const MovieAudioFrame& frame, std::int32_t gain,
        std::vector<float>& out, MovieControlFailure& f) noexcept {
    return invoke(f, true, [&] {
        if (!pcm_) return fail(f, "movie policies audio in explicitly absent source");
        if (gain != gain_) return fail(f, "movie policies requested gain generation mismatch");
        MoviePcmBindingFailure pf;
        if (!pcm_->prepare_audio(frame, gain, out, pf))
            return fail(f, "movie policies native PCM preparation failed", static_cast<std::int64_t>(pf.kind));
        return true;
    });
}
bool NativeMovieKernelPolicies::audio_accepted(const MoviePlaybackAudioAcceptance& a, MovieControlFailure& f) noexcept {
    return invoke(f, true, [&] {
        if (audio_presence_ != MoviePlaybackAudioPresence::required) return fail(f, "movie policies receipt in explicitly absent source");
        return context_->audio_accepted(a, f);
    });
}
bool NativeMovieKernelPolicies::set_gain(std::int32_t gain, MovieControlFailure& f) noexcept {
    return invoke(f, true, [&] { gain_ = gain; return true; });
}
bool NativeMovieKernelPolicies::end_admitted(const MoviePlaybackSnapshot& s, bool& out, MovieControlFailure& f) noexcept {
    return invoke(f, true, [&] {
        bool value = false;
        if (!context_->end_admitted(s, value, f) || f.kind != MovieControlError::none) return false;
        out = value; return true;
    });
}
bool NativeMovieKernelPolicies::stop(MovieControlFailure& f) noexcept {
    return invoke(f, false, [&] {
        if (closed_) return true;
        closed_ = true; active_ = false; pending_picture_.reset();
        if (!cleanup_needed_) return true;
        cleanup_needed_ = false;
        return context_->stop(f);
    });
}
} // namespace mka::video
