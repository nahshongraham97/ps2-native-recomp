#include "mka/bank_preparation.hpp"
#include <algorithm>
#include <mutex>
#include <new>
#include <stdexcept>
#include <unordered_set>
#include <thread>
#include <type_traits>
namespace mka::sound {
namespace {
template<class B> std::uint32_t word(const B& b,std::size_t a){return std::uint32_t(b[a])|(std::uint32_t(b[a+1])<<8)|(std::uint32_t(b[a+2])<<16)|(std::uint32_t(b[a+3])<<24);}
std::uint32_t resource_key(const CommandRecord& c){auto k=c.resource_bits();return k==0xffffffffu?0:k;}
struct RestoreLock {
    std::unique_lock<std::mutex>& lock;
    ~RestoreLock() { if (!lock.owns_lock()) lock.lock(); }
};
std::int16_t signed16(std::uint16_t x){return x<=0x7fff?static_cast<std::int16_t>(x):static_cast<std::int16_t>(-1-static_cast<int>(0xffff-x));}
}
struct PreparedBank::State {
    explicit State(SoundBank b):bank(std::move(b)){}
    SoundBank bank;
    std::shared_ptr<AudioResourceDomain> domain;
    ArchiveRecord origin;
    std::shared_ptr<SoundArchiveBacking> backing;
    std::vector<std::vector<CommandRecord>> commands;
    std::vector<std::uint32_t> flags;
    std::vector<std::shared_ptr<const PreparedProgram>> eager;
    struct NormalCache {
        std::shared_ptr<const PreparedNormal> sample;
        bool preserve=false;
    };
    std::vector<std::weak_ptr<NormalCache>> cache;
    // Zero-instance precreation or the durable source flag40 cache reference.
    std::vector<std::shared_ptr<NormalCache>> pinned_cache;
    mutable std::mutex mutex;
    bool unloaded=false;
    ResourceLookup resource(std::uint32_t key) const { return bank.lookup_resource(key); }
    std::shared_ptr<NormalCache> normal(const ResourceLookup& r,
        std::unique_lock<std::mutex>& lock, PreparationFailure& f) {
        RestoreLock restore{lock};
        lock.unlock();
        auto work = domain->acquire(AudioResourceWork::sample, f.resource);
        lock.lock();
        if (!work) { f.kind = PreparationError::resource_access; return {}; }
        if (unloaded) { f.kind = PreparationError::unloaded; return {}; }
        if(auto existing=cache[r.index].lock())return existing;
        auto input = backing;
        lock.unlock();
        auto payload=input->read_payload(r,f.payload);
        if(!payload){f.kind=PreparationError::payload_failed;return {};}
        std::vector<AdpcmMono> pcm;
        std::optional<NormalCount2Encoded> encoded;
        if(payload->plane_count()==1){
            AdpcmHistory history{};auto decoded=decode_adpcm_mono(payload->encoded_payload().data,payload->encoded_payload().size,history,f.codec);
            if(!decoded){f.kind=PreparationError::codec_failed;return {};}
            pcm.push_back(std::move(*decoded));
        }else if(payload->plane_count()==2){
            NormalCount2Head head;std::array<AdpcmHistory,2> histories{};
            auto decoded=decode_normal_count2(*payload,head,histories,f.count2);
            if(!decoded){f.kind=PreparationError::count2_failed;return {};}
            for(auto& plane:decoded->pcm)pcm.push_back(std::move(plane));
            encoded=std::move(decoded->encoded);
        }else{f.kind=PreparationError::unsupported_count;return {};}
        auto sample=std::make_shared<PreparedNormal>(PreparedNormal{r,std::move(*payload),std::move(pcm),std::move(encoded)});
        auto result=std::make_shared<NormalCache>(NormalCache{std::move(sample),false});
        // The expensive construction above is private. Movie exclusion covers
        // actual immutable PCM publication, not arbitrary registry/voice state.
        auto publication = domain->acquire(AudioResourceWork::sample_publication, f.resource);
        lock.lock();
        if (!publication) { f.kind = PreparationError::resource_access; return {}; }
        if (unloaded) { f.kind = PreparationError::unloaded; return {}; }
        if (auto existing = cache[r.index].lock()) return existing;
        cache[r.index]=result;return result;
    }
    std::shared_ptr<const PreparedProgram> program(std::size_t i,
        std::unique_lock<std::mutex>& lock, PreparationFailure& f,
        std::optional<AudioResourceLease>* retained_work=nullptr){
        RestoreLock restore{lock};
        lock.unlock();
        auto work = domain->acquire(AudioResourceWork::program, f.resource);
        lock.lock();
        if (!work) { f.kind = PreparationError::resource_access; return {}; }
        if(retained_work)*retained_work=std::move(work);
        if (unloaded) { f.kind = PreparationError::unloaded; return {}; }
        auto result=std::make_shared<PreparedProgram>();result->program_index=i;result->effective_flags=flags[i];result->commands=commands[i];
        result->normal_instances.reserve(commands[i].size());
        // Partial construction is private: any failure releases ALL prefix instances.
        for(std::size_t n=0;n<commands[i].size();++n){const auto& c=commands[i][n];if(c.kind()!=1)continue;
            f.program_index=i;f.command_index=n;
            auto r=resource(resource_key(c));if(r.status!=ResourceLookupStatus::found){f.kind=PreparationError::absent_resource;return {};}
            if(word(r.bytes,32)&1u){f.kind=PreparationError::unsupported_alternate_preparation;return {};}
            auto sample=normal(r,lock,f);if(!sample)return {};
            if(!(flags[i]&2u))sample->preserve=true; // Sticky source flag40 SUPPRESSES decrement.
            // All vector allocation preceded sample/cache policy mutation. Copies
            // of an immutable occurrence share its native acquisition lifetime.
            result->normal_instances.push_back({n,c,signed16(std::uint16_t(c.bytes[10])|(std::uint16_t(c.bytes[11])<<8)),sample->preserve,sample->sample,sample});
            // Install the actual instance owner before transferring precreation.
            if(sample->preserve)pinned_cache[r.index]=sample;
            else pinned_cache[r.index].reset();
        }
        return result;
    }
    std::optional<std::size_t> selector(std::int32_t selector,PreparationFailure& f)const{
        const auto p=bank.lookup(selector);
        if(p.status!=LookupStatus::found){f.kind=p.status==LookupStatus::absent_selector?PreparationError::absent_selector:PreparationError::absent_program;return std::nullopt;}
        return p.index;
    }
};
struct PreparedProgramWork::Impl {
    std::shared_ptr<PreparedBank::State> bank;
    std::shared_ptr<SoundArchiveBacking> backing;
    std::size_t program_index=0;
    mutable std::mutex mutex;
    ProgramWorkState phase=ProgramWorkState::queued;
    std::thread::id worker;
    bool cancellation_open=false,cancel_requested=false,published=false,retiring=false;
    std::optional<PreparationFailure> failure;
    std::shared_ptr<const PreparedProgram> candidate;
    std::optional<AudioResourceLease> work;
    ~Impl(){
        // Abandonment is cleanup only, with the same actual ownership order.
        candidate.reset();backing.reset();bank.reset();work.reset();
    }
};
PreparedProgramWork::~PreparedProgramWork()=default;
std::shared_ptr<PreparedProgramWork> PreparedBank::admit_deferred_normal(std::int32_t selector,PreparationFailure& f){
    f={};if(!state_){f.kind=PreparationError::unloaded;return {};}
    try {
        std::lock_guard lock(state_->mutex);
        if(state_->unloaded){f.kind=PreparationError::unloaded;return {};}
        auto index=state_->selector(selector,f);if(!index)return {};
        if(state_->eager[*index]){f.kind=PreparationError::already_prepared;return {};}
        if(!state_->backing){f.kind=PreparationError::no_backing;return {};}
        auto work=std::make_shared<PreparedProgramWork::Impl>();
        work->bank=state_;work->backing=state_->backing;work->program_index=*index;
        return std::shared_ptr<PreparedProgramWork>(new PreparedProgramWork(std::move(work)));
    }catch(const std::bad_alloc&){f.kind=PreparationError::allocation_failed;}
    catch(const std::length_error&){f.kind=PreparationError::host_capacity;}
    return {};
}
std::optional<std::vector<SampleCacheObservation>> PreparedBank::sample_cache(PreparationFailure& f)const {
    f={};if(!state_){f.kind=PreparationError::unloaded;return {};}
    try {
        std::lock_guard lock(state_->mutex);
        if(state_->unloaded){f.kind=PreparationError::unloaded;return {};}
        std::vector<SampleCacheObservation> result;result.reserve(state_->cache.size());
        for(std::size_t i=0;i<state_->cache.size();++i){auto cached=state_->cache[i].lock();result.push_back({i,bool(cached),cached&&cached->preserve});}
        return result;
    }catch(const std::bad_alloc&){f.kind=PreparationError::allocation_failed;}
    catch(const std::length_error&){f.kind=PreparationError::host_capacity;}
    return {};
}
bool PreparedProgramWork::build(PreparationFailure& f){
    f={};auto s=state_;
    {std::lock_guard lock(s->mutex);
     if(s->phase==ProgramWorkState::cancelled){f.kind=PreparationError::cancelled;return false;}
     if(s->phase!=ProgramWorkState::queued){f.kind=PreparationError::invalid_work_state;return false;}
     s->worker=std::this_thread::get_id();s->phase=ProgramWorkState::building;s->cancellation_open=true;
    }
    std::shared_ptr<const PreparedProgram> candidate;
    try {
        std::unique_lock bank_lock(s->bank->mutex);
        // Real work starts here, not at queue admission. The program lease is
        // retained through decision, callback/suppression and explicit retirement.
        candidate=s->bank->program(s->program_index,bank_lock,f,&s->work);
    }catch(const std::bad_alloc&){f.kind=PreparationError::allocation_failed;}
    catch(const std::length_error&){f.kind=PreparationError::host_capacity;}
    catch(...){f.kind=PreparationError::native_exception;}
    bool cancelled=false;
    {std::lock_guard lock(s->mutex);
     s->cancellation_open=false;cancelled=s->cancel_requested;
     if(f.kind!=PreparationError::none)s->failure=f;
    }
    if(cancelled){
        // Release this private program's instances only. Source sample cache
        // retention follows each occurrence's actual flag40 policy separately.
        candidate.reset();f.kind=PreparationError::cancelled;
        std::lock_guard lock(s->mutex);s->phase=ProgramWorkState::cancelled;return false;
    }
    std::lock_guard lock(s->mutex);
    if(!candidate){if(f.kind==PreparationError::none)f.kind=PreparationError::invalid_work_state;s->failure=f;s->phase=ProgramWorkState::failed;return false;}
    s->candidate=std::move(candidate);s->phase=ProgramWorkState::ready;return true;
}
ProgramWorkCancellation PreparedProgramWork::cancel(){
    auto s=state_;std::lock_guard lock(s->mutex);
    if(s->phase==ProgramWorkState::queued){s->cancel_requested=true;s->phase=ProgramWorkState::cancelled;return ProgramWorkCancellation::queued_removed;}
    if(s->phase==ProgramWorkState::building&&s->cancellation_open){s->cancel_requested=true;return ProgramWorkCancellation::running_requested;}
    return ProgramWorkCancellation::not_cancellable;
}
std::shared_ptr<const PreparedProgram> PreparedProgramWork::take_result(PreparationFailure& f){
    f={};auto s=state_;std::lock_guard lock(s->mutex);
    if(s->worker!=std::this_thread::get_id()){f.kind=PreparationError::wrong_thread;return {};}
    if(s->phase!=ProgramWorkState::ready||!s->candidate){f.kind=s->phase==ProgramWorkState::cancelled?PreparationError::cancelled:PreparationError::invalid_work_state;return {};}
    std::lock_guard bank_lock(s->bank->mutex);
    if(s->bank->unloaded){f.kind=PreparationError::unloaded;s->failure=f;s->phase=ProgramWorkState::failed;return {};}
    auto result=std::move(s->candidate);s->phase=ProgramWorkState::published;s->published=true;return result;
}
bool PreparedProgramWork::retire(PreparationFailure& f){
    f={};auto s=state_;std::optional<AudioResourceLease> work;
    std::shared_ptr<const PreparedProgram> discarded;
    {std::lock_guard lock(s->mutex);
     if(s->worker!=std::thread::id{}&&s->worker!=std::this_thread::get_id()){f.kind=PreparationError::wrong_thread;return false;}
     if(s->retiring||(s->phase!=ProgramWorkState::published&&s->phase!=ProgramWorkState::cancelled&&s->phase!=ProgramWorkState::failed)){
         f.kind=PreparationError::invalid_work_state;return false;
     }
     s->retiring=true;discarded=std::move(s->candidate);work=std::move(s->work);s->work.reset();
    }
    discarded.reset(); // Actual instance cleanup precedes work-barrier release.
    bool released=true;
    try {if(work)released=work->release(f.resource);}
    catch(...){released=false;f.kind=PreparationError::native_exception;}
    if(!released){
        if(f.kind==PreparationError::none)f.kind=PreparationError::resource_access;
        std::lock_guard lock(s->mutex);s->work=std::move(work);s->retiring=false;return false;
    }
    std::shared_ptr<PreparedBank::State> bank;std::shared_ptr<SoundArchiveBacking> backing;
    {std::lock_guard lock(s->mutex);bank=std::move(s->bank);backing=std::move(s->backing);}
    backing.reset();bank.reset(); // Ticket references retired before completion observation.
    {std::lock_guard lock(s->mutex);s->phase=ProgramWorkState::retired;s->retiring=false;}
    return true;
}
ProgramWorkObservation PreparedProgramWork::observe()const {
    auto s=state_;std::lock_guard lock(s->mutex);
    return {s->phase,s->cancellation_open,s->cancel_requested,s->published,s->failure};
}
static_assert(std::is_nothrow_copy_assignable_v<PreparationFailure>);
std::optional<PreparedBank> PreparedBank::prepare(BankAssetSource source,BankPreparationReport& report,PreparationFailure& failure){
    report={};failure={};
    try {
        if(!source.backing){failure.kind=PreparationError::no_backing;return std::nullopt;}
        auto bank=SoundBank::parse(std::move(source.bytes),failure.bank);
        if(!bank){failure.kind=PreparationError::malformed_bank;return std::nullopt;}
        const auto& bytes=bank->raw_bytes();
        if(!(bytes[4]&0x10)){failure.kind=PreparationError::unsupported_named_keys;return std::nullopt;}
        auto s=std::make_shared<State>(std::move(*bank));s->origin=std::move(source.origin);s->backing=std::move(source.backing);
        s->domain=s->backing->resource_domain();
        std::unique_lock<std::mutex> lock(s->mutex);
        const auto& raw=s->bank.raw_bytes();const auto programs=s->bank.program_count(),resources=s->bank.resource_count();
        s->commands.resize(programs);s->flags.resize(programs);s->eager.resize(programs);s->cache.resize(resources);s->pinned_cache.resize(resources);report.programs.resize(programs);
        const auto po=word(raw,20),ao=word(raw,24),ro=word(raw,16);std::size_t cursor=word(raw,28);
        // SoundBank already validated these exact table extents/counts, including
        // programs with no selector alias. Never confuse selector count with programs.
        for(std::size_t i=0;i<programs;++i){
            s->flags[i]=word(raw,po+i*16+12);report.programs[i].serialized_flags=s->flags[i];
            if(s->flags[i]&2u)report.resource_precreation_skipped=true;
            const auto n=word(raw,ao+i*16+8);s->commands[i].resize(n);
            for(auto& c:s->commands[i]){std::copy_n(raw.begin()+cursor,40,c.bytes.begin());cursor+=40;}
        }
        std::unordered_set<std::uint32_t> keys;
        for(std::size_t i=0;i<resources;++i)if(!keys.insert(word(raw,ro+i*36+8)).second){failure.kind=PreparationError::duplicate_resource_key;return std::nullopt;}
        if(!report.resource_precreation_skipped){
            for(std::size_t i=0;i<resources;++i){
                if(word(raw,ro+i*36+32)&1u)continue;
                ++report.resource_precreation_attempts;PreparationFailure attempt;
                auto resource=s->resource(word(raw,ro+i*36+8));
                std::shared_ptr<State::NormalCache> object;
                try{object=s->normal(resource,lock,attempt);}
                catch(const std::bad_alloc&){attempt.kind=PreparationError::allocation_failed;}
                catch(const std::length_error&){attempt.kind=PreparationError::host_capacity;}
                if(object)s->pinned_cache[i]=std::move(object);else ++report.resource_precreation_failures;
            }
        }
        for(std::size_t i=0;i<programs;++i){
            auto& observation=report.programs[i];
            for(const auto& c:s->commands[i])if(c.kind()==1){auto r=s->resource(resource_key(c));if(r.status==ResourceLookupStatus::found&&(word(r.bytes,32)&1u)){s->flags[i]|=2u;break;}}
            observation.effective_flags=s->flags[i];
            if(s->flags[i]&2u){observation.state=ProgramPreparationState::source_deferred;continue;}
            PreparationFailure f;
            try{s->eager[i]=s->program(i,lock,f);}
            catch(const std::bad_alloc&){f.kind=PreparationError::allocation_failed;}
            catch(const std::length_error&){f.kind=PreparationError::host_capacity;}
            if(s->eager[i])observation.state=ProgramPreparationState::eager_prepared;
            else{observation.state=ProgramPreparationState::failed;observation.failure=f;++report.eager_error_count;}
        }
        if(report.eager_error_count){failure.kind=PreparationError::eager_program_failures;return std::nullopt;}
        return PreparedBank(std::move(s));
    }catch(const std::bad_alloc&){failure.kind=PreparationError::allocation_failed;}
     catch(const std::length_error&){failure.kind=PreparationError::host_capacity;}
    return std::nullopt;
}
std::shared_ptr<const PreparedProgram> PreparedBank::eager_program(std::int32_t selector,PreparationFailure& failure)const{
    failure={};if(!state_){failure.kind=PreparationError::unloaded;return {};}
    std::unique_lock<std::mutex> lock(state_->mutex);if(state_->unloaded){failure.kind=PreparationError::unloaded;return {};}
    auto i=state_->selector(selector,failure);if(!i)return {};
    // A source-deferred program has no eager state. This is NOT a successful empty program.
    if(!state_->eager[*i]){failure.kind=PreparationError::source_deferred;return {};}
    return state_->eager[*i];
}
std::shared_ptr<const PreparedProgram> PreparedBank::prepare_deferred_normal(std::int32_t selector,PreparationFailure& failure){
    auto eager=eager_program(selector,failure);if(eager)return eager;
    if(failure.kind!=PreparationError::source_deferred)return {};
    auto work=admit_deferred_normal(selector,failure);if(!work)return {};
    std::shared_ptr<const PreparedProgram> result;
    if(work->build(failure))result=work->take_result(failure);
    PreparationFailure cleanup;
    if(!work->retire(cleanup)){
        if(failure.kind==PreparationError::none)failure=cleanup;
        else {failure.cleanup_kind=cleanup.kind;failure.cleanup_resource=cleanup.resource;}
        return {};
    }
    return result;
}
bool PreparedBank::active()const{
    if(!state_)return false;
    std::lock_guard<std::mutex> lock(state_->mutex);return !state_->unloaded;
}
void PreparedBank::unload(){
    if(!state_)return;
    std::lock_guard<std::mutex> lock(state_->mutex);state_->unloaded=true;
    state_->eager.clear();state_->pinned_cache.clear();state_->cache.clear();state_->backing.reset();
}
} // namespace mka::sound
