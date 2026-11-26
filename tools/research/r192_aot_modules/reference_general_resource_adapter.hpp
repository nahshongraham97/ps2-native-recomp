#pragma once
// Descriptor-derived reference adapter. Whole source packing equations admit
// supported parameterized bodies without an observed-shape table. Bounds and
// source memory/ABI limitations remain explicit; this is not final native credit.
#include "resource_pipeline_events.hpp"
#include "reference_resource_layout.hpp"
#include "reference_resource_word_equality.hpp"
#include "reference_resource_owner_epochs.hpp"
#include "ps2/system.hpp"
#include "ps2/memory.hpp"
#include "kernel.hpp"
#include <algorithm>
#include <compare>
#include <cstdio>
#include <limits>
#include <map>
#include <memory>
#include <vector>

namespace mka::emitted::general_resource_reference {
using namespace reference_resources;
struct Shape {unsigned active_words{},data_words{},total_words{};};
struct Contract {std::array<Bits,21>caller;};
struct Range{unsigned begin,end;};
class Adapter {
    struct Key{std::uint64_t epoch,stack;int thread;unsigned object,base;auto operator<=>(const Key&)const=default;};
    struct Context {
        Shape shape;Descriptor logical,declared;std::shared_ptr<const resource_owner_epochs::SourceProgram>source_program;Prepared prepared;Layout layout;Streams streams;ps2::Cpu entry,parked;
        unsigned owner,attributes,cache_identity{},backing{};bool active=false,revoked=false,has_parked=false;
        EventContinuation continuation;std::vector<Range>ranges;std::vector<std::vector<unsigned char>>parked_ram;
        Context(const Shape&s,const Descriptor&d,const Descriptor&raw,const Prepared&p,const Layout&l,const Streams&streams_,const ps2::Cpu&c,unsigned owner_,unsigned attributes_):
          shape(s),logical(d),declared(raw),prepared(p),layout(l),streams(streams_),entry(c),parked(c),owner(owner_),attributes(attributes_),continuation(streams_,p){}
    };
    const Contract&contract_;resource_owner_epochs::Registry*owners_{};std::uint64_t source_entries_{},snapshot_entries_{},active_advances_{};std::map<Key,std::shared_ptr<Context>>pending_;std::map<unsigned,bool>retiring_caches_;std::uint64_t epoch_{};
    std::uint64_t entries_{},resumes_{},completed_{},suspended_{},callbacks_{},other_{},orphans_{},failures_{};
    [[noreturn]] void fail(const char*reason){++failures_;std::fprintf(stderr,"[resource-reference] REFUSED %s\n",reason);throw std::runtime_error(std::string("resource reference: ")+reason);}
    static unsigned word(unsigned at){if(at>ps2::kRamSize-4||(at&3))resources::refuse("resource reference: word bounds/alignment");unsigned value;std::memcpy(&value,ps2::g_ram+at,4);return value;}
    static unsigned half(unsigned at){if(at>ps2::kRamSize-2||(at&1))resources::refuse("resource reference: half bounds/alignment");unsigned short value;std::memcpy(&value,ps2::g_ram+at,2);return value;}
    static bool private_equal(const ps2::Cpu&a,const ps2::Cpu&b){return !std::memcmp(a.r,b.r,sizeof a.r)&&!std::memcmp(&a.hi,&b.hi,sizeof a.hi)&&!std::memcmp(&a.lo,&b.lo,sizeof a.lo)&&a.sa==b.sa&&!std::memcmp(a.f,b.f,sizeof a.f)&&a.facc==b.facc&&a.fcr31==b.fcr31&&!std::memcmp(&a.vu0,&b.vu0,sizeof a.vu0);}
    static bool code(const Descriptor&d,const Shape&s,unsigned base){
        if((base&15)||base>ps2::kRamSize-4ull*(s.total_words+4))return false;
        std::vector<Bits>words(s.total_words);std::memcpy(words.data(),ps2::g_ram+base,4*s.total_words);
        return resource_word_equality({d.attributes.data(),d.attribute_count},{d.effects.data(),d.effect_count},{d.filters.data(),d.filter_count},words,s.data_words,s.total_words).equal;
    }
    static bool logical_equal(const Descriptor&a,const Descriptor&b){
        if(a.attribute_count!=b.attribute_count||a.effect_count!=b.effect_count||a.filter_count!=b.filter_count)return false;
        for(unsigned i=0;i<a.attribute_count;++i)if(a.attributes[i].tag!=b.attributes[i].tag||a.attributes[i].stream!=b.attributes[i].stream||a.attributes[i].offset!=b.attributes[i].offset)return false;
        for(unsigned i=0;i<a.effect_count;++i)if(a.effects[i].id!=b.effects[i].id||a.effects[i].destination_tag!=b.effects[i].destination_tag||a.effects[i].source_tag!=b.effects[i].source_tag)return false;
        for(unsigned i=0;i<a.filter_count;++i)if(a.filters[i].id!=b.filters[i].id||a.filters[i].source_tag!=b.filters[i].source_tag)return false;
        return true;
    }
    Descriptor descriptor(const ps2::Cpu&c,unsigned object,unsigned&owner,unsigned&attributes){
        Descriptor d{};owner=word(object+72);if(owner<16||owner>ps2::kRamSize-544)resources::refuse("resource owner extent unproved");
        d.attribute_count=word(owner+536);attributes=word(owner+540);d.effect_count=word(object+192);d.filter_count=word(object+80);
        if(!d.attribute_count||d.attribute_count>20||d.effect_count>13||d.filter_count>1||attributes>ps2::kRamSize-12*d.attribute_count||(attributes&3))resources::refuse("descriptor/resource bounds unproved");
        for(unsigned i=0;i<d.attribute_count;++i)d.attributes[i]={word(attributes+12*i),word(attributes+12*i+4),word(attributes+12*i+8)};
        for(unsigned i=0;i<d.effect_count;++i){auto&e=d.effects[i];const auto at=object+208+48*i;e.id=half(at);e.destination_tag=word(at+8);
            if(e.id==2||e.id==3||e.id==25||e.id==17)e.source_tag=word(at+16);
            for(unsigned j=0;j<3;++j)e.constant[j]=word(at+16+4*j);
            e.remap={std::int32_t(word(at+32)),std::int32_t(word(at+36)),word(at+24),word(at+28)};
        }
        for(unsigned i=0;i<d.filter_count;++i){const auto at=object+84+24*i;/* Constructor linkage is not a runtime filter operation. Body equality selects the emitted prefix. */d.filters[i]={half(at),word(at+4),word(at+20)};}
        d.scalar_step=c.f[12];d.vector_step=c.vu0.vf[1].w[0];return d;
    }
    static bool cache_member(unsigned owner,unsigned object,unsigned base){
        if(owner<80||word(owner-64)!=0x59fd90||!word(owner-60))return false;
        if(word(0x59fd90)!=0x12d870||word(0x59fd94)!=0x12d860||word(0x59fd98)!=0x12d830||word(0x59fd9c)!=0x210920)return false;
        const auto backing=word(owner-4),count=word(owner+524),table=word(owner+528);
        if(backing<16||backing>base||base>=ps2::kRamSize||table>ps2::kRamSize||count>(ps2::kRamSize-table)/4)return false;
        for(unsigned i=0;i<count;++i)if(word(table+4*i)==object)return word(object+72)==owner;
        return false;
    }
    static unsigned pool_offset(const Layout&p,Slot slot){for(unsigned i=0;i<p.node_count;++i){const auto&n=p.nodes[i];if(n.slot.bank==slot.bank&&n.slot.index==slot.index){if(n.stream!=2)resources::refuse("resource reference: literal ownership");return n.offset;}}resources::refuse("resource reference: literal slot missing");}
    Prepared prepare_pool(Descriptor d,const Layout&p,const Streams&s,unsigned base,const std::shared_ptr<const resource_owner_epochs::SourceProgram>&source){
        auto address=[&](unsigned offset,unsigned bytes){const auto at=std::uint64_t(base)+4*p.data_start_words+offset;if(offset>32767||at+bytes>ps2::kRamSize)fail("literal pool extent unproved");
            if(source&&(!owners_||!owners_->validate_region(source->cache,unsigned(at),bytes,source->generation).admitted))fail("literal pool outside authenticated source allocation");return unsigned(at);};
        for(unsigned i=0;i<d.effect_count;++i){const auto&t=p.effects[i];if(t.constant.present()){const auto at=address(t.constant_offset,t.constant.bank==0?16:4);if(t.constant.bank==0)std::memcpy(d.effects[i].constant.data(),ps2::g_ram+at,16);else d.effects[i].constant[0]=word(at);}}
        for(unsigned i=0;i<d.filter_count;++i)d.filters[i].limit=word(address(pool_offset(p,p.filters[i]),4));
        auto prepared=resources::prepare(d,s);
        for(unsigned i=0;i<d.effect_count;++i)if(d.effects[i].id==17){const auto&t=p.effects[i];prepared.upper[i]=word(address(pool_offset(p,t.upper),4));prepared.scale[i]=word(address(pool_offset(p,t.scale),4));prepared.descriptor.effects[i].remap.start=word(address(pool_offset(p,t.start),4));prepared.descriptor.effects[i].remap.extent=word(address(pool_offset(p,t.extent),4));}
        return prepared;
    }
    static Streams streams(const ps2::Cpu&c){Streams s{};if(c.r[10].d[0]>0x7fffffff)resources::refuse("resource reference: count representation");s.count=c.r[10].w[0];
        for(unsigned i=0;i<2;++i){const auto from=c.r[4+i].d[0],to=c.r[6+i].d[0],stride=c.r[8+i].d[0];if(from>ps2::kRamSize||to>ps2::kRamSize||stride>0x7fffffff)resources::refuse("resource reference: stream representation");s.streams[i]={{reinterpret_cast<std::byte*>(ps2::g_ram+from),ps2::kRamSize-from},{reinterpret_cast<std::byte*>(ps2::g_ram+to),ps2::kRamSize-to},std::size_t(stride)};}return s;}
    static void first_arguments(const ps2::Cpu&c,unsigned object){
        const auto position=word(object+4),a=word(object+44),b=word(object+52);auto ext=[](unsigned x){return std::uint64_t(std::int64_t(std::int32_t(x)));};
        if(c.r[4].d[0]!=ext(word(object+8))||c.r[5].d[0]!=ext(word(object+16))||c.r[6].d[0]!=ext(word(object+40)+position*a)||c.r[7].d[0]!=ext(word(object+48)+position*b)||c.r[8].d[0]!=ext(a)||c.r[9].d[0]!=ext(b)||c.r[10].d[0]!=ext(word(object)))resources::refuse("resource reference: original caller argument relation");
    }
    void ranges(Context&x,const Key&key){
        auto add=[&](std::uint64_t begin,std::uint64_t bytes){if(begin>ps2::kRamSize||bytes>ps2::kRamSize-begin)fail("owned memory extent unproved");if(bytes)x.ranges.push_back({unsigned(begin),unsigned(begin+bytes)});};
        std::vector<Range>identity;add(x.owner-64,64);add(key.object,928);add(key.base,4*(x.shape.total_words+4));add(x.owner,544);if(!x.source_program)add(x.attributes,12*x.logical.attribute_count);add(key.stack,496);add(0x2e60e0,0x2e6dec-0x2e60e0);identity=x.ranges;
        const auto n=x.streams.count;
        for(unsigned i=0;i<x.logical.attribute_count;++i){const auto&a=x.logical.attributes[i];const auto bytes=width(format(a.tag));const auto stride=x.entry.r[8+a.stream].d[0];
            for(unsigned reg:{4+a.stream,6+a.stream}){const auto offset=a.offset;const auto begin=x.entry.r[reg].d[0]+offset;const auto length=n?std::uint64_t(n-1)*stride+bytes:0;
                if(reg>=6)for(const auto&r:identity)if(length&&begin<r.end&&r.begin<begin+length)fail("output aliases code/descriptor/frame");add(begin,length);
                if(a.tag==259){const auto retained=x.entry.r[reg].d[0]+(offset&~15u),extent=n?std::uint64_t(n-1)*stride+16:0;if(reg>=6)for(const auto&r:identity)if(extent&&retained<r.end&&r.begin<retained+extent)fail("retained quad aliases code/descriptor/frame");add(retained,extent);}
            }
        }
        std::sort(x.ranges.begin(),x.ranges.end(),[](auto&a,auto&b){return a.begin<b.begin;});std::vector<Range>merged;
        for(const auto&r:x.ranges){if(!merged.empty()&&r.begin<=merged.back().end)merged.back().end=std::max(merged.back().end,r.end);else merged.push_back(r);}x.ranges=std::move(merged);
    }
    static std::vector<std::vector<unsigned char>>snapshot(const Context&x){std::vector<std::vector<unsigned char>>saved;for(const auto&r:x.ranges)saved.emplace_back(ps2::g_ram+r.begin,ps2::g_ram+r.end);return saved;}
    static bool memory_equal(const Context&x,const std::vector<std::vector<unsigned char>>&saved){if(x.ranges.size()!=saved.size())return false;for(unsigned i=0;i<x.ranges.size();++i)if(std::memcmp(ps2::g_ram+x.ranges[i].begin,saved[i].data(),saved[i].size()))return false;return true;}
    void identity(const Key&key,const std::shared_ptr<Context>&x,const ps2::Cpu&c){
        const auto found=pending_.find(key);if(x->revoked||epoch_!=key.epoch||found==pending_.end()||found->second!=x)fail("context revoked during original callback");
        if(&c!=&ps2::g_cpu||ps2::kernel::current_thread()!=key.thread||c.r[29].d[0]!=key.stack||c.r[19].d[0]!=key.object||c.r[31].d[0]!=0x2e6180||word(key.object+76)!=key.base)fail("retained caller/thread/stack identity changed");
        if(!code(x->logical,x->shape,key.base)||word(key.object+916)!=x->shape.data_words||std::memcmp(ps2::g_ram+0x2e612c,contract_.caller.data(),84))fail("retained code/caller identity changed");
        unsigned owner=x->owner;
        if(x->source_program){
            if(!owners_||word(key.object+72)!=owner)fail("retained source owner binding changed");
            const auto proof=owners_->validate(owner-64,key.object,key.base,4*x->shape.total_words);
            if(!proof.admitted||proof.generation!=x->source_program->generation)fail("source owner epoch revoked");
        }else{
            unsigned attributes=0;const auto d=descriptor(c,key.object,owner,attributes);
            if(owner!=x->owner||attributes!=x->attributes||!logical_equal(d,x->declared))fail("retained descriptor owner/shape changed");
        }
        if(!cache_member(owner,key.object,key.base)||word(owner-60)!=x->cache_identity||word(owner-4)!=x->backing||retiring_caches_.contains(owner-64))fail("retained cache owner identity changed");
    }
    static void apply(ps2::Cpu&current,const Context&x,const Boundary&b,bool guard){
        if(b.budget<INT32_MIN||b.budget>INT32_MAX)resources::refuse("resource reference: budget representation");const auto&p=x.prepared;const auto&d=p.descriptor;const auto&e=b.effects;const auto&l=x.layout;
        auto scalar=[&](Slot s,Bits v){if(s.present())current.f[s.index]=v;};auto vector=[&](Slot s,const Vector&v){if(s.present())std::memcpy(&current.vu0.vf[s.index],v.data(),16);};scalar(l.zero,0);
        for(unsigned i=0;i<d.effect_count;++i){const auto&s=l.effects[i];if(s.constant.present()){if(s.constant.bank==0)vector(s.constant,p.vector_constants[i]);else scalar(s.constant,p.scalar_constants[i]);}scalar(s.upper,p.upper[i]);scalar(s.scale,p.scale[i]);scalar(s.start,d.effects[i].remap.start);scalar(s.extent,d.effects[i].remap.extent);if(e.remap_written)scalar(s.work,e.remap_word);}
        for(unsigned i=0;i<d.filter_count;++i)scalar(l.filters[i],d.filters[i].limit);
        if(e.element_observed){for(unsigned i=0;i<d.attribute_count;++i){const auto&s=l.resources[i];const auto&v=e.last_resources[i];if(s.bank==0)vector(s,v.vector);else if(s.bank==1)scalar(s,v.scalar);else if(s.bank==2)current.r[s.index].sd[0]=std::int32_t(v.scalar);else if(s.bank==3)current.r[s.index].d[0]=v.integer;}if(l.retained.present())std::memcpy(&current.r[l.retained.index],e.last_retained.data(),16);}
        if(e.scalar_accumulator_written)current.facc=e.scalar_accumulator;if(e.vector_accumulator_written)std::memcpy(&current.vu0.acc,e.vector_accumulator.data(),16);if(e.vector_written){current.vu0.mac=e.vector.last_mac;current.vu0.status=e.vector.project_status(current.vu0.status);}current.fcr31|=e.scalar_flags;if(e.condition_written)current.fcr31=(current.fcr31&~0x00800000u)|(e.condition?0x00800000u:0);
        current.r[2].d[0]=b.accepted;for(unsigned i=4;i<8;++i)current.r[i].d[0]=x.entry.r[i].d[0]+std::uint64_t(i<6?b.consumed:b.accepted)*x.entry.r[(i&1)?9:8].d[0];current.r[10].sd[0]=b.phase==Phase::complete?-1:std::int32_t(b.remaining_after_pending);current.budget=std::int32_t(b.budget);
        if(!guard)current.pc=b.phase==Phase::complete?x.entry.r[31].w[0]:(x.entry.pc+4*l.prelude_words)|ps2::kUnwind;
    }
    struct Bridge{Adapter&adapter;std::shared_ptr<Context>context;ps2::Cpu&cpu;Key key;};
    static bool poll(void*raw,const Boundary&before,std::int64_t&budget){auto&b=*static_cast<Bridge*>(raw);auto&a=b.adapter;auto&x=*b.context;auto&c=b.cpu;a.identity(b.key,b.context,c);apply(c,x,before,true);const auto prior=c;const auto saved=snapshot(x);++a.callbacks_;const bool unwind=ps2::preempt(c);budget=c.budget;
        // A shared lease survives every callback revocation. No result may be
        // published until epoch/membership/thread/owner/lifetime are rechecked.
        a.identity(b.key,b.context,c);if(!private_equal(prior,c)||prior.pc!=c.pc)a.fail("original callback changed unproved private/shared state");if(!memory_equal(x,saved))a.fail("original callback changed active-owned memory");return unwind;}
public:
    explicit Adapter(const Contract&contract,resource_owner_epochs::Registry*owners=nullptr):contract_(contract),owners_(owners){static_assert(sizeof(ps2::Cpu)==1456);}
    ~Adapter(){std::fprintf(stderr,"[resource-source-binding] source_entries=%llu snapshot_reference_entries=%llu native_credit=0\n",source_entries_,snapshot_entries_);std::fprintf(stderr,"[resource-reference] summary entries=%llu resumes=%llu completed=%llu suspended=%llu original_preempt_calls=%llu other_aot_invocations=%llu orphan_aot_invocations=%llu failures=%llu pending=%zu\n",entries_,resumes_,completed_,suspended_,callbacks_,other_,orphans_,failures_,pending_.size());}
    void reset(){if(epoch_==UINT64_MAX)fail("epoch exhausted");for(auto&[key,x]:pending_)x->revoked=true;pending_.clear();retiring_caches_.clear();++epoch_;}
    void retire(int thread){for(auto it=pending_.begin();it!=pending_.end();)if(it->first.thread==thread){it->second->revoked=true;it=pending_.erase(it);}else ++it;}
    void retire_cache(unsigned cache){retiring_caches_[cache]=true;for(auto it=pending_.begin();it!=pending_.end();)if(it->second->owner==cache+64){it->second->revoked=true;it=pending_.erase(it);}else ++it;}
    void activate_cache(unsigned cache){retiring_caches_.erase(cache);}
    void retire_identity(unsigned id){std::vector<unsigned>caches;for(const auto&[key,x]:pending_)if(x->cache_identity==id)caches.push_back(x->owner-64);for(unsigned cache:caches)retire_cache(cache);}
    bool pending()const{return !pending_.empty()||active_advances_!=0;}
    bool try_dispatch(ps2::Cpu&c,bool source_required=false){
        if(owners_&&owners_->poisoned())fail("machine state owner poisoned; execution requires successful restore");
        const auto pc=c.pc&~ps2::kUnwind,object=c.r[19].w[0];const auto thread=ps2::kernel::current_thread();
        // Retained contexts cannot escape their authenticated continuation.
        for(const auto&[key,x]:pending_)if(key.thread==thread&&key.stack==c.r[29].d[0]&&(pc==key.base||pc==key.base+4*x->layout.prelude_words))if(object!=key.object||c.r[31].d[0]!=0x2e6180||word(object+76)!=key.base)fail("retained continuation identity changed");
        if(c.r[31].d[0]!=0x2e6180||object<16||object>ps2::kRamSize-928||(object&3))return false;
        const auto base=word(object+76);const Key key{epoch_,c.r[29].d[0],thread,object,base};auto found=pending_.find(key);
        if(found==pending_.end())for(const auto&[prior,retained]:pending_)if(prior.object==object&&prior.base==base)fail(pc==base?"active owner alias across contexts":"retained context key changed");
        std::shared_ptr<Context>x;
        if(found!=pending_.end()){
            x=found->second;
            if(pc!=base+4*x->layout.prelude_words)fail("retained resume PC is not admitted");
            identity(key,x,c);if(x->active||!x->has_parked)fail("resume lifecycle unproved");
            if(!private_equal(x->parked,c))fail("retained private/shared VU changed");
            if(!memory_equal(*x,x->parked_ram))fail("active-owned memory changed during suspension");++resumes_;
        }else{
            if(pc!=base){++orphans_;return false;}
            Descriptor raw{},selected{};Shape shape{};Layout layout{};unsigned owner=0,attributes=0;bool admitted=false;std::shared_ptr<const resource_owner_epochs::SourceProgram>source_program;
            try{
                owner=word(object+72);
                if(owners_&&owner>=64){
                    source_program=owners_->get_program(owner-64,object);
                    if(source_program){
                        const auto&input=*source_program->input;
                        if(input.original_attributes.empty()||input.original_attributes.size()>20||input.original_effects.size()>13||input.outcome.effective.size()>1)resources::refuse("source program outside current native bounds");
                        raw.attribute_count=unsigned(input.original_attributes.size());std::copy(input.original_attributes.begin(),input.original_attributes.end(),raw.attributes.begin());
                        raw.effect_count=unsigned(input.original_effects.size());std::copy(input.original_effects.begin(),input.original_effects.end(),raw.effects.begin());
                        for(unsigned i=0;i<raw.effect_count;++i)if(raw.effects[i].id!=2&&raw.effects[i].id!=3&&raw.effects[i].id!=25&&raw.effects[i].id!=17)raw.effects[i].source_tag=0;
                        raw.filter_count=unsigned(input.outcome.effective.size());std::copy(input.outcome.effective.begin(),input.outcome.effective.end(),raw.filters.begin());
                        raw.scalar_step=c.f[12];raw.vector_step=c.vu0.vf[1].w[0];
                        activate_cache(owner-64);
                    }else if(owners_->has_owner(owner-64)){++other_;return false;}
                }
                if(!source_program&&source_required){++other_;return false;}
                if(!source_program)raw=descriptor(c,object,owner,attributes);
                if(retiring_caches_.contains(owner-64)||!cache_member(owner,object,base)){++other_;return false;}
                // Original construction can stop at the first refused filter.
                // Current linkage cannot recover its historical value. Compare
                // EVERY possible bounded prefix; only exact code/metadata wins.
                for(unsigned count=source_program?raw.filter_count:0;count<=raw.filter_count;++count){
                    auto d=raw;d.filter_count=count;const auto l=derive_layout(d);if(!l.intact())continue;
                    unsigned words=0;for(unsigned b:{0u,1u,3u,4u,5u,6u})words+=l.intended[b];
                    Shape candidate{words,l.data_start_words,l.total_words};
                    if(word(object+916)==candidate.data_words&&code(d,candidate,base)){
                        if(admitted)resources::refuse("ambiguous source body prefix");
                        admitted=true;selected=d;shape=candidate;layout=l;
                    }
                }
            }catch(const std::exception&){++other_;return false;}
            if(!admitted){++other_;return false;}
            if(source_program){const auto proof=owners_->validate(owner-64,object,base,4*shape.total_words);if(!proof.admitted||proof.generation!=source_program->generation){++other_;return false;}}
            if(&c!=&ps2::g_cpu||thread<=0||key.stack<16||key.stack>ps2::kRamSize-496||(key.stack&15)||c.r[19].d[0]!=object||c.r[11].d[0]!=base+4*shape.data_words||std::memcmp(ps2::g_ram+0x2e612c,contract_.caller.data(),84))fail("known plan caller/frame/data binding unproved");
            for(const auto&[other,retained]:pending_)if(other.object==object&&other.base==base)fail("active owner alias across contexts");
            if(c.r[12].d[0]!=base)fail("initial target binding changed");first_arguments(c,object);
            auto streams_=streams(c);auto prepared=prepare_pool(selected,layout,streams_,base,source_program);
            x=std::make_shared<Context>(shape,selected,raw,prepared,layout,streams_,c,owner,attributes);x->cache_identity=word(owner-60);x->backing=word(owner-4);x->source_program=source_program;ranges(*x,key);pending_.emplace(key,x);++entries_;if(source_program)++source_entries_;else ++snapshot_entries_;
        }
        x->active=true;struct AdvanceLease{Adapter&owner;explicit AdvanceLease(Adapter&a):owner(a){++owner.active_advances_;}~AdvanceLease(){--owner.active_advances_;}}advance_lease(*this);Bridge bridge{*this,x,c,key};try{const auto boundary=x->continuation.advance(c.budget,{&bridge,poll});identity(key,x,c);apply(c,*x,boundary,false);x->active=false;
            if(boundary.phase==Phase::complete){++completed_;pending_.erase(key);}else if(boundary.phase==Phase::suspended){++suspended_;x->has_parked=true;x->parked=c;x->parked_ram=snapshot(*x);}else fail("native continuation phase unproved");
        }catch(...){x->active=false;x->revoked=true;const auto live=pending_.find(key);if(live!=pending_.end()&&live->second==x)pending_.erase(live);throw;}return true;
    }
};
}
// Preserve the measured kernel/system lifecycle hook ABI when replacing only
// dispatch and adapter objects in the complete compiled reference candidate.
namespace mka::emitted::reference {bool preferred_family(ps2::Cpu&);void reset_family_after_state_replacement();void retire_family_thread(int);bool family_pending();}
