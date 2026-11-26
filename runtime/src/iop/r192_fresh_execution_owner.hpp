#pragma once
// Reference-v8 execution connector. Fresh raw-image provenance is distinct
// from restored code observations, native HLE bodies and SDK registry history.
#include "iop/r192_loader_code_owner.hpp"

namespace ps2::iop::fresh_execution {
namespace lo = loader_owner;
namespace aot = r192_aot;
namespace md = metadata_guard;
enum class Mode { fresh, restoring, restored_legacy_unproved, poisoned };
struct Token { int id{}; std::uint64_t epoch{}, generation{}; };

class Owners {
public:
    using Read = md::Guard::Read;
    using Fact = lo::Session::Fact;
    using List = lo::Session::List;
    using Libraries = std::function<std::uint32_t(const std::string &)>;
    Owners(std::span<const lo::Source> sources, Read read, aot::Services legacy,
           Fact facts, List list, Libraries libraries)
        : sources_(sources.begin(),sources.end()), read_(std::move(read)), legacy_(legacy),
          facts_(std::move(facts)), list_(std::move(list)), libraries_(std::move(libraries)) {}
    Mode mode() const { return mode_; }
    std::size_t count() const { ready(); return owners_.size(); }
    std::uint64_t validated_entries() const { return entry_checks_; }
    void reset() {
        bump_epoch(); owners_.clear(); retired_.clear(); linking_=false; mode_=Mode::fresh;
    }
    // Must be called BEFORE the first byte of progressive machine restoration.
    // Successful restore deliberately retains the old route without claiming
    // fresh source ownership, SDK roots, registration or allocation history.
    void before_restore() {
        ready(); bump_epoch(); owners_.clear(); retired_.clear(); linking_=false; mode_=Mode::restoring;
    }
    void after_restore(bool success) {
        if (mode_!=Mode::restoring) fail("restore_protocol_mismatch");
        mode_=success ? Mode::restored_legacy_unproved : Mode::poisoned;
    }
    void poison() noexcept { mode_=Mode::poisoned; }
    Token load(const std::vector<std::uint8_t>&bytes,const std::string&path,
               const std::string&args,const lo::Session::Load&actual) {
        fresh(); const auto digest=lo::image_sha256(bytes); const lo::Source *source{};
        for(const auto&s:sources_) if(s.digest==digest) {
            if(source) fail("ambiguous_original_digest"); source=&s;
        }
        if(!source) fail("unknown_original_before_loader");
        auto owner=std::make_unique<Owner>(); owner->source=source;
        int loaded{};
        auto loader=[&](const auto&b,const auto&p,const auto&a){ loaded=actual(b,p,a);return loaded; };
        // Reuse the published actual-loader SHA/PIC/schema admission, with a
        // private guard per lease so a source-proved free can destroy it.
        owner->admission=std::make_unique<lo::Session>(std::span<const lo::Source>(source,1),read_,services(),loader,
            facts_,[this]{return live_facts();});
        try {
            owner->admission_token=owner->admission->load(bytes,path,args);
            owner->facts=facts_(loaded); exclusive(owner->facts);
            if(owners_.count(loaded)) fail("loaded_id_reused_without_reset");
            if(!owner->bound.bind(*source->code,owner->facts.base)) fail("PIC_rebind_failed");
            owner->generation=next_generation();
            owner->guard=guard(*owner);
            const Token token{loaded,epoch_,owner->generation}; owners_.emplace(loaded,std::move(owner));return token;
        } catch(...) { poison();throw; }
    }
    Token token(int id) const {
        fresh();auto it=owners_.find(id);if(it==owners_.end())fail("no_fresh_loaded_owner");
        return {id,epoch_,it->second->generation};
    }
    const aot::BoundPic&enter(Token token,std::uint32_t pc) {
        fresh(); if(linking_)fail("execution_during_loader_link_transition");
        auto&owner=get(token);
        try {
            exclusive(owner.facts);
            if(!owner.guard->permits_entry(owner.guard_token,pc))fail("metadata_or_data_PC_not_original_code");
            ++entry_checks_;return owner.bound;
        } catch(...) {poison();throw;}
    }
    const aot::BoundPic&enter(std::uint32_t pc) {
        fresh(); const auto address=pc&0x1fffffffu; Owner *found{};int id{};
        for(auto&[key,o]:owners_) if(o->facts.base<=address && address-o->facts.base<o->facts.size) {
            if(found)fail("ambiguous_fresh_code_owner");found=o.get();id=key;
        }
        if(!found)fail("execution_without_fresh_code_provenance");
        for(const auto&[old_id,old]:retired_)
            if(std::uint64_t(old.base)<std::uint64_t(found->facts.base)+found->facts.size &&
               std::uint64_t(found->facts.base)<std::uint64_t(old.base)+old.size)
                fail("reused_code_range_requires_generation_bound_entry");
        return enter({id,epoch_,found->generation},pc);
    }
    // Only the source-pinned successful g_used allocation erasure may call
    // this, with the actual erased range. A no-op free must not revoke owners.
    void allocation_freed(std::uint32_t base,std::uint32_t size,bool actually_erased) {
        ready(); if(!actually_erased || !size)return;
        base&=0x1fffffffu;
        if(base>=kRamSize || size>kRamSize-base)fail("freed_allocation_bounds");
        for(auto it=owners_.begin();it!=owners_.end();) {
            const auto&f=it->second->facts;
            if(std::uint64_t(base)<std::uint64_t(f.base)+f.size && std::uint64_t(f.base)<std::uint64_t(base)+size) {
                retired_[f.id]=f;it=owners_.erase(it);
            } else ++it;
        }
    }
    // Source-v8 loader relinks exactly declared import pairs. This does NOT
    // authorize mutable SDK registry/export metadata, registration or release.
    void before_original_link_imports() {
        fresh();if(linking_)fail("nested_loader_link_transition");
        try {
            for(const auto&[id,o]:owners_) { exclusive(o->facts);o->guard->verify(); }
            linking_=true;
        } catch(...) {poison();throw;}
    }
    void after_original_link_imports() {
        fresh();if(!linking_)fail("missing_loader_link_transition");
        try {
            std::vector<std::pair<Owner*,std::unique_ptr<md::Guard>>> replacement;
            replacement.reserve(owners_.size());
            for(auto&[id,o]:owners_) { exclusive(o->facts);replacement.emplace_back(o.get(),guard(*o)); }
            for(auto&x:replacement)x.first->guard=std::move(x.second);
            linking_=false;
        } catch(...) {poison();throw;}
    }
    bool import_identity(std::uint32_t address,const aot::Import&entry) const {
        if(!legacy_.validate_import || !legacy_.validate_import(legacy_.user,address,entry))return false;
        const auto first=read_(address,4),second=read_(address+4,4);
        if(first>>26!=2)return true; // Actual native-HLE identity, no SDK-root claim.
        if(second || !libraries_)return false;
        const auto table=libraries_(entry.library);
        for(const auto&[id,o]:owners_)for(const auto&r:o->source->records) {
            if(r.kind!=md::Kind::exports || o->facts.base+r.offset!=table || r.words.size()<7 || entry.ordinal>=r.words.size()-6)continue;
            std::string name;
            for(unsigned i=0;i<8;++i){auto ch=static_cast<char>(r.words[3+i/4]>>(8*(i%4)));if(!ch)break;name+=ch;}
            if(name!=entry.library)continue;
            if(!same(facts_(id),o->facts))return false;
            for(const auto&other:live_facts())
                if(other.id!=id && other.size && std::uint64_t(other.base)<std::uint64_t(o->facts.base)+o->facts.size &&
                   std::uint64_t(o->facts.base)<std::uint64_t(other.base)+other.size)return false;
            // Current original export metadata is authority only after exact
            // canonical/relocation checks, never because a host map names it.
            for(std::size_t i=0;i<r.words.size();++i){
                auto expected=r.words[i];
                if(std::find(r.relocations.begin(),r.relocations.end(),static_cast<std::uint32_t>(i*4))!=r.relocations.end())expected+=o->facts.base;
                if(read_(table+static_cast<std::uint32_t>(i*4),4)!=expected)return false;
            }
            const auto site=20u+4u*entry.ordinal;
            if(std::count(r.relocations.begin(),r.relocations.end(),site)!=1)return false;
            const auto target=o->facts.base+r.words[5+entry.ordinal];
            return first==(0x08000000u|((target>>2)&0x03ffffffu));
        }
        return false; // Restored host map or retired/provider-less export is not authority.
    }
private:
    struct Owner {
        const lo::Source*source{};lo::Facts facts;std::uint64_t generation{};
        std::unique_ptr<lo::Session> admission;lo::OwnerHandle admission_token;
        aot::BoundPic bound;std::unique_ptr<md::Guard> guard;md::OwnerHandle guard_token;
    };
    std::vector<lo::Source>sources_;Read read_;aot::Services legacy_;Fact facts_;List list_;Libraries libraries_;
    std::map<int,std::unique_ptr<Owner>>owners_;std::map<int,lo::Facts>retired_;
    Mode mode_{Mode::fresh};std::uint64_t epoch_{1},generation_{},entry_checks_{};bool linking_{};
    [[noreturn]]static void fail(const char*s){throw std::runtime_error(s);}
    void ready()const{if(mode_==Mode::poisoned)fail("fresh_execution_session_poisoned");}
    void fresh()const{ready();if(mode_!=Mode::fresh)fail("restored_or_restoring_not_fresh_owner");}
    void bump_epoch(){if(epoch_==UINT64_MAX)fail("owner_epoch_exhausted");++epoch_;}
    std::uint64_t next_generation(){if(generation_==UINT64_MAX)fail("owner_generation_exhausted");return ++generation_;}
    static bool same(const lo::Facts&a,const lo::Facts&b){return a.id==b.id && a.name==b.name && a.base==b.base && a.size==b.size && a.entry==b.entry && a.gp==b.gp;}
    std::vector<lo::Facts>live_facts()const{
        auto facts=list_();facts.erase(std::remove_if(facts.begin(),facts.end(),[this](const auto&f){auto it=retired_.find(f.id);return it!=retired_.end()&&same(f,it->second);}),facts.end());return facts;
    }
    void exclusive(const lo::Facts&f)const{
        const auto now=facts_(f.id);if(!same(now,f))fail("fresh_loaded_facts_changed");std::size_t count{};
        for(const auto&o:live_facts()){
            if(o.id==f.id){if(!same(o,f))fail("live_fact_disagreement");++count;continue;}
            if(o.size && std::uint64_t(o.base)<std::uint64_t(f.base)+f.size && std::uint64_t(f.base)<std::uint64_t(o.base)+o.size)fail("overlapping_live_original_modules");
        }
        if(count!=1)fail("fresh_owner_missing_from_live_facts");
    }
    Owner&get(Token t){if(t.epoch!=epoch_)fail("stale_fresh_owner_epoch");auto it=owners_.find(t.id);if(it==owners_.end()||it->second->generation!=t.generation)fail("stale_fresh_owner_generation");return *it->second;}
    aot::Services services(){return {this,[](void*u,std::uint32_t a,const aot::Import&i){return static_cast<Owners*>(u)->import_identity(a,i);},nullptr};}
    std::unique_ptr<md::Guard>guard(Owner&o){
        auto result=std::make_unique<md::Guard>(read_);o.guard_token=result->bind_owner(o.bound.module,services(),o.source->records.size());
        for(const auto&r:o.source->records){
            if(r.kind==md::Kind::registry_root)fail("invented_SDK_root_forbidden");auto words=r.words;
            for(auto at:r.relocations){if((at&3u)||at/4>=words.size())fail("record_relocation_bounds");words[at/4]+=o.facts.base;}
            std::vector<std::uint8_t>bytes;bytes.reserve(words.size()*4);
            for(auto w:words)for(unsigned i=0;i<4;++i)bytes.push_back(static_cast<std::uint8_t>(w>>(8*i)));
            result->bind_record(o.guard_token,r.kind,r.offset,std::move(bytes));
        }
        result->seal_owner(o.guard_token);return result;
    }
};
} // namespace ps2::iop::fresh_execution
