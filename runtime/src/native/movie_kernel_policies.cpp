#include "mka/movie_admission_feed.hpp"
#include "mka/movie_kernel_policies.hpp"
#include "mka/movie_input_acceptance.hpp"
#include <exception>
#include <new>
#include <type_traits>

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

std::shared_ptr<MovieEligibilityServiceBinding> MovieEligibilityServiceBinding::create(
        std::string key,NativeMovieServiceClock&& clock,MovieControlFailure& f) noexcept {
    f={};
    if(key.empty()||key.find('\0')!=std::string::npos||!clock.valid()) {
        fail(f,"movie eligibility invalid source service owner");return {};
    }
    try {return std::shared_ptr<MovieEligibilityServiceBinding>(new MovieEligibilityServiceBinding(std::move(key),std::move(clock)));}
    catch(const std::bad_alloc&){fail(f,"movie eligibility service owner allocation");return {};}
}
NativeMovieServiceClock* MovieEligibilityServiceBinding::service(MovieControlFailure& f) noexcept {
    f={};
    if(std::this_thread::get_id()!=owner_){f={MovieControlError::wrong_thread,"movie eligibility service owner thread",0};return nullptr;}
    if(!clock_.valid()){fail(f,"movie eligibility moved service clock");return nullptr;}
    if(clock_.state().frequency!=configured_frequency_){fail(f,"movie eligibility service frequency changed after binding");return nullptr;}
    return &clock_;
}

NativeMovieKernelPolicies::NativeMovieKernelPolicies(RetailMovieRequest r,
        MoviePlaybackAudioPresence presence, std::unique_ptr<NativeMoviePcmBinding> pcm,
        std::unique_ptr<NativeMovieFrameEligibility> eligibility,
        std::unique_ptr<MoviePlaybackPolicyContext> context,std::shared_ptr<MovieEligibilityServiceBinding> service)
    : request_(std::move(r)), audio_presence_(presence), pcm_(std::move(pcm)),
      eligibility_(std::move(eligibility)), context_(std::move(context)),service_(std::move(service)) {}

std::unique_ptr<NativeMovieKernelPolicies> NativeMovieKernelPolicies::create(
        RetailMovieRequest request, MoviePlaybackAudioPresence presence,
        std::unique_ptr<NativeMoviePcmBinding> pcm, MovieFrameEligibilityConfig config,
        std::unique_ptr<MoviePlaybackPolicyContext> context, MovieControlFailure& f) noexcept {
    return create(std::move(request),presence,std::move(pcm),config,std::move(context),{},f);
}
std::unique_ptr<NativeMovieKernelPolicies> NativeMovieKernelPolicies::create(
        RetailMovieRequest request, MoviePlaybackAudioPresence presence,
        std::unique_ptr<NativeMoviePcmBinding> pcm, MovieFrameEligibilityConfig config,
        std::unique_ptr<MoviePlaybackPolicyContext> context,
        std::shared_ptr<MovieEligibilityServiceBinding> service,MovieControlFailure& f) noexcept {
    f = {};
    if(service) {
        if(service->source_key()!=request.logical_key){fail(f,"movie policies invalid bound service identity");return {};}
        auto* clock=service->service(f);if(!clock)return {};
        if(clock->state().frequency!=config.frequency){fail(f,"movie policies source service frequency mismatch");return {};}
    }
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
            std::move(request), presence, std::move(pcm), std::move(eligibility), std::move(context),std::move(service)));
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
bool NativeMovieKernelPolicies::validate_observation_source(
        std::shared_ptr<const MovieSourceObservationIdentity> identity,MovieControlFailure& f) noexcept {
    return invoke(f,false,[&]{
        if(used_||closed_||!identity||(source_identity_&&source_identity_!=identity))
            return fail(f,"movie policies missing or changed actual source observation identity");
        if(source_identity_)return true;
        if(!context_->bind_source(identity,f)||f.kind!=MovieControlError::none)
            return fail(f,"movie policies source context identity join failed");
        source_identity_=std::move(identity);return true;
    });
}
bool NativeMovieKernelPolicies::observe_source(const MovieSourceObservationBatch& batch,MovieControlFailure& f) noexcept {
    return invoke(f,true,[&]{
        if(batch.source_key!=request_.logical_key||!batch.identity||batch.identity!=source_identity_||batch.events.empty())
            return fail(f,"movie policies source observation identity/key/extent mismatch");
        auto sequence=observation_sequence_,video_end=observed_video_end_;
        auto prefix=observed_prefix_;auto ended=observed_program_end_;auto accepted=observed_input_receipt_;
        auto acceptance_error=MovieInputReceiptError::none;
        for(const auto& event:batch.events){
            if(sequence==UINT64_MAX||event.sequence!=sequence+1)
                return fail(f,"movie policies source observation sequence mismatch");
            const bool valid=std::visit([&](const auto& value){
                using T=std::decay_t<decltype(value)>;
                if constexpr(std::is_same_v<T,MovieVideoEsObserved>){
                    if(ended||value.bytes.empty()||value.es_begin!=video_end||value.es_end<=value.es_begin||
                       value.es_end-value.es_begin!=value.bytes.size())return false;
                    video_end=value.es_end;return true;
                }else if constexpr(std::is_same_v<T,MovieVideoPrefixObserved>){
                    const auto& raw=value.prefix;const auto& pes=value.pes;
                    if(ended||(raw.raw_codeword&0xffffff00u)!=0x00000100u||
                       raw.classification!=movie_prefix_classification(raw.raw_codeword)||
                       raw.es_prefix_first_byte>video_end||video_end-raw.es_prefix_first_byte<4||
                       (prefix&&raw.es_prefix_first_byte<=*prefix)||
                       pes.stream_id<0xe0||pes.stream_id>0xef||pes.packet_offset>=pes.payload_offset||
                       pes.payload_offset>=pes.packet_end||pes.es_begin>=pes.es_end||
                       raw.es_prefix_first_byte<pes.es_begin||raw.es_prefix_first_byte>=pes.es_end||
                       value.quarter_pts!=pes.quarter_pts())return false;
                    prefix=raw.es_prefix_first_byte;return true;
                }else if constexpr(std::is_same_v<T,MovieDecodedPictureObserved>){
                    return valid_picture(value.picture)&&value.picture.picture_es_byte<=video_end&&
                        video_end-value.picture.picture_es_byte>=4&&
                        value.timing.packet_picture_offset==value.picture.picture_es_byte-value.picture.packet_es_begin;
                }else if constexpr(std::is_same_v<T,MovieProgramEndObservation>){
                    if(ended||!video_end||value.byte_extent!=4||value.file_offset>UINT64_MAX-4||
                       value.trailing_padding_bytes>UINT64_MAX-value.file_offset-4)return false;
                    ended=true;return true;
                }else if constexpr(std::is_same_v<T,MovieCodecInputAccepted>){
                    acceptance_error=check_movie_input_receipt(value.receipt,batch.source_key,batch.identity,accepted,video_end,ended);
                    if(acceptance_error!=MovieInputReceiptError::none)return false;
                    accepted=value.receipt;return true;
                }
            },event.value);
            if(!valid)return fail(f,"movie policies invalid source observation data/order",static_cast<std::int64_t>(acceptance_error));
            sequence=event.sequence;
        }
        // Required source-specific consumer receives the actual producer data.
        // It must own/validate any parser/admission/output/lifecycle semantics.
        if(!context_->observe_source(batch,f)||f.kind!=MovieControlError::none)return false;
        observation_sequence_=sequence;observed_video_end_=video_end;
        observed_prefix_=prefix;observed_program_end_=ended;observed_input_receipt_=std::move(accepted);return true;
    });
}
bool NativeMovieKernelPolicies::begin(const RetailMovieRequest& request, std::int32_t gain, MovieControlFailure& f) noexcept {
    return invoke(f, false, [&] {
        if (used_ || closed_) return fail(f, "movie policies source is single use");
        if (!format_joined_) return fail(f, "movie policies actual client format not joined");
        if (!source_identity_) return fail(f,"movie policies actual source observation identity not joined");
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
        const auto valid_common=std::visit([&](const auto& value){
            return value.source_key==request_.logical_key&&value.selected_picture==*frame.codec_timing.picture_source;
        },*decision);
        if(!valid_common)return fail(f,"movie policies decision source or selected picture identity mismatch");
        bool eligible=false;MovieEligibilityRoute selected_route;
        std::optional<MovieFrameEligibilityResult> ordinary;
        if(auto* value=std::get_if<MovieOrdinaryDecision>(&*decision)) {
            const auto& rate=value->rate_record;
            auto bound=rate.admitted_header_es_byte;
            if(rate.parsed_batch){
                const auto& batch=*rate.parsed_batch;const auto& current=batch.current();
                if(!current||!current->source||current->source->source_key!=rate.source_key||current->source->identity!=rate.identity||
                   !current->sequence||current->sequence->origin.prefix.es_prefix_first_byte!=rate.sequence_header_es_byte||
                   batch.initiating_prefix().prefix.es_prefix_first_byte!=rate.admitted_header_es_byte||batch.parse_end()<4)
                    return fail(f,"movie policies foreign parsed batch rate provenance");
                const auto* actual=std::get_if<MovieSequenceHeader>(&current->sequence->unit.payload);
                if(!actual||actual->frame_rate_code!=rate.frame_rate_code)return fail(f,"movie policies altered authentic batch sequence rate");
                bound=batch.parse_end()-4;
            }
            if(rate.source_key!=request_.logical_key||!rate.identity||rate.identity!=source_identity_||
               !rate.admission_id||rate.sequence_header_es_byte>bound||
               rate.frame_rate_code<0||rate.frame_rate_code>15)
                return fail(f, "movie policies missing associated parser rate provenance");
            if (value->ordinary_clock.denominator <= 0) return fail(f, "movie policies missing ordinary clock");
            if (value->adjusted_target.denominator <= 0) return fail(f, "movie policies missing adjusted target");
            MovieFrameEligibilityFailure ef;
            ordinary=eligibility_->evaluate(value->ordinary_clock,value->adjusted_target,rate.frame_rate_code,ef);
            if(!ordinary)return fail(f,"movie policies ordinary eligibility failed",static_cast<std::int64_t>(ef.kind));
            selected_route=MovieEligibilityRoute::ordinary;eligible=ordinary->eligible;
        } else if(std::holds_alternative<MovieSourceDirectDecision>(*decision)) {
            selected_route=MovieEligibilityRoute::source_direct;eligible=true;
        } else {
            const auto& value=std::get<MovieSourceRetainedDecision>(*decision);
            if(!service_||!value.service||value.service!=service_||value.service->source_key()!=request_.logical_key)
                return fail(f,"movie policies retained service identity mismatch");
            if(value.adjusted_target.denominator<=0)return fail(f,"movie policies invalid retained target");
            auto* clock=service_->service(f);if(!clock)return false;
            MovieServiceClockFailure why;
            auto result=clock->eligible({value.adjusted_target.numerator,value.adjusted_target.denominator},why);
            if(!result)return fail(f,"movie policies retained eligibility failed",static_cast<std::int64_t>(why.kind));
            selected_route=MovieEligibilityRoute::source_retained;eligible=*result;
        }
        // Branch kernels have succeeded; remaining scalar/value copies cannot
        // allocate. Historical ordinary diagnostics stay explicitly separate.
        if(ordinary)last_=*ordinary;
        route_=MoviePolicyRouteResult{selected_route,eligible};
        pending_picture_=eligible?frame.codec_timing.picture_source:std::nullopt;
        out=eligible;return true;
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
