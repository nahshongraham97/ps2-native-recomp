#include "mka/movie_native_context.hpp"
#include <array>
#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
#include <type_traits>
namespace mka::video {
namespace {
using Wide=__int128;
static_assert(std::is_nothrow_move_assignable_v<MovieParserRateRecord>);
struct Guard {bool& value;explicit Guard(bool& v):value(v){value=true;}~Guard(){value=false;}};
bool same_request(const RetailMovieRequest& a,const RetailMovieRequest& b) noexcept {
    return a.id==b.id&&a.logical_key==b.logical_key&&a.logical_extent.width==b.logical_extent.width&&a.logical_extent.height==b.logical_extent.height&&a.requested_view==b.requested_view&&a.selected_view==b.selected_view&&a.route==b.route&&a.route_extra==b.route_extra&&a.subtitle_language==b.subtitle_language&&a.subtitle_tag==b.subtitle_tag;
}
bool store(Wide n,std::int64_t& out) noexcept {if(n<std::numeric_limits<std::int64_t>::min()||n>std::numeric_limits<std::int64_t>::max())return false;out=static_cast<std::int64_t>(n);return true;}
bool picture_valid(const MoviePictureSource& p) noexcept {return p.pes.packet_offset<p.pes.payload_offset&&p.pes.payload_offset<p.pes.packet_end&&p.pes.stream_id>=0xe0&&p.pes.stream_id<=0xef&&p.pes.es_begin<=p.picture_es_byte&&p.picture_es_byte<p.pes.es_end&&p.packet_es_begin<=p.picture_es_byte&&p.quarter_pts==p.pes.quarter_pts();}
constexpr std::array<std::int64_t,9> rates{1,23976,24000,25000,29970,30000,50000,59940,60000};
}
NativeMoviePlaybackContext::NativeMoviePlaybackContext(MovieNativeContextConfiguration c,NativeMovieClockFilter&& filter) noexcept:config_(std::move(c)),filter_(std::move(filter)) {
    state_.bound=bool(config_.identity);state_.paused=config_.filter.paused;state_.next_source_audio_frame=config_.audio_source_origin;
}
bool NativeMoviePlaybackContext::fail(MovieControlFailure& f,MovieNativeContextError e,const char* operation,std::int64_t detail) const noexcept {f={MovieControlError::native_service,operation,detail?detail:static_cast<std::int64_t>(e)};return false;}
bool NativeMoviePlaybackContext::enter(MovieControlFailure& f,bool active) const noexcept {
    f={};if(std::this_thread::get_id()!=owner_){f={MovieControlError::wrong_thread,"movie native context owner thread",0};return false;}
    if(in_call_){f={MovieControlError::invalid_state,"movie native context reentry",0};return false;}
    if(state_.closed||(active&&!state_.begun)){f={MovieControlError::invalid_state,"movie native context inactive",0};return false;}return true;
}
std::unique_ptr<NativeMoviePlaybackContext> NativeMoviePlaybackContext::create(MovieNativeContextConfiguration c,MovieControlFailure& f) noexcept {
    f={};auto invalid=[&](const char* text){f={MovieControlError::native_service,text,static_cast<std::int64_t>(MovieNativeContextError::invalid_config)};};
    if(c.request.logical_key.empty()||c.request.logical_key.find('\0')!=std::string::npos||!c.request.logical_extent.width||!c.request.logical_extent.height||c.request.route!=MovieRoute::blocking||!c.service||c.service->source_key()!=c.request.logical_key){invalid("movie native context source configuration");return {};}
    if(c.audio_enabled&&!c.audio_source_rate){invalid("movie native context missing source audio rate");return {};}
    if((c.target_route!=MovieNativeTargetRoute::descriptor&&c.target_route!=MovieNativeTargetRoute::published_position)||(c.completion!=MovieNativeCompletion::all&&c.completion!=MovieNativeCompletion::audio&&c.completion!=MovieNativeCompletion::video&&c.completion!=MovieNativeCompletion::either)){invalid("movie native context unsupported policy selector");return {};}
    if((c.completion==MovieNativeCompletion::all&&c.video_enabled!=c.audio_enabled)||
       (c.completion==MovieNativeCompletion::audio&&!c.audio_enabled&&c.video_enabled)||
       (c.completion==MovieNativeCompletion::video&&!c.video_enabled&&c.audio_enabled)){
        invalid("movie native context completion selector requires disabled source stage");return {};
    }
    auto* service=c.service->service(f);if(!service)return {};
    MovieClockFilterFailure why;auto filter=NativeMovieClockFilter::create(c.filter,why);
    if(!filter){invalid(why.kind==MovieClockFilterError::allocation?"movie native context filter allocation":"movie native context filter configuration");f.native_code=static_cast<std::int64_t>(why.kind);return {};}
    try{return std::unique_ptr<NativeMoviePlaybackContext>(new NativeMoviePlaybackContext(std::move(c),std::move(*filter)));}
    catch(const std::bad_alloc&){invalid("movie native context allocation");f.native_code=static_cast<std::int64_t>(MovieNativeContextError::allocation);return {};}
}
bool NativeMoviePlaybackContext::bind_source(std::shared_ptr<const MovieSourceObservationIdentity> identity,MovieControlFailure& f) noexcept {
    if(!enter(f,false))return false;Guard guard(in_call_);
    if(!identity|| (config_.identity&&config_.identity!=identity))return fail(f,MovieNativeContextError::foreign_source,"movie native context source epoch mismatch");
    if(!config_.identity&&state_.begun)return fail(f,MovieNativeContextError::invalid_order,"movie native context late source binding");
    config_.identity=std::move(identity);state_.bound=true;return true;
}
bool NativeMoviePlaybackContext::begin(const RetailMovieRequest& request,std::int32_t,MovieControlFailure& f) noexcept {
    if(!enter(f,false))return false;Guard guard(in_call_);
    if(state_.begun||!config_.identity||!same_request(request,config_.request))return fail(f,MovieNativeContextError::foreign_source,"movie native context begin request/source mismatch");
    state_.begun=true;return true;
}
bool NativeMoviePlaybackContext::pump_metadata(MovieControlFailure& f) noexcept {if(!enter(f))return false;Guard guard(in_call_);return true;}
bool NativeMoviePlaybackContext::observe_source(const MovieSourceObservationBatch& batch,MovieControlFailure& f) noexcept {
    if(!enter(f))return false;Guard guard(in_call_);
    if(batch.events.empty()||batch.source_key!=config_.request.logical_key||batch.identity!=config_.identity)return fail(f,MovieNativeContextError::foreign_source,"movie native context observation source mismatch");
    auto sequence=state_.observation_sequence,end=observed_es_end_;auto last=last_prefix_;bool ended=state_.program_end_observed;auto accepted=state_.last_input_receipt;
    try {
        std::vector<std::uint8_t> extra;
        std::map<std::uint64_t,Decoded> additions;
        std::map<std::uint64_t,MovieVideoPrefixObserved> prefix_additions;
        auto code_at=[&](std::uint64_t offset)->std::uint32_t {std::uint32_t code=0;for(unsigned i=0;i<4;++i){const auto at=offset+i;const auto byte=at<source_bytes_.size()?source_bytes_[static_cast<std::size_t>(at)]:extra[static_cast<std::size_t>(at-source_bytes_.size())];code=(code<<8)|byte;}return code;};
        for(const auto& event:batch.events){
            if(sequence==std::numeric_limits<std::uint64_t>::max()||event.sequence!=sequence+1)return fail(f,MovieNativeContextError::invalid_order,"movie native context observation sequence");++sequence;
            if(const auto* raw=std::get_if<MovieVideoEsObserved>(&event.value)){
                if(ended||raw->es_begin!=end||raw->es_end<end||raw->es_end-end!=raw->bytes.size()||raw->es_end>source_bytes_.max_size())return fail(f,MovieNativeContextError::invalid_order,"movie native context raw ES order");
                extra.insert(extra.end(),raw->bytes.begin(),raw->bytes.end());end=raw->es_end;
            }else if(const auto* p=std::get_if<MovieVideoPrefixObserved>(&event.value)){
                const auto at=p->prefix.es_prefix_first_byte;
                if(ended||at>end||end-at<4||(last&&at<=*last)||p->pes.stream_id<0xe0||p->pes.stream_id>0xef||p->pes.packet_offset>=p->pes.payload_offset||p->pes.payload_offset>=p->pes.packet_end||p->pes.es_begin>at||at>=p->pes.es_end||p->quarter_pts!=p->pes.quarter_pts()||p->prefix.raw_codeword!=code_at(at)||p->prefix.classification!=movie_prefix_classification(p->prefix.raw_codeword))return fail(f,MovieNativeContextError::invalid_order,"movie native context actual prefix observation");last=at;prefix_additions.emplace(at,*p);
            }else if(const auto* decoded=std::get_if<MovieDecodedPictureObserved>(&event.value)){
                const auto& p=decoded->picture;const auto at=p.picture_es_byte;
                if(!picture_valid(p)||decoded->timing.packet_picture_offset!=at-p.packet_es_begin||at>end||end-at<4||code_at(at)!=0x100||decoded_.contains(at)||additions.contains(at))return fail(f,MovieNativeContextError::foreign_source,"movie native context decoded source observation");additions.emplace(at,Decoded{p,false});
            }else if(const auto* input=std::get_if<MovieCodecInputAccepted>(&event.value)){
                const auto error=check_movie_input_receipt(input->receipt,batch.source_key,batch.identity,accepted,end,ended);
                if(error!=MovieInputReceiptError::none)return fail(f,MovieNativeContextError::invalid_receipt,"movie native context codec input receipt",static_cast<std::int64_t>(error));
                accepted=input->receipt;
            }else if(const auto* control=std::get_if<MovieProgramEndObservation>(&event.value)){
                if(ended||!end||control->byte_extent!=4||control->file_offset>UINT64_MAX-4||control->trailing_padding_bytes>UINT64_MAX-control->file_offset-4)return fail(f,MovieNativeContextError::invalid_order,"movie native context program end replay");ended=true;
            }
        }
        source_bytes_.reserve(static_cast<std::size_t>(end));
        source_bytes_.insert(source_bytes_.end(),extra.begin(),extra.end());
        decoded_.merge(additions);prefixes_.merge(prefix_additions);observed_es_end_=end;last_prefix_=last;state_.observation_sequence=sequence;state_.program_end_observed=ended;state_.last_input_receipt=std::move(accepted);return true;
    }catch(const std::bad_alloc&){return fail(f,MovieNativeContextError::allocation,"movie native context observation allocation");}
    catch(const std::length_error&){return fail(f,MovieNativeContextError::host_capacity,"movie native context observation host capacity");}
}
bool NativeMoviePlaybackContext::pause(bool paused,MovieControlFailure& f) noexcept {
    if(!enter(f,false))return false;Guard guard(in_call_);MovieClockFilterFailure why;
    if(!filter_.set_paused(paused,why))return fail(f,MovieNativeContextError::filter,"movie native context pause filter",static_cast<std::int64_t>(why.kind));state_.paused=paused;return true;
}
std::optional<MovieHeaderUnit> NativeMoviePlaybackContext::checked_unit(const std::shared_ptr<const MovieCurrentHeaderUnit>& u,
        std::uint64_t admitted,std::uint32_t code,MovieControlFailure& f) const noexcept {
        if(!u){fail(f,MovieNativeContextError::missing_admission,"movie native context current parsed unit unavailable");return {};}
        const auto at=u->origin.prefix.es_prefix_first_byte;const auto& pes=u->origin.pes;
        auto observed=prefixes_.find(at);
        if(observed==prefixes_.end()||observed->second.prefix!=u->origin.prefix||
           observed->second.pes!=pes||observed->second.quarter_pts!=u->origin.quarter_pts){
            fail(f,MovieNativeContextError::foreign_source,"movie native context unobserved current prefix/PES origin");return {};
        }
        if(at>admitted||at>observed_es_end_||u->unit.consumed_bytes>observed_es_end_-at||
           u->unit.consumed_bytes<4||u->consumed_end!=at+u->unit.consumed_bytes||
           u->unit.bytes.size()!=u->unit.consumed_bytes||u->origin.prefix.raw_codeword!=code||
           u->origin.prefix.classification!=movie_prefix_classification(code)||
           pes.stream_id<0xe0||pes.stream_id>0xef||pes.packet_offset>=pes.payload_offset||pes.payload_offset>=pes.packet_end||
           pes.es_begin>at||at>=pes.es_end||u->origin.quarter_pts!=pes.quarter_pts()||
           !std::equal(u->unit.bytes.begin(),u->unit.bytes.end(),source_bytes_.begin()+static_cast<std::size_t>(at))){
            fail(f,MovieNativeContextError::foreign_source,"movie native context current unit bytes/origin mismatch");return {};
        }
        auto parsed=parse_movie_header_unit(u->unit.bytes);
        if(!parsed.unit||parsed.unit->consumed_bytes!=u->unit.consumed_bytes){
            fail(f,parsed.failure.kind==MovieHeaderUnitError::allocation?MovieNativeContextError::allocation:MovieNativeContextError::foreign_source,
                "movie native context checked current unit");return {};
        }
        return std::move(parsed.unit);
}
bool NativeMoviePlaybackContext::admit_header(const MovieNativeHeaderAdmission& event,MovieControlFailure& f) noexcept {
    if(!enter(f))return false;Guard guard(in_call_);
    const auto& current=event.current;
    if(!current||!current->source||current->source->source_key!=config_.request.logical_key||current->source->identity!=config_.identity)
        return fail(f,MovieNativeContextError::foreign_source,"movie native context admission source mismatch");
    if(state_.admissions==std::numeric_limits<std::uint64_t>::max()||event.admission_id!=state_.admissions+1)
        return fail(f,MovieNativeContextError::invalid_order,"movie native context admission order");
    const auto admitted=event.admitted_header_es_byte;
    auto checked_bound=admitted;
    if(event.parsed_batch){
        const auto& batch=*event.parsed_batch;const auto& origin=batch.initiating_prefix();
        auto seen=prefixes_.find(origin.prefix.es_prefix_first_byte);
        if(batch.current()!=current||!batch.batch_id()||!batch.parsed_units()||origin.prefix.es_prefix_first_byte!=admitted||
           batch.parse_end()<=admitted||batch.parse_end()>observed_es_end_||batch.parse_end()<4||
           seen==prefixes_.end()||seen->second.prefix!=origin.prefix||seen->second.pes!=origin.pes||seen->second.quarter_pts!=origin.quarter_pts)
            return fail(f,MovieNativeContextError::foreign_source,"movie native context unowned parsed outer batch");
        if(batch.stopping_prefix()){
            const auto& stop=*batch.stopping_prefix();auto observed=prefixes_.find(batch.parse_end());
            if(stop.prefix.es_prefix_first_byte!=batch.parse_end()||!(stop.prefix.classification&2)||
               observed_es_end_-batch.parse_end()<4||observed==prefixes_.end()||observed->second.prefix!=stop.prefix||
               observed->second.pes!=stop.pes||observed->second.quarter_pts!=stop.quarter_pts)
                return fail(f,MovieNativeContextError::foreign_source,"movie native context unobserved outer batch stop");
        }
        if(batch.offered_end()<admitted||batch.offered_end()>observed_es_end_||batch.offered_end()-admitted<4)
            return fail(f,MovieNativeContextError::foreign_source,"movie native context batch original offered range");
        if(batch.timestamp_prefix()){
            const auto& first=*batch.timestamp_prefix();const auto at=first.prefix.es_prefix_first_byte;auto actual=prefixes_.find(at);
            if(at<admitted||at>batch.offered_end()||batch.offered_end()-at<4||!(first.prefix.classification&4)||
               actual==prefixes_.end()||actual->second.prefix!=first.prefix||actual->second.pes!=first.pes||actual->second.quarter_pts!=first.quarter_pts)
                return fail(f,MovieNativeContextError::foreign_source,"movie native context original raw timestamp prefix");
        }
        for(const auto* unit:{&current->sequence,&current->sequence_extension,&current->display_extension,&current->gop,&current->picture,&current->coding,&current->quant_matrix,&current->userdata})
            if(*unit&&(*unit)->consumed_end>batch.parse_end())return fail(f,MovieNativeContextError::foreign_source,"movie native context unit beyond actual parsed batch");
        checked_bound=batch.parse_end()-4;
    }else{
        auto is_origin=[&](const std::shared_ptr<const MovieCurrentHeaderUnit>& u){return u&&u->origin.prefix.es_prefix_first_byte==admitted;};
        if(admitted>observed_es_end_||observed_es_end_-admitted<4||
           (!is_origin(current->sequence)&&!is_origin(current->gop)&&!is_origin(current->picture)))
            return fail(f,MovieNativeContextError::invalid_order,"movie native context actual admitted header origin");
    }
    // Validate actual parsed-unit bytes and the FIRST-byte PES owner. The
    // independently retained current units need no future picture/index record.
    auto sequence_unit=checked_unit(current->sequence,checked_bound,0x1b3,f);if(!sequence_unit)return false;
    const auto* sequence=std::get_if<MovieSequenceHeader>(&sequence_unit->payload);
    const auto* claimed_sequence=std::get_if<MovieSequenceHeader>(&current->sequence->unit.payload);
    if(!sequence||!claimed_sequence||sequence->frame_rate_code!=claimed_sequence->frame_rate_code)
        return fail(f,MovieNativeContextError::foreign_source,"movie native context serialized sequence rate mismatch");
    if(sequence->frame_rate_code<1||sequence->frame_rate_code>8)
        return fail(f,MovieNativeContextError::missing_rate,"movie native context source rate");
    std::optional<MovieTimecodeDescriptor> descriptor;std::optional<MovieTimecodeRational> time;
    auto baseline=state_.first_baseline;MovieTimecodeFailure why;
    if(event.descriptor_route){
        auto gop_unit=checked_unit(current->gop,checked_bound,0x1b8,f);if(!gop_unit)return false;
        const auto* gop=std::get_if<MovieGopHeader>(&gop_unit->payload);
        const auto* claimed_gop=std::get_if<MovieGopHeader>(&current->gop->unit.payload);
        if(!gop||!claimed_gop||gop->raw_time_code!=claimed_gop->raw_time_code||gop->closed_gop!=claimed_gop->closed_gop||gop->broken_link!=claimed_gop->broken_link)
            return fail(f,MovieNativeContextError::foreign_source,"movie native context serialized GOP mismatch");
        std::uint16_t temporal=0;
        if(!event.clear_temporal||*event.descriptor_route==MovieNativeDescriptorRoute::pes_position){
            auto picture_unit=checked_unit(current->picture,checked_bound,0x100,f);if(!picture_unit)return false;
            const auto* picture=std::get_if<MoviePictureHeader>(&picture_unit->payload);
            const auto* claimed_picture=std::get_if<MoviePictureHeader>(&current->picture->unit.payload);
            if(!picture||!claimed_picture||picture->temporal_reference!=claimed_picture->temporal_reference||picture->coding_type!=claimed_picture->coding_type)
                return fail(f,MovieNativeContextError::foreign_source,"movie native context serialized picture mismatch");
            temporal=picture->temporal_reference;
        }
        if(*event.descriptor_route==MovieNativeDescriptorRoute::gop)
            descriptor=MovieTimecodeDescriptor{sequence->frame_rate_code,gop->drop_frame,gop->hours,gop->minutes,gop->seconds,gop->pictures,temporal,0};
        else if(*event.descriptor_route==MovieNativeDescriptorRoute::pes_position){
            descriptor=convert_movie_position_timecode(current->picture->origin.quarter_pts,sequence->frame_rate_code,gop->drop_frame,temporal,why);
            if(!descriptor)return fail(f,MovieNativeContextError::timecode,"movie native context PES descriptor",static_cast<std::int64_t>(why.kind));
        }else return fail(f,MovieNativeContextError::invalid_config,"movie native context descriptor route");
        if(event.clear_temporal)descriptor->temporal_reference=0;
        time=convert_movie_timecode(*descriptor,why);if(!time)return fail(f,MovieNativeContextError::timecode,"movie native context admitted timecode",static_cast<std::int64_t>(why.kind));
        if(event.latch_first_baseline&&!baseline){auto first=*descriptor;first.temporal_reference=0;baseline=convert_movie_timecode(first,why);if(!baseline)return fail(f,MovieNativeContextError::timecode,"movie native context first baseline",static_cast<std::int64_t>(why.kind));}
    }else if(event.clear_temporal||event.latch_first_baseline)return fail(f,MovieNativeContextError::missing_admission,"movie native context rate-only admission has no descriptor");
    try {
        std::optional<MovieParserRateRecord> rate;
        if(event.latch_eligibility_rate)rate.emplace(MovieParserRateRecord{config_.request.logical_key,config_.identity,event.admission_id,admitted,current->sequence->origin.prefix.es_prefix_first_byte,sequence->frame_rate_code,event.parsed_batch});
        admissions_.emplace(event.admission_id,Admission{event,descriptor,time,sequence->frame_rate_code});
        if(rate)state_.eligibility_rate=std::move(rate);
        state_.first_baseline=baseline;++state_.admissions;return true;
    }catch(const std::bad_alloc&){return fail(f,MovieNativeContextError::allocation,"movie native context admission allocation");}
}
bool NativeMoviePlaybackContext::validate_work(const MovieAdmissionCurrent& current,MovieControlFailure& f) const noexcept {
    if(!enter(f))return false;Guard guard(in_call_);
    if(!current.source||current.source->source_key!=config_.request.logical_key||current.source->identity!=config_.identity)
        return fail(f,MovieNativeContextError::foreign_source,"movie native context work source mismatch");
    if(!current.picture||!current.coding||current.coding->origin.prefix.es_prefix_first_byte<=current.picture->origin.prefix.es_prefix_first_byte)
        return fail(f,MovieNativeContextError::missing_admission,"movie native context stale/missing current picture coding");
    // Raw start-code observations can lie INSIDE structured matrix/header
    // bytes. Walk checked unit extents; only an actual parsed picture breaks
    // the association between this picture and its coding extension.
    auto cursor=current.picture->consumed_end;
    const auto coding_at=current.coding->origin.prefix.es_prefix_first_byte;
    if(cursor>coding_at||coding_at>observed_es_end_)
        return fail(f,MovieNativeContextError::missing_admission,"movie native context overlapping picture/coding origins");
    while(cursor<coding_at){
        if(!prefixes_.contains(cursor))return fail(f,MovieNativeContextError::missing_admission,"movie native context missing structured work origin");
        auto between=parse_movie_header_unit(std::span(source_bytes_).subspan(static_cast<std::size_t>(cursor)));
        if(!between.unit){return fail(f,between.failure.kind==MovieHeaderUnitError::allocation?MovieNativeContextError::allocation:MovieNativeContextError::missing_admission,
            "movie native context intervening structured header");}
        if(std::holds_alternative<MoviePictureHeader>(between.unit->payload))
            return fail(f,MovieNativeContextError::missing_admission,"movie native context coding belongs to another parsed picture");
        const auto amount=between.unit->consumed_bytes;
        if(amount<4||amount>coding_at-cursor)return fail(f,MovieNativeContextError::missing_admission,"movie native context invalid intervening header extent");
        cursor+=amount;
    }
    auto picture_unit=checked_unit(current.picture,observed_es_end_,0x100,f);if(!picture_unit)return false;
    auto coding_unit=checked_unit(current.coding,observed_es_end_,0x1b5,f);if(!coding_unit)return false;
    const auto* picture=std::get_if<MoviePictureHeader>(&picture_unit->payload);
    const auto* claimed_picture=std::get_if<MoviePictureHeader>(&current.picture->unit.payload);
    const auto* coding=std::get_if<MoviePictureCodingExtension>(&coding_unit->payload);
    const auto* claimed_coding=std::get_if<MoviePictureCodingExtension>(&current.coding->unit.payload);
    if(!picture||!claimed_picture||picture->coding_type!=claimed_picture->coding_type||picture->temporal_reference!=claimed_picture->temporal_reference||
       !coding||!claimed_coding||coding->picture_structure!=claimed_coding->picture_structure||coding->repeat_first_field!=claimed_coding->repeat_first_field)
        return fail(f,MovieNativeContextError::foreign_source,"movie native context current work raw/typed profile mismatch");
    return true;
}
bool NativeMoviePlaybackContext::publish_output(const MovieNativeOutputPublication& event,MovieControlFailure& f) noexcept {
    if(!enter(f))return false;Guard guard(in_call_);
    auto work=admissions_.find(event.work_admission_id),timing=admissions_.find(event.timing_admission_id);auto decoded=decoded_.find(event.selected_picture.picture_es_byte);
    if(work==admissions_.end()||timing==admissions_.end())return fail(f,MovieNativeContextError::missing_admission,"movie native context output admission missing");
    if(decoded==decoded_.end()||decoded->second.source!=event.selected_picture||!picture_valid(event.selected_picture))return fail(f,MovieNativeContextError::foreign_source,"movie native context selected decoded identity");
    if(decoded->second.submitted||outputs_.contains(event.selected_picture.picture_es_byte))return fail(f,MovieNativeContextError::duplicate_output,"movie native context duplicate output");
    if(!timing->second.time||!timing->second.descriptor)return fail(f,MovieNativeContextError::missing_admission,"movie native context timing admission has no descriptor");
    auto baseline=state_.queued_baseline;MovieTimecodeFailure why;
    if(!baseline){if(!state_.first_baseline)return fail(f,MovieNativeContextError::missing_baseline,"movie native context missing first source baseline");auto descriptor=*timing->second.descriptor;if(!config_.preserve_queued_temporal)descriptor.temporal_reference=0;auto converted=convert_movie_timecode(descriptor,why);if(!converted)return fail(f,MovieNativeContextError::timecode,"movie native context queued baseline",static_cast<std::int64_t>(why.kind));std::int64_t n;if(!store(Wide(converted->numerator)-state_.first_baseline->numerator,n))return fail(f,MovieNativeContextError::overflow,"movie native context queued baseline overflow");baseline=MovieTimecodeRational{n,converted->denominator};}
    const auto& t=*timing->second.time;std::int64_t numerator;
    if(config_.target_route==MovieNativeTargetRoute::descriptor){if(!store(Wide(t.numerator)-state_.first_baseline->numerator-baseline->numerator+config_.target_offset,numerator))return fail(f,MovieNativeContextError::overflow,"movie native context descriptor target overflow");}
    else {if(!event.published_position)return fail(f,MovieNativeContextError::missing_position,"movie native context missing actual published position");const auto code=work->second.work_rate;
        // Verified timecode denominators <=60000; uint64 position difference,
        // denominator*1000 product <2^91, so signed128 cannot overflow.
        const Wide delta=Wide(*event.published_position)-state_.position_origin;
        if(!store((Wide(t.denominator)*1000*delta)/rates[code]+config_.target_offset,numerator))return fail(f,MovieNativeContextError::overflow,"movie native context position target overflow");}
    try{outputs_.emplace(event.selected_picture.picture_es_byte,Output{event.selected_picture,{numerator,t.denominator}});state_.queued_baseline=baseline;state_.outputs_pending=outputs_.size();return true;}
    catch(const std::bad_alloc&){return fail(f,MovieNativeContextError::allocation,"movie native context output allocation");}
}
bool NativeMoviePlaybackContext::advance_position_origin(std::uint64_t position,MovieControlFailure& f) noexcept {if(!enter(f))return false;Guard guard(in_call_);if(position==std::numeric_limits<std::uint64_t>::max())return fail(f,MovieNativeContextError::overflow,"movie native context position origin overflow");state_.position_origin=position+1;return true;}
bool NativeMoviePlaybackContext::maintenance(MovieServiceMaintenanceGates gates,MovieControlFailure& f) noexcept {if(!enter(f))return false;Guard guard(in_call_);auto* clock=config_.service->service(f);if(!clock)return false;MovieServiceClockFailure why;if(!clock->maintenance(gates,why))return fail(f,MovieNativeContextError::service,"movie native context service maintenance",static_cast<std::int64_t>(why.kind));return true;}
bool NativeMoviePlaybackContext::increment_service(MovieClockRational value,MovieControlFailure& f) noexcept {if(!enter(f))return false;Guard guard(in_call_);auto* clock=config_.service->service(f);if(!clock)return false;MovieServiceClockFailure why;if(!clock->increment(value,why))return fail(f,MovieNativeContextError::service,"movie native context service increment",static_cast<std::int64_t>(why.kind));return true;}
bool NativeMoviePlaybackContext::sample_service(bool admitted,MovieControlFailure& f) noexcept {if(!enter(f))return false;Guard guard(in_call_);auto* clock=config_.service->service(f);if(!clock)return false;MovieServiceClockFailure why;auto read=clock->read(admitted,why);if(!read)return fail(f,MovieNativeContextError::service,"movie native context service read",static_cast<std::int64_t>(why.kind));state_.cached_clock=read->value;route_=read->value?ClockRoute::ordinary:ClockRoute::retained;return true;}
bool NativeMoviePlaybackContext::increment_service_ticks(std::int64_t expected,__int128 ticks,MovieControlFailure& f) noexcept {if(!enter(f))return false;Guard guard(in_call_);auto* clock=config_.service->service(f);if(!clock)return false;MovieServiceClockFailure why;if(!clock->increment_ticks(expected,ticks,why))return fail(f,MovieNativeContextError::service,"movie native context actual native elapsed ticks",static_cast<std::int64_t>(why.kind));return true;}
bool NativeMoviePlaybackContext::sample_audio(MovieClockRational audio,MovieClockRational timer,bool admitted,bool active,MovieControlFailure& f) noexcept {
    if(!enter(f))return false;Guard guard(in_call_);
    if(!admitted){state_.cached_clock.reset();route_=ClockRoute::retained;return true;}
    if(!active){state_.cached_clock=state_.retained_audio_clock;route_=state_.cached_clock?ClockRoute::ordinary:ClockRoute::retained;return true;}
    MovieClockFilterFailure why;auto updated=filter_.update(audio,timer,why);if(!updated)return fail(f,MovieNativeContextError::filter,"movie native context source filter",static_cast<std::int64_t>(why.kind));
    // The absent source sentinel remains a separate initialization rule. Once
    // present, native exact elapsed readings may change rational representation.
    const auto prior=state_.retained_audio_clock;
    if((!prior&&updated->output.numerator>-1)||(prior&&Wide(updated->output.numerator)*prior->denominator>Wide(prior->numerator)*updated->output.denominator))state_.retained_audio_clock=updated->output;
    state_.cached_clock=state_.retained_audio_clock;
    route_=state_.cached_clock?ClockRoute::ordinary:ClockRoute::retained;return true;
}
bool NativeMoviePlaybackContext::select_direct_control(MovieControlFailure& f) noexcept {if(!enter(f))return false;Guard guard(in_call_);route_=ClockRoute::direct;return true;}
bool NativeMoviePlaybackContext::command(MovieNativeCommand value,MovieControlFailure& f) noexcept {
    if(!enter(f))return false;Guard guard(in_call_);
    switch(value){case MovieNativeCommand::owner_start:state_.owner_started=true;break;case MovieNativeCommand::video_start:if(!config_.video_enabled)return fail(f,MovieNativeContextError::invalid_order,"movie native context disabled video command");state_.video_started=true;break;case MovieNativeCommand::audio_start:if(!config_.audio_enabled)return fail(f,MovieNativeContextError::invalid_order,"movie native context disabled audio command");state_.audio_started=true;break;case MovieNativeCommand::video_finish:if(!state_.video_started)return fail(f,MovieNativeContextError::invalid_order,"movie native context video finish before start");state_.video_finished=true;break;case MovieNativeCommand::audio_finish:if(!state_.audio_started)return fail(f,MovieNativeContextError::invalid_order,"movie native context audio finish before start");state_.audio_finished=true;break;case MovieNativeCommand::owner_stop:if(!state_.owner_started)return fail(f,MovieNativeContextError::invalid_order,"movie native context stop acknowledgment before start");state_.stop_acknowledged=true;break;default:return fail(f,MovieNativeContextError::invalid_config,"movie native context unknown source command");}return true;
}
bool NativeMoviePlaybackContext::ready(const MoviePlaybackSnapshot&,bool& out,MovieControlFailure& f) noexcept {out=false;if(!enter(f))return false;Guard guard(in_call_);out=!state_.stop_acknowledged&&state_.owner_started&&(!config_.video_enabled||state_.video_started)&&(!config_.audio_enabled||state_.audio_started);return true;}
std::optional<MoviePolicyDecision> NativeMoviePlaybackContext::decision(const MovieVideoFrame& frame,const MoviePlaybackSnapshot&,MovieControlFailure& f) noexcept {
    if(!enter(f))return {};Guard guard(in_call_);if(!frame.codec_timing.picture_source)return fail(f,MovieNativeContextError::foreign_source,"movie native context frame lacks source"),std::nullopt;
    const auto& selected=*frame.codec_timing.picture_source;auto found=outputs_.find(selected.picture_es_byte);
    if(found==outputs_.end()||found->second.selected!=selected||!frame.source_syntax||frame.source_syntax->identity!=config_.identity||frame.source_syntax->source_key!=config_.request.logical_key||frame.source_syntax->picture_es_byte!=selected.picture_es_byte||frame.source_syntax->pes!=selected.pes||frame.source_syntax->quarter_pts!=selected.quarter_pts)return fail(f,MovieNativeContextError::missing_admission,"movie native context selected output not published"),std::nullopt;
    try{std::optional<MoviePolicyDecision> result;
        if(route_==ClockRoute::direct)result.emplace(MovieSourceDirectDecision{config_.request.logical_key,selected});
        else if(route_==ClockRoute::retained)result.emplace(MovieSourceRetainedDecision{config_.request.logical_key,selected,found->second.target,config_.service});
        else if(route_==ClockRoute::ordinary){if(!state_.cached_clock||!state_.eligibility_rate)return fail(f,MovieNativeContextError::missing_rate,"movie native context latched eligibility rate/clock missing"),std::nullopt;result.emplace(MovieOrdinaryDecision(config_.request.logical_key,selected,*state_.eligibility_rate,*state_.cached_clock,found->second.target));}
        else return fail(f,MovieNativeContextError::missing_clock,"movie native context no source clock disposition"),std::nullopt;
        pending_decision_=selected;return result;
    }catch(const std::bad_alloc&){fail(f,MovieNativeContextError::allocation,"movie native context decision allocation");return {};}
}
bool NativeMoviePlaybackContext::video_submitted(const MovieGpuMetadata& metadata,MovieControlFailure& f) noexcept {if(!enter(f))return false;Guard guard(in_call_);if(!metadata.codec_timing.picture_source||!pending_decision_||*pending_decision_!=*metadata.codec_timing.picture_source)return fail(f,MovieNativeContextError::foreign_source,"movie native context submitted picture mismatch");auto found=outputs_.find(pending_decision_->picture_es_byte);if(found==outputs_.end()||state_.submitted==std::numeric_limits<std::uint64_t>::max())return fail(f,MovieNativeContextError::invalid_order,"movie native context duplicate submission");decoded_.find(pending_decision_->picture_es_byte)->second.submitted=true;outputs_.erase(found);pending_decision_.reset();++state_.submitted;state_.outputs_pending=outputs_.size();return true;}
bool NativeMoviePlaybackContext::audio_accepted(const MoviePlaybackAudioAcceptance& a,MovieControlFailure& f) noexcept {if(!enter(f))return false;Guard guard(in_call_);const Wide source=Wide(a.source_first_frame)+a.packet_client_offset,end=source+a.submission.client_frames,client=Wide(a.submission.first_client_frame)+a.submission.client_frames;
    if(!config_.audio_enabled||a.submission.reset_epoch!=config_.audio_reset_epoch||!a.submission.client_frames||a.packet_client_frames!=a.source_frames||a.packet_client_offset>a.packet_client_frames||a.submission.client_frames>a.packet_client_frames-a.packet_client_offset||source!=state_.next_source_audio_frame||a.submission.first_client_frame!=state_.next_client_frame||end>std::numeric_limits<std::uint64_t>::max()||client>std::numeric_limits<std::uint64_t>::max()||a.submission.client_frames>std::numeric_limits<std::uint64_t>::max()-state_.accepted_audio_frames)return fail(f,MovieNativeContextError::invalid_receipt,"movie native context actual audio acceptance");state_.next_source_audio_frame=static_cast<std::uint64_t>(end);state_.next_client_frame=static_cast<std::uint64_t>(client);state_.accepted_audio_frames+=a.submission.client_frames;return true;}
bool NativeMoviePlaybackContext::end_admitted(const MoviePlaybackSnapshot& s,bool& out,MovieControlFailure& f) noexcept {out=false;if(!enter(f))return false;Guard guard(in_call_);bool complete;
    if(!config_.video_enabled&&!config_.audio_enabled)complete=true;
    else switch(config_.completion){case MovieNativeCompletion::all:complete=state_.video_finished&&state_.audio_finished;break;case MovieNativeCompletion::audio:complete=state_.audio_finished;break;case MovieNativeCompletion::video:complete=state_.video_finished;break;case MovieNativeCompletion::either:complete=state_.video_finished||state_.audio_finished;break;default:return fail(f,MovieNativeContextError::invalid_config,"movie native context completion selector");}
    if(config_.audio_enabled){
        if(!s.audio_queue||!s.source_retirement)return fail(f,MovieNativeContextError::invalid_receipt,"movie native context missing actual audio drain observation");
        const auto& q=*s.audio_queue;const auto& r=*s.source_retirement;
        if(q.reset_epoch!=config_.audio_reset_epoch||r.epoch!=config_.audio_reset_epoch||r.source_identity!=config_.request.logical_key||r.source_rate!=config_.audio_source_rate||q.submitted_frames!=state_.next_client_frame||r.submitted!=q.submitted_frames||r.retired>r.submitted||r.pending!=r.submitted-r.retired||Wide(config_.audio_source_origin)+r.retired!=r.source_boundary)return fail(f,MovieNativeContextError::invalid_receipt,"movie native context foreign audio drain observation");
    }
    const bool drained=s.decoder_eof&&!s.queued_video&&!s.queued_audio&&!s.outstanding_fences&&outputs_.empty()&&(!config_.audio_enabled||(s.audio_queue&&s.source_retirement&&s.audio_queue->reset_epoch==config_.audio_reset_epoch&&s.source_retirement->epoch==config_.audio_reset_epoch&&s.source_retirement->source_identity==config_.request.logical_key&&s.source_retirement->source_rate==config_.audio_source_rate&&!s.audio_queue->padding_frames&&!s.source_retirement->pending));out=complete&&state_.stop_acknowledged&&state_.program_end_observed&&drained;return true;}
bool NativeMoviePlaybackContext::stop(MovieControlFailure& f) noexcept {f={};if(std::this_thread::get_id()!=owner_){f={MovieControlError::wrong_thread,"movie native context stop thread",0};return false;}if(in_call_){f={MovieControlError::invalid_state,"movie native context stop reentry",0};return false;}if(state_.closed)return true;Guard guard(in_call_);outputs_.clear();admissions_.clear();decoded_.clear();prefixes_.clear();source_bytes_.clear();pending_decision_.reset();state_.outputs_pending=0;state_.closed=true;return true;}
std::optional<MovieNativeContextSnapshot> NativeMoviePlaybackContext::snapshot(MovieControlFailure& f) const noexcept {f={};if(std::this_thread::get_id()!=owner_){f={MovieControlError::wrong_thread,"movie native context snapshot thread",0};return {};}if(in_call_){f={MovieControlError::invalid_state,"movie native context snapshot reentry",0};return {};}Guard guard(in_call_);auto* clock=config_.service->service(f);if(!clock)return {};try{auto result=state_;result.service=clock->state();return result;}catch(const std::bad_alloc&){fail(f,MovieNativeContextError::allocation,"movie native context snapshot allocation");return {};}}
} // namespace mka::video
