#include "mka/bank_preparation.hpp"
#include <algorithm>
#include <mutex>
#include <new>
#include <stdexcept>
#include <unordered_set>
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
    std::vector<std::weak_ptr<const PreparedNormal>> cache;
    std::vector<bool> release_flag;
    std::vector<std::shared_ptr<const PreparedNormal>> unused_precreated;
    mutable std::mutex mutex;
    bool unloaded=false;
    ResourceLookup resource(std::uint32_t key) const { return bank.lookup_resource(key); }
    std::shared_ptr<const PreparedNormal> normal(const ResourceLookup& r,
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
        auto result=std::make_shared<PreparedNormal>(PreparedNormal{r,std::move(*payload),std::move(pcm),std::move(encoded)});
        // The expensive construction above is private. Movie exclusion covers
        // actual immutable PCM publication, not arbitrary registry/voice state.
        auto publication = domain->acquire(AudioResourceWork::sample_publication, f.resource);
        lock.lock();
        if (!publication) { f.kind = PreparationError::resource_access; return {}; }
        if (unloaded) { f.kind = PreparationError::unloaded; return {}; }
        if (auto existing = cache[r.index].lock()) return existing;
        release_flag[r.index]=false;cache[r.index]=result;return result;
    }
    std::shared_ptr<const PreparedProgram> program(std::size_t i,
        std::unique_lock<std::mutex>& lock, PreparationFailure& f){
        RestoreLock restore{lock};
        lock.unlock();
        auto work = domain->acquire(AudioResourceWork::program, f.resource);
        lock.lock();
        if (!work) { f.kind = PreparationError::resource_access; return {}; }
        if (unloaded) { f.kind = PreparationError::unloaded; return {}; }
        auto result=std::make_shared<PreparedProgram>();result->program_index=i;result->effective_flags=flags[i];result->commands=commands[i];
        // Partial construction is private: any failure releases ALL prefix instances.
        for(std::size_t n=0;n<commands[i].size();++n){const auto& c=commands[i][n];if(c.kind()!=1)continue;
            f.program_index=i;f.command_index=n;
            auto r=resource(resource_key(c));if(r.status!=ResourceLookupStatus::found){f.kind=PreparationError::absent_resource;return {};}
            if(word(r.bytes,32)&1u){f.kind=PreparationError::unsupported_alternate_preparation;return {};}
            auto sample=normal(r,lock,f);if(!sample)return {};
            if(!(flags[i]&2u))release_flag[r.index]=true; // Cached flag40 is sticky while object lives.
            result->normal_instances.push_back({n,c,signed16(std::uint16_t(c.bytes[10])|(std::uint16_t(c.bytes[11])<<8)),release_flag[r.index],std::move(sample)});
            // Transfer the zero-instance precreation ownership into actual instances.
            // Weak cache then disappears automatically if a failed prefix had last use.
            if(release_flag[r.index])unused_precreated[r.index].reset();
            else unused_precreated[r.index]=result->normal_instances.back().sample;
        }
        return result;
    }
    std::optional<std::size_t> selector(std::int32_t selector,PreparationFailure& f)const{
        const auto p=bank.lookup(selector);
        if(p.status!=LookupStatus::found){f.kind=p.status==LookupStatus::absent_selector?PreparationError::absent_selector:PreparationError::absent_program;return std::nullopt;}
        return p.index;
    }
};
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
        s->commands.resize(programs);s->flags.resize(programs);s->eager.resize(programs);s->cache.resize(resources);s->release_flag.resize(resources);s->unused_precreated.resize(resources);report.programs.resize(programs);
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
                std::shared_ptr<const PreparedNormal> object;
                try{object=s->normal(resource,lock,attempt);}
                catch(const std::bad_alloc&){attempt.kind=PreparationError::allocation_failed;}
                catch(const std::length_error&){attempt.kind=PreparationError::host_capacity;}
                if(object)s->unused_precreated[i]=std::move(object);else ++report.resource_precreation_failures;
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
    failure={};if(!state_){failure.kind=PreparationError::unloaded;return {};}
    std::unique_lock<std::mutex> lock(state_->mutex);if(state_->unloaded){failure.kind=PreparationError::unloaded;return {};}
    auto i=state_->selector(selector,failure);if(!i)return {};
    if(state_->eager[*i])return state_->eager[*i];
    try{return state_->program(*i,lock,failure);}
    catch(const std::bad_alloc&){failure.kind=PreparationError::allocation_failed;}
    catch(const std::length_error&){failure.kind=PreparationError::host_capacity;}
    return {};
}
bool PreparedBank::active()const{
    if(!state_)return false;
    std::lock_guard<std::mutex> lock(state_->mutex);return !state_->unloaded;
}
void PreparedBank::unload(){
    if(!state_)return;
    std::lock_guard<std::mutex> lock(state_->mutex);state_->unloaded=true;
    state_->eager.clear();state_->unused_precreated.clear();state_->cache.clear();state_->backing.reset();
}
} // namespace mka::sound
