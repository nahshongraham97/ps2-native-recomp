#include "mka/movie_prefix_events.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>
using namespace mka::video;
namespace {int fail_after=-1;}
void* operator new(std::size_t n){if(fail_after==0){fail_after=-1;throw std::bad_alloc();}if(fail_after>0)--fail_after;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}void operator delete(void* p,std::size_t)noexcept{std::free(p);}
namespace {
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
NativeMoviePrefixEvents make(std::uint64_t origin=0){MoviePrefixFailure f;auto p=NativeMoviePrefixEvents::create(origin,f);check(bool(p)&&f.kind==MoviePrefixError::none,"explicit source origin");return std::move(*p);}
std::uint32_t literal_class(unsigned code){if(code==0)return 4;if(code==1)return 3;if(code<=0xaf)return 1;if(code==0xb2)return 32;if(code==0xb3)return 64;if(code==0xb5)return 16;if(code==0xb7)return 128;if(code==0xb8)return 8;return 0;}
std::vector<MoviePrefixEvent> scan(const std::vector<std::uint8_t>& bytes,std::span<const std::size_t> chunks){auto p=make(100);MoviePrefixFailure f;std::vector<MoviePrefixEvent> result;std::size_t at=0,index=0;while(at<bytes.size()){const auto n=std::min(chunks[index++%chunks.size()],bytes.size()-at);check(n>0,"positive fixture chunk");auto batch=p.feed(100+at,std::span(bytes).subspan(at,n),f);check(batch&&f.kind==MoviePrefixError::none&&batch->es_begin==100+at&&batch->es_end==100+at+n,"arbitrary contiguous feed");result.insert(result.end(),batch->events.begin(),batch->events.end());at+=n;}auto tail=p.finish(f);check(tail&&tail->source_end==100+bytes.size()&&tail->bytes.size()==std::min<std::size_t>(3,bytes.size()),"format tail and exact end");return result;}
}
int main(){try{
 static_assert(!std::is_default_constructible_v<NativeMoviePrefixEvents>);static_assert(!std::is_copy_constructible_v<NativeMoviePrefixEvents>);
 MoviePrefixFailure f;
 std::vector<std::uint8_t> all;std::vector<MoviePrefixEvent> expected;
 for(unsigned c=0;c<256;++c){all.insert(all.end(),{0,0,1,static_cast<std::uint8_t>(c)});expected.push_back({100+4*c,0x100u+c,literal_class(c)});}
 const std::array<std::size_t,1> one{all.size()},bytes1{1},bytes2{2},bytes3{3};const std::array<std::size_t,7> varied{1,2,3,4,5,17,257};
 check(scan(all,one)==expected,"all256 literal source code classes with class0 preserved");for(const auto& chunks:{std::span<const std::size_t>(bytes1),std::span<const std::size_t>(bytes2),std::span<const std::size_t>(bytes3),std::span<const std::size_t>(varied)})check(scan(all,chunks)==expected,"single vs arbitrary split equivalence");
 for(std::size_t cut=1;cut<=3;++cut){auto p=make(500);const std::array<std::uint8_t,4> input{0,0,1,0xb3};auto a=p.feed(500,std::span(input).first(cut),f);check(a&&a->events.empty(),"no incomplete4byte occurrence");auto b=p.feed(500+cut,std::span(input).subspan(cut),f);check(b&&b->events==std::vector<MoviePrefixEvent>{{500,0x1b3,64}},"prefix split1/2/3 exact FIRSTbyte");}
 const std::vector<std::uint8_t> ordered{0,0,0,1,0xb3,0,0,1,0xb3,0,0,1,0xb5,0,0,1,0xb2,0,0,1,1,0,0,1,2,0,0,1,0xff};
 const std::vector<MoviePrefixEvent> order_expected{{101,0x1b3,64},{105,0x1b3,64},{109,0x1b5,16},{113,0x1b2,32},{117,0x101,3},{121,0x102,1},{125,0x1ff,0}};
 check(scan(ordered,varied)==order_expected,"repeated headers/ext/userdata/slices/unknown retain source order");
 {auto p=make();const std::array<std::uint8_t,8> mismatch{0,1,0xb3,0,0,2,0,1};auto r=p.feed(0,mismatch,f);check(r&&r->events.empty(),"leading prefix mismatches not recognized");auto tail=p.finish(f);check(tail&&tail->bytes==std::vector<std::uint8_t>({2,0,1}),"mismatch tail observed without MPEG judgment");}
 {auto p=make();const std::array<std::uint8_t,3> incomplete{0,0,1};auto r=p.feed(0,incomplete,f);check(r&&r->events.empty(),"incomplete prefix no event");auto t=p.finish(f);check(t&&t->bytes==std::vector<std::uint8_t>({0,0,1}),"finish observes partial prefix without malformed claim");auto again=p.finish(f);check(again&&again->bytes==t->bytes,"finish idempotent");auto state=p.snapshot(f);check(state&&state->finished&&!state->failed,"finished snapshot");check(!p.feed(3,{},f)&&f.kind==MoviePrefixError::after_finish,"feed after finish named");check(!p.finish(f)&&f.kind==MoviePrefixError::failed_owner&&f.cause==MoviePrefixError::after_finish,"terminal cause retained");}
 {auto p=make(40);auto empty=p.feed(40,{},f);check(empty&&empty->events.empty()&&empty->es_end==40,"empty contiguous chunk");auto state=*p.snapshot(f);check(!p.feed(41,{},f)&&f.kind==MoviePrefixError::noncontiguous&&f.expected_offset==40&&f.provided_offset==41,"gap named");state.failed=true;check(p.snapshot(f)==state,"bad gap no offset/tail commit");check(!p.feed(40,{},f)&&f.kind==MoviePrefixError::failed_owner,"failed owner does not retry");}
 {auto p=make(9);const std::array<std::uint8_t,1> byte{0};check(p.feed(9,byte,f).has_value(),"contiguous first byte");check(!p.feed(9,byte,f)&&f.kind==MoviePrefixError::noncontiguous,"overlap rejected");}
 {auto p=make(UINT64_MAX);const std::array<std::uint8_t,1> byte{0};check(!p.feed(UINT64_MAX,byte,f)&&f.kind==MoviePrefixError::offset_overflow,"endpoint overflow before input read");auto state=p.snapshot(f);check(state&&state->next_es_byte==UINT64_MAX&&state->tail_size==0&&state->failed,"overflow is atomic");}
 {auto p=make(UINT64_MAX-4);const std::array<std::uint8_t,4> prefix{0,0,1,0};auto r=p.feed(UINT64_MAX-4,prefix,f);check(r&&r->events==std::vector<MoviePrefixEvent>{{UINT64_MAX-4,0x100,4}}&&r->es_end==UINT64_MAX,"highest representable half-open source range");}
 {auto p=make();const std::array<std::uint8_t,2> prefix{0,0};check(p.feed(0,prefix,f).has_value(),"move partial input");NativeMoviePrefixEvents moved(std::move(p));check(!p.feed(2,{},f)&&f.kind==MoviePrefixError::invalid_owner&&!p.snapshot(f),"moved owner rejects");moved=std::move(moved);const std::array<std::uint8_t,2> suffix{1,0xb8};auto r=moved.feed(2,suffix,f);check(r&&r->events==std::vector<MoviePrefixEvent>{{0,0x1b8,8}},"move/selfmove retains format lookbehind");auto held=r->events;auto replacement=make();replacement=std::move(moved);check(!moved.finish(f)&&f.kind==MoviePrefixError::invalid_owner&&replacement.finish(f),"move assignment source invalidated");check(held==std::vector<MoviePrefixEvent>{{0,0x1b8,8}},"owned results independent of moved owner");}
 {std::optional<MoviePrefixBatch> held;std::optional<MoviePrefixTail> tail;{auto p=make(77);const std::array<std::uint8_t,4> input{0,0,1,0xb2};held=p.feed(77,input,f);tail=p.finish(f);}check(held&&held->events==std::vector<MoviePrefixEvent>{{77,0x1b2,32}}&&tail&&tail->bytes==std::vector<std::uint8_t>({0,1,0xb2}),"outputs survive input and owner destruction");}
 for(int allocation=0;allocation<2;++allocation){auto p=make();const auto before=*p.snapshot(f);const std::array<std::uint8_t,8> input{0,0,1,0xb3,0,0,1,0xb8};fail_after=allocation;auto r=p.feed(0,input,f);fail_after=-1;check(!r&&f.kind==MoviePrefixError::allocation,"actual staged event allocation failure");auto after=*p.snapshot(f);auto expected_state=before;expected_state.failed=true;check(after==expected_state,"allocation after partial local scan has no committed output/input");}
 {auto p=make();const std::array<std::uint8_t,1> byte{1};check(p.feed(0,byte,f).has_value(),"tail allocation setup");auto before=*p.snapshot(f);fail_after=0;auto r=p.finish(f);fail_after=-1;check(!r&&f.kind==MoviePrefixError::allocation,"actual finish tail allocation named");before.failed=true;check(p.snapshot(f)==before,"failed finish leaves unfinished source state");}
 std::puts("PASS native raw ES prefix events streaming/classes/ownership; no header admission or timeline binding");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"[movie-prefix-events] %s\n",e.what());return 1;}}
