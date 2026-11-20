#include "mka/movie_header_boundary.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <limits>
#include <new>
#include <stdexcept>
#include <thread>
#include <type_traits>
using namespace mka::video;
namespace {int fail_after=-1;}
void* operator new(std::size_t n){if(fail_after==0){fail_after=-1;throw std::bad_alloc();}if(fail_after>0)--fail_after;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}
namespace {
using Bytes=std::vector<std::uint8_t>;
void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
struct Rig {
 std::shared_ptr<const MovieSourceObservationIdentity> id=std::make_shared<const MovieSourceObservationIdentity>();
 std::optional<NativeMovieHeaderBoundary> owner;MovieHeaderBoundaryFailure f;
 explicit Rig(std::uint64_t begin=0){owner=NativeMovieHeaderBoundary::create("source",id,begin,f);check(owner.has_value()&&f.kind==MovieHeaderBoundaryError::none,"explicit source owner");}
 MovieHeaderBoundaryState state(){auto s=owner->snapshot(f);check(s.has_value()&&f.kind==MovieHeaderBoundaryError::none,"owned state snapshot");return *s;}
 void append(const Bytes& b){const auto end=state().es_end;check(b.size()<=UINT64_MAX-end,"fixture endpoint");check(owner->append("source",id,{end,end+b.size(),b},f)&&f.kind==MovieHeaderBoundaryError::none,"contiguous append");}
 MovieHeaderBoundaryProposal proposal(){auto q=owner->propose(f);check(q.has_value()&&f.kind==MovieHeaderBoundaryError::none,"source boundary proposal");return *q;}
 void consume(std::uint64_t n,MovieHeaderConsumeKind kind=MovieHeaderConsumeKind::parser){check(owner->consume("source",id,kind,n,f)&&f.kind==MovieHeaderBoundaryError::none,"explicit checked consumption");}
};
Bytes view(std::size_t n,std::initializer_list<std::pair<std::size_t,std::uint8_t>> codes){Bytes b(n,0x55);for(auto [at,code]:codes){check(at<=n&&n-at>=4,"literal source prefix extent");b[at]=0;b[at+1]=0;b[at+2]=1;b[at+3]=code;}return b;}
void exact(const MovieHeaderBoundaryProposal& q,std::uint64_t begin,std::uint64_t count,std::uint32_t c,bool other=false,bool need=false,bool empty=false){check(q==MovieHeaderBoundaryProposal{begin,count,c,other,empty,need},"literal source proposal fields");}
}
int main(){try{
 static_assert(!std::is_default_constructible_v<NativeMovieHeaderBoundary>);static_assert(!std::is_copy_constructible_v<NativeMovieHeaderBoundary>);
 {Rig r(99);exact(r.proposal(),99,0,0,false,false,true);r.append({});exact(r.proposal(),99,0,0,false,false,true);}
 {Rig r;r.append({7,8,9});exact(r.proposal(),0,0,0,false,true);r.append({1,2,3});exact(r.proposal(),0,3,0,true);r.consume(3,MovieHeaderConsumeKind::skip);check(r.state().bytes==Bytes({1,2,3}),"leading other retains exact last3");}
 {Rig r;r.append(view(20,{{5,0xb3},{13,0xb7}}));exact(r.proposal(),0,5,0,true);check(r.state().es_begin==0&&r.state().bytes.size()==20,"proposal never consumes");r.consume(5,MovieHeaderConsumeKind::skip);exact(r.proposal(),5,8,64);r.consume(8);exact(r.proposal(),13,4,128);r.consume(4);exact(r.proposal(),17,0,0,false,true);}
 {Rig r;r.append(view(60,{{0,0xb3},{8,0xb8},{16,0},{40,0}}));exact(r.proposal(),0,16,64);auto first=r.state();check(first.cached_boundary==40&&first.searched_available_end==60,"LASTCC cache is not refined FIRST endpoint");r.consume(8);check(r.state().cached_boundary==40&&r.state().searched_available_end==60,"in-range cache survives consume");exact(r.proposal(),8,8,8);r.consume(8);exact(r.proposal(),16,24,4);r.consume(24);auto atcache=r.state();check(atcache.cached_boundary==40,"cache at new start retained on consume");exact(r.proposal(),40,4,4,false,true);check(r.state()==atcache,"unchanged end refresh does not replace backing cache");r.append(view(16,{{4,0xb7}}));exact(r.proposal(),40,24,4);r.consume(25);check(!r.state().cached_boundary&&!r.state().searched_available_end,"consumed-past cache clears BOTH C/V");}
 {Rig r;r.append(view(52,{{0,0xb3},{8,0xb8},{40,0xb8}}));exact(r.proposal(),0,8,64);}
 {Rig r;r.append(view(40,{{0,0},{24,0xb3}}));exact(r.proposal(),0,24,4);}
 for(auto c:std::array<std::uint8_t,3>{0xb3,0xb8,0}){Rig r;r.append(view(12,{{0,c}}));exact(r.proposal(),0,0,c==0xb3?64:c==0xb8?8:4,false,true);check(r.state().cached_boundary==0&&r.state().searched_available_end==12,"fresh sole header count0 retains last boundary");exact(r.proposal(),0,4,c==0xb3?64:c==0xb8?8:4,false,true);}
 {Rig r;r.append(view(20,{{0,0xb3},{8,0xb8}}));exact(r.proposal(),0,4,64,false,true);auto before=r.state();exact(r.proposal(),0,4,64,false,true);check(r.state()==before,"sole FIRSTGOP==cachedLAST needs lookahead unchangedE");r.append(view(28,{{8,0xb8}}));exact(r.proposal(),0,8,64);}
 {Rig r;r.append(view(20,{{0,0xb3},{8,0}}));exact(r.proposal(),0,4,64,false,true);auto before=r.state();exact(r.proposal(),0,4,64,false,true);check(r.state()==before,"sole FIRSTpicture==cachedLAST needs lookahead");r.append(view(20,{{8,0}}));exact(r.proposal(),0,8,64);}
 {Rig r;r.append(view(20,{{0,0xb8},{8,0}}));exact(r.proposal(),0,4,8,false,true);r.append(view(20,{{8,0}}));exact(r.proposal(),0,8,8);}
 {Rig r;r.append(view(8,{{0,1}}));exact(r.proposal(),0,4,3,false,true);check(!r.state().cached_boundary&&r.state().searched_available_end==8,"CE firstslice excluded from CC");r.consume(1,MovieHeaderConsumeKind::skip);check(!r.state().cached_boundary&&!r.state().searched_available_end,"absent boundary is outside advanced view; clear searched end too");}
 {Rig r;r.append(view(8,{{0,0},{4,0xb7}}));exact(r.proposal(),0,4,4);check(!r.proposal().need_lookahead,"known count4 is still NOT parser admission");r.consume(4);auto before=r.state();exact(r.proposal(),4,4,128);check(r.state()==before,"B7 bypass does not touch cache");}
 {const auto b=view(60,{{0,0xb3},{8,0xb8},{16,0},{40,0}});for(std::size_t split=0;split<=b.size();++split){Rig r(100);r.append(Bytes(b.begin(),b.begin()+split));r.append(Bytes(b.begin()+split,b.end()));exact(r.proposal(),100,16,64);check(r.state().bytes==b,"all append split points preserve owned source bytes");}}
 for(std::size_t split=1;split<=3;++split){Rig r;const Bytes b{0,0,1,0xb7};r.append(Bytes(b.begin(),b.begin()+split));exact(r.proposal(),0,0,0,false,true);r.append(Bytes(b.begin()+split,b.end()));exact(r.proposal(),0,4,128);}
 {Rig r;r.append(view(9,{{1,0xb2},{5,0xb5}}));exact(r.proposal(),0,6,0,true);}
 {Rig r;const Bytes many(1048579,0x55);r.append(many);exact(r.proposal(),0,many.size()-3,0,true);r.consume(many.size()-3,MovieHeaderConsumeKind::skip);check(r.state().bytes==Bytes(3,0x55),"dynamic native bytes no source capacity");}
 {Rig r(UINT64_MAX-12);r.append(view(12,{{0,0},{8,0xb7}}));exact(r.proposal(),UINT64_MAX-12,8,4);r.consume(8);exact(r.proposal(),UINT64_MAX-4,4,128);r.consume(4);auto before=r.state();check(!r.owner->append("source",r.id,{UINT64_MAX,0,{1}},r.f)&&r.f.kind==MovieHeaderBoundaryError::overflow&&r.state()==before,"native64 endpoint overflow is atomic");}
 {Rig r;r.append(view(20,{{0,0},{8,0xb3}}));r.proposal();auto before=r.state();auto foreign=std::make_shared<const MovieSourceObservationIdentity>();
  check(!r.owner->append("source",foreign,{20,21,{1}},r.f)&&r.f.kind==MovieHeaderBoundaryError::foreign_source&&r.state()==before,"same key foreign epoch rejected");check(!r.owner->consume("wrong",r.id,MovieHeaderConsumeKind::parser,1,r.f)&&r.f.kind==MovieHeaderBoundaryError::foreign_source&&r.state()==before,"foreign parser result key rejected");
  check(!r.owner->append("source",r.id,{19,20,{1}},r.f)&&r.f.kind==MovieHeaderBoundaryError::noncontiguous&&r.state()==before,"overlap rejected atomically");check(!r.owner->append("source",r.id,{21,22,{1}},r.f)&&r.f.kind==MovieHeaderBoundaryError::noncontiguous&&r.state()==before,"gap rejected atomically");check(!r.owner->append("source",r.id,{20,22,{1}},r.f)&&r.f.kind==MovieHeaderBoundaryError::malformed_range&&r.state()==before,"malformed extent rejected atomically");
  check(!r.owner->consume("source",r.id,MovieHeaderConsumeKind::skip,21,r.f)&&r.f.kind==MovieHeaderBoundaryError::range&&r.state()==before,"rejected parser/skip amount no cache loss");check(!r.owner->consume("source",r.id,static_cast<MovieHeaderConsumeKind>(99),1,r.f)&&r.f.kind==MovieHeaderBoundaryError::invalid_consume_kind&&r.state()==before,"unknown consume operation rejected");r.consume(0);check(r.state()==before,"zero consumption no cache mutation");
  std::exception_ptr failure;std::thread t([&]{try{MovieHeaderBoundaryFailure f;check(!r.owner->propose(f)&&f.kind==MovieHeaderBoundaryError::wrong_thread,"foreign query rejected");check(!r.owner->append("source",r.id,{20,20,{}},f)&&f.kind==MovieHeaderBoundaryError::wrong_thread,"foreign append rejected");check(!r.owner->consume("source",r.id,MovieHeaderConsumeKind::skip,1,f)&&f.kind==MovieHeaderBoundaryError::wrong_thread,"foreign consume rejected");}catch(...){failure=std::current_exception();}});t.join();if(failure)std::rethrow_exception(failure);check(r.state()==before,"foreign operations no mutation");
 }
 {Rig r;MovieVideoEsObserved input{0,4,{0,0,1,0xb7}};auto before=r.state();fail_after=0;auto accepted=r.owner->append("source",r.id,input,r.f);fail_after=-1;check(!accepted&&r.f.kind==MovieHeaderBoundaryError::allocation&&r.state()==before,"genuine append allocation failure atomic");check(r.owner->append("source",r.id,input,r.f),"retry after rejected allocation allowed");before=r.state();fail_after=0;auto copy=r.owner->snapshot(r.f);fail_after=-1;check(!copy&&r.f.kind==MovieHeaderBoundaryError::allocation&&r.state()==before,"genuine snapshot allocation failure preserves owner");}
 {MovieHeaderBoundaryFailure f;auto id=std::make_shared<const MovieSourceObservationIdentity>();const std::string long_key(200,'K');fail_after=0;auto owner=NativeMovieHeaderBoundary::create(long_key,id,0,f);fail_after=-1;check(!owner&&f.kind==MovieHeaderBoundaryError::allocation,"genuine key ownership allocation failure");check(!NativeMovieHeaderBoundary::create("",id,0,f)&&f.kind==MovieHeaderBoundaryError::invalid_source,"missing source key");check(!NativeMovieHeaderBoundary::create("source",{},0,f)&&f.kind==MovieHeaderBoundaryError::invalid_source,"missing source identity");}
 {Rig r;r.append({0,0});NativeMovieHeaderBoundary moved(std::move(*r.owner));check(!r.owner->propose(r.f)&&r.f.kind==MovieHeaderBoundaryError::invalid_owner,"moved owner rejected");moved=std::move(moved);check(moved.append("source",r.id,{2,4,{1,0xb7}},r.f),"selfmove retains incomplete prefix");exact(*moved.propose(r.f),0,4,128);Rig replacement;*replacement.owner=std::move(moved);check(!moved.snapshot(r.f)&&r.f.kind==MovieHeaderBoundaryError::invalid_owner&&replacement.owner->propose(r.f)->classification==128,"move assignment retains source and invalidates donor");}
 {std::optional<MovieHeaderBoundaryState> held;std::weak_ptr<const MovieSourceObservationIdentity> weak;MovieHeaderBoundaryFailure f;{auto id=std::make_shared<const MovieSourceObservationIdentity>();weak=id;auto owner=NativeMovieHeaderBoundary::create("held",id,500,f);MovieVideoEsObserved b{500,504,{0,0,1,0xb7}};check(owner&&owner->append("held",id,b,f),"owned lifetime setup");held=owner->snapshot(f);}check(held&&held->bytes==Bytes({0,0,1,0xb7})&&!weak.expired(),"source/data snapshot survives input and owner");held.reset();check(weak.expired(),"final source lifetime released");}
 std::puts("PASS native source header-boundary proposals/cache/ownership; no parser admission or timeline binding");return 0;
}catch(const std::exception&e){std::fprintf(stderr,"[movie-header-boundary] %s\n",e.what());return 1;}}
