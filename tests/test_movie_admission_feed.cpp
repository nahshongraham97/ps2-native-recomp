#include "mka/movie_admission_feed.hpp"
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <utility>
using namespace mka::video;
namespace { int allocation_after = -1; NativeMovieAdmissionFeed* reenter = nullptr; bool reentry_checked = false; }
void* operator new(std::size_t n) { if(reenter){auto* p=reenter;reenter=nullptr;MovieAdmissionFeedFailure a,b;const bool skipped=p->skip(1,a);const auto snapshot=p->snapshot(b);reentry_checked=!skipped&&!snapshot&&a.kind==MovieAdmissionFeedError::reentry&&b.kind==MovieAdmissionFeedError::reentry;}if (allocation_after == 0) { allocation_after = -1; throw std::bad_alloc(); } if (allocation_after > 0) --allocation_after; if (auto* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
namespace {
using Bytes = std::vector<std::uint8_t>;
void check(bool v, const char* why) { if (!v) throw std::runtime_error(why); }
struct Bits {
    Bytes bytes; unsigned bit = 0;
    explicit Bits(unsigned code) : bytes{0, 0, 1, std::uint8_t(code)} {}
    void put(unsigned n, std::uint32_t v) { for (unsigned i = 0; i < n; ++i) { if (!bit) bytes.push_back(0); bytes.back() |= ((v >> (n - 1 - i)) & 1) << (7 - bit); bit = (bit + 1) % 8; } }
};
Bytes sequence(bool matrix = false) { Bits b(0xb3); for (auto [n,v] : {std::pair{12u,640u},{12u,448u},{4u,3u},{4u,5u},{18u,1u},{1u,1u},{10u,112u},{1u,0u}}) b.put(n,v); b.put(1,matrix); if(matrix)for(unsigned i=0;i<64;++i)b.put(8,i);b.put(1,0);return b.bytes; }
Bytes extension(bool progressive = true) { Bits b(0xb5);for(auto [n,v]:{std::pair{4u,1u},{8u,0x48u},{1u,unsigned(progressive)},{2u,1u},{2u,0u},{2u,0u},{12u,0u},{1u,1u},{8u,0u},{1u,0u},{2u,0u},{5u,0u}})b.put(n,v);return b.bytes; }
Bytes gop(bool marker = true) { Bits b(0xb8); b.put(25, marker ? 1u << 12 : 0); b.put(1,0);b.put(1,0);return b.bytes; }
Bytes picture() { Bits b(0); b.put(10,7);b.put(3,1);b.put(16,0xffff);b.put(1,0);return b.bytes; }
Bytes coding(bool repeat = true, bool top = true) { Bits b(0xb5);b.put(4,8);for(unsigned i=0;i<4;++i)b.put(4,1);b.put(2,0);b.put(2,3);for(auto v:{unsigned(top),0u,0u,0u,0u,0u,unsigned(repeat),0u,1u})b.put(1,v);b.put(1,0);return b.bytes; }
void add(Bytes& a, const Bytes& b) { a.insert(a.end(), b.begin(), b.end()); }
struct Feed {
    std::shared_ptr<const MovieSourceObservationIdentity> id = std::make_shared<const MovieSourceObservationIdentity>();
    std::unique_ptr<NativeMovieAdmissionFeed> owner;
    Bytes bytes; std::uint64_t seq = 0; std::size_t sent = 0, search = 0, first_limit;
    explicit Feed(Bytes b, std::size_t split = 0) : bytes(std::move(b)), first_limit(split ? split : bytes.size()) {
        MovieAdmissionFeedFailure f; owner = NativeMovieAdmissionFeed::create("fixture.sfd", id, 0, f); check(bool(owner), "explicit source feed create");
    }
    MovieSourceObservationBatch chunk(std::size_t end) {
        MovieSourceObservationBatch b{"fixture.sfd",id,{}};
        b.events.push_back({++seq,MovieVideoEsObserved{sent,end,Bytes(bytes.begin()+sent,bytes.begin()+end)}});
        while(search+4<=end) {
            const auto at=search++;
            if(bytes[at]||bytes[at+1]||bytes[at+2]!=1)continue;
            std::uint32_t word=0;for(unsigned i=0;i<4;++i)word=(word<<8)|bytes[at+i];
            const bool first=at<first_limit;MovieVideoPesSpan pes{0xe0,first?100u:1000u,first?110u:1010u,first?1000u:2000u,first?0u:first_limit,first?first_limit:bytes.size(),first?std::nullopt:std::optional<std::uint64_t>(16),{}};
            b.events.push_back({++seq,MovieVideoPrefixObserved{{at,word,movie_prefix_classification(word)},pes,pes.quarter_pts()}});
        }
        sent=end;return b;
    }
    void push(std::size_t end) { MovieAdmissionFeedFailure f; auto b=chunk(end);check(owner->push(b,f),"checked raw/prefix ingress"); }
    MovieAdmissionCandidate next() { MovieAdmissionFeedFailure f;auto n=owner->parse_next(f);check(bool(n),"explicit parser candidate");return std::move(*n); }
    MovieAdmissionFeedSnapshot snapshot() { MovieAdmissionFeedFailure f;auto n=owner->snapshot(f);check(bool(n),"owned feed diagnostics");return *n; }
};
}
int main() { try {
    Bytes data=sequence();add(data,extension());const auto ga=data.size();add(data,gop());const auto pa=data.size();add(data,picture());add(data,coding());const auto slice=data.size();add(data,{0,0,1,1,0x55});add(data,{0,0,1,0xb7});
    Feed feed(data,ga+4);feed.push(ga+4);
    check(feed.snapshot().current.gop_parse_entries==0,"raw prefix never increments GOP counter");
    auto s=feed.next();check(s.kind==MovieAdmissionCandidateKind::parsed_header&&s.consumed_bytes==12&&s.current.sequence&&!s.current.gop&&!s.current.picture,"B3 before future GOP/picture exists");
    feed.next();auto n=feed.next();check(n.kind==MovieAdmissionCandidateKind::need_more&&n.current.gop_parse_entries==1&&n.current.last_primary_kind==2&&!n.current.gop&&n.consumed_bytes==0,"explicit incomplete GOP entry increments without byte progress");
    n=feed.next();check(n.current.gop_parse_entries==2&&n.current.successful_gops==0,"second explicit parser entry is not successful-format count");
    feed.push(data.size());n=feed.next();check(n.current.gop_parse_entries==3&&n.current.successful_gops==1&&n.current.gop&&!n.current.picture&&n.parsed->origin.prefix.es_prefix_first_byte==ga&&!n.parsed->origin.quarter_pts,"GOP current before any picture and exact missing FIRST-byte PTS");
    auto held=n.current.gop;feed.next();n=feed.next();check(n.current.picture&&n.current.coding&&n.current.info54==4&&n.current.info57==1,"source flags literal progressive/repeat/top111 maps4/1");
    n=feed.next();check(n.kind==MovieAdmissionCandidateKind::first_slice&&n.es_begin==slice&&n.consumed_bytes==0,"first slice proposes work without consumption");
    const auto before=feed.snapshot();MovieAdmissionFeedFailure f;auto proposal=feed.owner->proposal(f);check(bool(proposal)&&feed.snapshot().es_begin==before.es_begin,"source boundary proposal cannot consume");
    check(feed.next().kind==MovieAdmissionCandidateKind::blocked&&feed.owner->skip(0,f)&&feed.next().kind==MovieAdmissionCandidateKind::blocked,"pending work cannot replay or auto advance");
    check(!feed.owner->skip(UINT64_MAX,f)&&feed.snapshot().es_begin==slice,"invalid actual consumption atomic");check(feed.owner->skip(5,f),"explicit actual work cursor consumption");
    n=feed.next();check(n.kind==MovieAdmissionCandidateKind::parsed_header&&n.consumed_bytes==4&&n.current.last_primary_kind==1&&n.current.picture&&n.current.gop==held,"B7 neither future picture nor guessed state reset/end");
    feed.owner.reset();data.clear();check(std::get<MovieGopHeader>(held->unit.payload).raw_time_code==(1u<<12),"owned current origin/raw header survives source/feed teardown");
    {
        Feed whole(feed.bytes);whole.push(whole.bytes.size());for(unsigned i=0;i<5;++i)whole.next();auto work=whole.next();check(work.kind==MovieAdmissionCandidateKind::first_slice&&work.current.successful_gops==1&&work.current.gop_parse_entries==1&&work.current.picture->origin.prefix.es_prefix_first_byte==pa,"alternative chunk layout same parsed fields with independently explicit invocation count");
    }
    {
        Bytes a=sequence();add(a,extension());const auto pic=a.size();add(a,picture());add(a,coding(false,false));add(a,{0,0,1,1,0x55});Feed cross(a,pic+2);cross.push(pic+2);cross.next();cross.next();check(cross.next().kind==MovieAdmissionCandidateKind::need_more,"split raw prefix waits for metadata");cross.push(a.size());auto p=cross.next();check(p.parsed&&p.parsed->origin.prefix.es_prefix_first_byte==pic&&p.parsed->origin.pes.es_end==pic+2&&!p.parsed->origin.quarter_pts,"cross-PES prefix keeps FIRST-byte owner without timestamp inheritance");
    }
    {
        Bytes a=sequence(true);a[a.size()-2]=0;a.back()=0;const auto end=a.size();add(a,{1,0xb8,0,0,1,0xb7});Feed cross(a,end);cross.push(end);check(cross.next().consumed_bytes==76,"structured matrix exact extent");cross.push(a.size());check(cross.snapshot().current.gop_parse_entries==0&&cross.next().kind==MovieAdmissionCandidateKind::other,"late raw prefix inside consumed matrix is not new GOP");check(cross.owner->skip(2,f)&&cross.next().consumed_bytes==4,"explicit padding skip reaches real B7");
    }
    constexpr std::uint8_t flags[8]{0,0,1,1,0,0,2,4};
    for(unsigned bits=0;bits<8;++bits){Bytes a=sequence();add(a,extension(bits&4));add(a,picture());add(a,coding(bits&2,bits&1));Feed x(a);x.push(a.size());for(unsigned i=0;i<4;++i)x.next();auto c=x.snapshot().current;check(c.info54==flags[bits]&&c.info57==bool(bits&2),"all8 source-derived flag table combinations");}
    {
        for (auto code : {0xb3u, 0u}) {
            Feed partial(Bytes{0,0,1,std::uint8_t(code)});partial.push(partial.bytes.size());
            auto entered=partial.next();check(entered.kind==MovieAdmissionCandidateKind::need_more&&entered.current.last_primary_kind==(code==0xb3?1:3),"primary kind updated on incomplete sequence/picture entry");
        }
        Feed x(gop(false));x.push(x.bytes.size());check(!x.owner->parse_next(f)&&f.kind==MovieAdmissionFeedError::parser,"malformed GOP named parser failure");auto state=x.snapshot();check(state.failed&&state.current.gop_parse_entries==1&&state.current.last_primary_kind==2&&state.current.successful_gops==0&&state.es_begin==0,"source entry counter survives field failure, no fabricated unit consumption");check(!x.owner->parse_next(f)&&f.kind==MovieAdmissionFeedError::failed_owner,"terminal malformed owner rejects retry");
    }
    {
        Feed x(sequence());auto batch=x.chunk(x.bytes.size());allocation_after=0;const bool pushed=x.owner->push(batch,f);allocation_after=-1;check(!pushed&&f.kind==MovieAdmissionFeedError::allocation&&x.snapshot().observation_sequence==0&&x.snapshot().buffered_bytes==0,"genuine ingress allocation failure commits no bytes/sequence");check(x.owner->push(batch,f),"same observation retry after host allocation failure");allocation_after=0;auto parsed=x.owner->parse_next(f);allocation_after=-1;check(!parsed&&f.kind==MovieAdmissionFeedError::allocation&&x.snapshot().es_begin==0&&x.snapshot().current.sequence_parse_entries==0,"host allocation failure rolls back parser-entry publication");
        auto foreign=batch;foreign.identity=std::make_shared<const MovieSourceObservationIdentity>();const auto snapshot=x.snapshot();check(!x.owner->push(foreign,f)&&f.kind==MovieAdmissionFeedError::foreign_source&&x.snapshot().observation_sequence==snapshot.observation_sequence,"foreign source rejects atomically");
        MovieAdmissionFeedFailure thread_failure;std::optional<MovieAdmissionCandidate> result;std::thread t([&]{result=x.owner->parse_next(thread_failure);});t.join();check(!result&&thread_failure.kind==MovieAdmissionFeedError::wrong_thread,"owner-thread contract");
        NativeMovieAdmissionFeed moved(std::move(*x.owner));check(!x.owner->snapshot(f)&&f.kind==MovieAdmissionFeedError::invalid_owner,"moved owner rejects");moved=std::move(moved);reenter=&moved;auto completed=moved.parse_next(f);check(completed&&completed->kind==MovieAdmissionCandidateKind::parsed_header&&reentry_checked,"selfmove preserves bytes; allocator reentry cannot consume or inspect half operation");
    }
    std::puts("PASS native admission feed current headers/explicit parse entries/work candidates; no business admission or decoder execution");return 0;
} catch(const std::exception& e){std::fprintf(stderr,"FAIL admission feed %s\n",e.what());return 1;} }
