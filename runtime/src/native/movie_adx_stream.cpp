#include "mka/movie_adx_stream.hpp"
#include <algorithm>
#include <exception>
#include <limits>
#include <new>
#include <stdexcept>
#include <type_traits>
namespace mka::video {
namespace {
bool fail(MovieAdxStreamFailure& f,MovieAdxStreamError kind,const char* op) noexcept {f.kind=kind;f.operation=op;return false;}
MovieAdxStreamResult rejected() noexcept {MovieAdxStreamResult r;r.status=MovieAdxStreamStatus::error;return r;}
std::uint16_t be16(const std::uint8_t* p) noexcept{return std::uint16_t((std::uint16_t(p[0])<<8)|p[1]);}
}
struct NativeMovieAdxStream::Guard {
    NativeMovieAdxStream& owner;bool entered=false;
    Guard(NativeMovieAdxStream& o,MovieAdxStreamFailure& f):owner(o){
        f={};
        if(std::this_thread::get_id()!=o.owner_){fail(f,MovieAdxStreamError::wrong_thread,"ADX stream creating thread");return;}
        if(o.busy_){fail(f,MovieAdxStreamError::reentrant,"ADX stream non-reentrant operation");return;}
        o.busy_=true;entered=true;
    }
    ~Guard(){if(entered)owner.busy_=false;}
    explicit operator bool()const noexcept{return entered;}
};
NativeMovieAdxStream::NativeMovieAdxStream(std::unique_ptr<MovieAdxProfileResolver> p):resolver_(std::move(p)){}
NativeMovieAdxStream::~NativeMovieAdxStream(){if(owner_!=std::this_thread::get_id()||busy_)std::terminate();}
std::unique_ptr<NativeMovieAdxStream> NativeMovieAdxStream::create(std::vector<std::uint8_t> extra,
        std::unique_ptr<MovieAdxProfileResolver> resolver,MovieAdxStreamFailure& f) noexcept {
    f={};if(!resolver){fail(f,MovieAdxStreamError::profile,"ADX stream requires owned explicit profile resolver");return {};}
    try {
        auto owner=std::unique_ptr<NativeMovieAdxStream>(new NativeMovieAdxStream(std::move(resolver)));
        Guard guard(*owner,f);if(!guard)return {};
        if(!owner->initialize(extra,f))return {};
        return owner;
    }catch(const std::bad_alloc&){fail(f,MovieAdxStreamError::allocation,"ADX stream factory allocation");return {};}
    catch(const std::length_error&){fail(f,MovieAdxStreamError::host_capacity,"ADX stream factory extent");return {};}
    catch(...){fail(f,MovieAdxStreamError::resolver,"ADX stream resolver exception");return {};}
}
bool NativeMovieAdxStream::initialize(std::span<const std::uint8_t> extra,MovieAdxStreamFailure& f){
    if(extra.empty())return true;
    auto header=parse_movie_adx_header(extra,f.header);
    if(!header)return fail(f,MovieAdxStreamError::header,"ADX exact extradata header required");
    if(header->data_start!=extra.size())return fail(f,MovieAdxStreamError::invalid_input,"ADX extradata must contain header only");
    return admit_profile(state_,std::move(*header),f);
}
bool NativeMovieAdxStream::admit_profile(State& state,MovieAdxHeader header,MovieAdxStreamFailure& f){
    auto profile=resolver_->resolve(header,f);
    if(!profile||f.kind!=MovieAdxStreamError::none){if(f.kind==MovieAdxStreamError::none)fail(f,MovieAdxStreamError::profile,"ADX explicit resolver returned no profile");return false;}
    if(profile->mode!=AdxBlockMode::version4_flags0||profile->coefficients.size()!=2||profile->histories.size()!=2||
       !std::equal(profile->histories.begin(),profile->histories.end(),header.histories.begin()))
        return fail(f,MovieAdxStreamError::profile,"ADX resolver mode/count/header-history identity mismatch");
    state.header=std::move(header);state.profile=std::move(profile);return true;
}
bool NativeMovieAdxStream::process(State& s,MovieAdxStreamResult& out,MovieAdxStreamFailure& f){
    if(!s.header){
        auto header=parse_movie_adx_header(s.carry,f.header);
        if(!header){
            if(f.header.kind==MovieAdxHeaderError::incomplete){f={};return true;}
            return fail(f,MovieAdxStreamError::header,"ADX first source header invalid");
        }
        const auto consumed=static_cast<std::size_t>(header->data_start);
        if(!admit_profile(s,std::move(*header),f))return false;
        out.new_header=s.header;
        s.carry.erase(s.carry.begin(),s.carry.begin()+consumed);
    }
    out.header_available=true;
    const auto declared=s.header->declared_samples;
    if(s.published<declared){
        const auto needed=(declared-s.published+31)/32;
        const auto available=s.carry.size()/36;
        const auto groups=std::min<std::uint64_t>(needed,available);
        // Scale positions only: never search arbitrary residual/ES bytes.
        const auto scale_prefixes=s.carry.size()<2?0:(s.carry.size()-2)/18+1;
        const auto inspect=std::min<std::uint64_t>(scale_prefixes,needed*2);
        for(std::uint64_t i=0;i<inspect;++i){
            const auto prefix=be16(s.carry.data()+static_cast<std::size_t>(i)*18);
            if(prefix==0x8000)return fail(f,MovieAdxStreamError::duplicate_header,"ADX duplicate header at encoded block boundary");
            if(prefix==0x8001)return fail(f,MovieAdxStreamError::premature_marker,"ADX marker before declared samples decoded");
            if(prefix&0x8000)return fail(f,MovieAdxStreamError::invalid_marker,"ADX unsupported scale sign-bit control");
        }
        // Also classify a leading marker split before a complete first block.
        if(!groups&&s.carry.size()>=2){
            const auto prefix=be16(s.carry.data());
            if(prefix==0x8000)return fail(f,MovieAdxStreamError::duplicate_header,"ADX repeated source header");
            if(prefix==0x8001)return fail(f,MovieAdxStreamError::premature_marker,"ADX early marker prefix");
            if(prefix&0x8000)return fail(f,MovieAdxStreamError::invalid_marker,"ADX unsupported control prefix");
        }
        if(groups){
            const auto bytes=static_cast<std::size_t>(groups)*36;
            auto decoded=decode_movie_adx_blocks(std::span<const std::uint8_t>(s.carry).first(bytes),*s.profile,f.block);
            if(!decoded)return fail(f,MovieAdxStreamError::decode,"ADX complete group decode failed");
            const auto publish=std::min<std::uint64_t>(decoded->frames,declared-s.published);
            if(s.decoded>UINT64_MAX-decoded->frames||s.published>UINT64_MAX-publish)
                return fail(f,MovieAdxStreamError::overflow,"ADX native decoded/published counter");
            MovieAdxStreamFrame frame;frame.first_source_frame=s.published;frame.sample_rate=s.header->sample_rate;
            frame.raw_decoded_frames=decoded->frames;frame.planes.resize(2);
            for(std::size_t i=0;i<2;++i)frame.planes[i].assign(decoded->planes[i].begin(),decoded->planes[i].begin()+static_cast<std::size_t>(publish));
            out.frame=std::move(frame);
            s.profile->histories=std::move(decoded->final_histories);
            s.decoded+=decoded->frames;s.published+=publish;
            s.carry.erase(s.carry.begin(),s.carry.begin()+bytes);
        }
    }
    if(s.published==declared&&!s.carry.empty()){
        if(s.carry[0]!=0x80)return fail(f,MovieAdxStreamError::trailing_data,"ADX extra encoded data after declared samples");
        if(s.carry.size()>=2){
            const auto prefix=be16(s.carry.data());
            if(prefix==0x8000)return fail(f,MovieAdxStreamError::duplicate_header,"ADX header after declared samples");
            if(prefix!=0x8001)return fail(f,MovieAdxStreamError::invalid_marker,"ADX only8001 end marker admitted");
            if(s.carry.size()>=4){
                const std::uint64_t extent=std::uint64_t(be16(s.carry.data()+2))+4;
                if(extent<16)return fail(f,MovieAdxStreamError::invalid_marker,"ADX marker shorter than source parser minimum");
                if(s.carry.size()>extent)return fail(f,MovieAdxStreamError::trailing_data,"ADX bytes after complete end marker");
                if(s.carry.size()==extent){s.marker=std::move(s.carry);s.carry.clear();s.ended=true;out.completed_marker=s.marker;}
            }
        }
    }
    out.encoded_ended=s.ended;
    out.status=out.frame?MovieAdxStreamStatus::frame:s.ended?MovieAdxStreamStatus::ended:MovieAdxStreamStatus::need_more;
    return true;
}
MovieAdxStreamResult NativeMovieAdxStream::terminal(MovieAdxStreamFailure& f) noexcept {
    if(f.kind==MovieAdxStreamError::none)fail(f,MovieAdxStreamError::invalid_input,"ADX stream unspecified failure");
    terminal_=f;return rejected();
}
MovieAdxStreamResult NativeMovieAdxStream::feed(std::span<const std::uint8_t> bytes,MovieAdxStreamFailure& f) noexcept {
    Guard guard(*this,f);if(!guard)return rejected();
    if(terminal_.kind!=MovieAdxStreamError::none){f=terminal_;f.original_kind=f.kind;f.kind=MovieAdxStreamError::failed_owner;f.operation="ADX stream terminal owner";return rejected();}
    try {
        if(!bytes.empty()&&!bytes.data()){fail(f,MovieAdxStreamError::invalid_input,"ADX packet missing bytes");return terminal(f);}
        if(state_.ended){
            if(!bytes.empty()){fail(f,MovieAdxStreamError::trailing_data,"ADX packet after accepted marker");return terminal(f);}
            MovieAdxStreamResult out;out.header_available=true;out.encoded_ended=true;out.status=MovieAdxStreamStatus::ended;return out;
        }
        State staged=state_;
        if(bytes.size()>staged.carry.max_size()-staged.carry.size()){fail(f,MovieAdxStreamError::host_capacity,"ADX staged packet extent");return terminal(f);}
        if(!bytes.empty())staged.carry.insert(staged.carry.end(),bytes.begin(),bytes.end());
        MovieAdxStreamResult out;
        if(!process(staged,out,f))return terminal(f);
        static_assert(std::is_nothrow_move_assignable_v<State>);
        state_=std::move(staged);return out;
    }catch(const std::bad_alloc&){fail(f,MovieAdxStreamError::allocation,"ADX transactional packet allocation");return terminal(f);}
    catch(const std::length_error&){fail(f,MovieAdxStreamError::host_capacity,"ADX transactional packet vector extent");return terminal(f);}
    catch(...){fail(f,MovieAdxStreamError::resolver,"ADX transactional resolver exception");return terminal(f);}
}
MovieAdxStreamResult NativeMovieAdxStream::finish(MovieAdxStreamFailure& f) noexcept {
    Guard guard(*this,f);if(!guard)return rejected();
    if(terminal_.kind!=MovieAdxStreamError::none){f=terminal_;f.original_kind=f.kind;f.kind=MovieAdxStreamError::failed_owner;return rejected();}
    if(!state_.header||!state_.ended||!state_.carry.empty()||state_.published!=state_.header->declared_samples){
        fail(f,MovieAdxStreamError::incomplete,"ADX finish requires exact samples and complete accepted marker");return terminal(f);
    }
    MovieAdxStreamResult out;out.status=MovieAdxStreamStatus::ended;out.header_available=true;out.encoded_ended=true;return out;
}
std::optional<MovieAdxStreamSnapshot> NativeMovieAdxStream::snapshot(MovieAdxStreamFailure& f) noexcept {
    Guard guard(*this,f);if(!guard)return {};
    try {
        MovieAdxStreamSnapshot out;out.header=state_.header;
        if(state_.profile)out.histories=state_.profile->histories;
        out.carry=state_.carry;out.marker=state_.marker;out.raw_decoded_frames=state_.decoded;out.published_frames=state_.published;
        out.encoded_ended=state_.ended;out.failed=terminal_.kind!=MovieAdxStreamError::none;return out;
    }catch(const std::bad_alloc&){fail(f,MovieAdxStreamError::allocation,"ADX owned snapshot allocation");return {};}
    catch(const std::length_error&){fail(f,MovieAdxStreamError::host_capacity,"ADX snapshot extent");return {};}
}
} // namespace mka::video
