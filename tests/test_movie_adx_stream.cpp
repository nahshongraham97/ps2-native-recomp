#include "mka/movie_adx_stream.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <thread>
#include <utility>
using namespace mka::video;
namespace {bool fail_next=false;}
void* operator new(std::size_t n){if(std::exchange(fail_next,false))throw std::bad_alloc();if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}
namespace {
void check(bool yes,const char* s){if(!yes)throw std::runtime_error(s);}
void put16(std::vector<std::uint8_t>& b,std::size_t i,std::uint16_t n){b[i]=n>>8;b[i+1]=n&255;}
void put32(std::vector<std::uint8_t>& b,std::size_t i,std::uint32_t n){b[i]=n>>24;b[i+1]=(n>>16)&255;b[i+2]=(n>>8)&255;b[i+3]=n&255;}
std::vector<std::uint8_t> header(std::uint32_t samples=33){
    std::vector<std::uint8_t> b(40,0);put16(b,0,0x8000);put16(b,2,36);b[4]=3;b[5]=18;b[6]=4;b[7]=2;
    put32(b,8,48000);put32(b,12,samples);put16(b,16,500);b[18]=4;
    put16(b,24,10);put16(b,26,0xfffc);put16(b,28,100);put16(b,30,0xfff9);
    const std::uint8_t cookie[]={'(','c',')','C','R','I'};std::copy(cookie,cookie+6,b.end()-6);return b;
}
std::vector<std::uint8_t> group(){std::vector<std::uint8_t> b(36);std::fill(b.begin()+2,b.begin()+18,0x10);std::fill(b.begin()+20,b.end(),0xf0);return b;}
std::vector<std::uint8_t> marker(std::size_t n=18){check(n>=16&&n<=65539,"fixture marker size");std::vector<std::uint8_t> b(n,0x5a);put16(b,0,0x8001);put16(b,2,static_cast<std::uint16_t>(n-4));return b;}
void append(std::vector<std::uint8_t>& a,const std::vector<std::uint8_t>& b){a.insert(a.end(),b.begin(),b.end());}
struct Resolver final:MovieAdxProfileResolver {
    int calls=0,mode=0;NativeMovieAdxStream* callback_owner=nullptr;bool reentry_rejected=false;
    std::optional<AdxBlockProfile> resolve(const MovieAdxHeader& h,MovieAdxStreamFailure& f) override {
        f={};++calls;
        if(callback_owner){MovieAdxStreamFailure nested;reentry_rejected=!callback_owner->snapshot(nested)&&nested.kind==MovieAdxStreamError::reentrant;}
        std::vector<std::pair<std::int16_t,std::int16_t>> seeds(h.histories.begin(),h.histories.end());
        if(mode==1)++seeds[0].first;
        if(mode==2)seeds.pop_back();
        if(mode==3){f.kind=MovieAdxStreamError::resolver;f.operation="fixture contradictory resolver";}
        if(mode==4)return {};
        if(mode==5)throw std::runtime_error("fixture resolver exception");
        auto c=std::vector<std::pair<std::int16_t,std::int16_t>>(2,{4096,0});
        if(mode==6)c.pop_back();
        return AdxBlockProfile(mode==7?AdxBlockMode::unspecified:AdxBlockMode::version4_flags0,std::move(c),std::move(seeds));
    }
};
struct Rig {
    Resolver* resolver;std::unique_ptr<NativeMovieAdxStream> owner;
    explicit Rig(std::vector<std::uint8_t> extra={}){auto p=std::make_unique<Resolver>();resolver=p.get();MovieAdxStreamFailure f;owner=NativeMovieAdxStream::create(std::move(extra),std::move(p),f);check(owner&&f.kind==MovieAdxStreamError::none,"explicit stream owner");}
};
void preserved(const MovieAdxStreamSnapshot& a,const MovieAdxStreamSnapshot& b){
    check(a.raw_decoded_frames==b.raw_decoded_frames&&a.published_frames==b.published_frames&&a.histories==b.histories&&a.carry==b.carry&&a.marker==b.marker&&a.encoded_ended==b.encoded_ended,"terminal failure leaves committed counters/history/carry unchanged");
    check(bool(a.header)==bool(b.header)&&(!a.header||a.header->raw_header==b.header->raw_header),"terminal failure leaves header unchanged");
}
}
int main(int argc,char**){try{
    check(argc==1,"usage: test_native_movie_adx_stream (no arguments)");MovieAdxStreamFailure f;
    {Rig r;auto h=header();auto g=group();auto end=marker();auto bytes=h;append(bytes,g);append(bytes,g);append(bytes,end);
     auto result=r.owner->feed(bytes,f);check(result.status==MovieAdxStreamStatus::frame&&result.encoded_ended&&result.header_available&&result.new_header&&result.completed_marker==end,"same feed returns owned PCM and marker completion");
     check(result.frame&&result.frame->first_source_frame==0&&result.frame->sample_rate==48000&&result.frame->raw_decoded_frames==64&&result.frame->planes[0].size()==33&&result.frame->planes[1].size()==33,"declared33 trim versus64 decoded");
     check(result.frame->planes[0][0]==11&&result.frame->planes[0][31]==26&&result.frame->planes[0][32]==27&&result.frame->planes[1][0]==99&&result.frame->planes[1][32]==83,"independent seeded continuation across both groups");
     auto state=r.owner->snapshot(f);check(state&&state->raw_decoded_frames==64&&state->published_frames==33&&state->histories[0].first==42&&state->histories[1].first==68&&state->carry.empty(),"full final group history advances beyond trimmed output");
     check(r.resolver->calls==1&&r.owner->finish(f).status==MovieAdxStreamStatus::ended&&r.owner->feed({},f).status==MovieAdxStreamStatus::ended,"one profile admission and stable encoded end");auto held=std::move(*result.frame);r.owner.reset();check(held.planes[0][32]==27,"PCM owns lifetime after stream destruction");}
    {auto all=header();append(all,group());append(all,group());append(all,marker());
     for(std::size_t split=0;split<=all.size();++split){Rig r;std::vector<std::int16_t> joined;auto a=r.owner->feed(std::span(all).first(split),f);check(a.status!=MovieAdxStreamStatus::error,"first arbitrary split");if(a.frame)joined.insert(joined.end(),a.frame->planes[0].begin(),a.frame->planes[0].end());auto b=r.owner->feed(std::span(all).subspan(split),f);check(b.status!=MovieAdxStreamStatus::error,"second arbitrary split");if(b.frame){check(b.frame->first_source_frame==joined.size(),"chunk first index continues");joined.insert(joined.end(),b.frame->planes[0].begin(),b.frame->planes[0].end());}check(joined.size()==33&&joined.back()==27&&r.owner->finish(f).status==MovieAdxStreamStatus::ended&&r.resolver->calls==1,"all header/group/marker split positions exact");}
     Rig bytewise;std::uint64_t published=0;for(auto byte:all){auto v=bytewise.owner->feed(std::span(&byte,1),f);check(v.status!=MovieAdxStreamStatus::error,"bytewise packet staging");if(v.frame){check(v.frame->first_source_frame==published,"bytewise source index");published+=v.frame->planes[0].size();}}check(published==33&&bytewise.owner->finish(f).status==MovieAdxStreamStatus::ended,"bytewise complete header/payload/control");}
    {Rig r(header(1));check(r.resolver->calls==1,"complete extradata admitted once");auto packet=group();append(packet,marker(16));auto out=r.owner->feed(packet,f);check(out.status==MovieAdxStreamStatus::frame&&out.frame->planes[0].size()==1&&out.frame->raw_decoded_frames==32&&out.encoded_ended&&r.resolver->calls==1,"extradata plus data only, minimal marker and single sample trim");}
    {Rig r(header(32));auto frame=r.owner->feed(group(),f);check(frame.status==MovieAdxStreamStatus::frame&&!frame.encoded_ended,"count alone does not imply end");auto before=r.owner->snapshot(f);check(r.owner->finish(f).status==MovieAdxStreamStatus::error&&f.kind==MovieAdxStreamError::incomplete,"missing marker finish fails");auto after=r.owner->snapshot(f);check(before&&after&&after->failed,"terminal snapshot remains observable");preserved(*before,*after);}
    for(int which=0;which<8;++which){Rig r;auto good_header=header(33);check(r.owner->feed(good_header,f).header_available,"negative setup header");auto before=r.owner->snapshot(f);std::vector<std::uint8_t> bad;
      if(which==0)bad=marker();
      if(which==1){bad=group();append(bad,marker());}
      if(which==2)bad=header();
      if(which==3){bad=group();bad[0]=0x80;bad[1]=2;}
      if(which==4){bad=group();append(bad,group());append(bad,group());}
      if(which>=5){bad=group();append(bad,group());auto tail=marker();if(which==5)put16(tail,2,1);if(which==6)tail[1]=2;append(bad,tail);if(which==7)bad.push_back(0);}
      auto result=r.owner->feed(bad,f);check(result.status==MovieAdxStreamStatus::error&&!result.frame,"invalid control/group has no partial PCM publication");
      if(which<=1)check(f.kind==MovieAdxStreamError::premature_marker,"premature exact8001 marker");if(which==2)check(f.kind==MovieAdxStreamError::duplicate_header,"duplicate first header not searched away");
      auto after=r.owner->snapshot(f);check(before&&after&&after->failed,"failed owner state observed");preserved(*before,*after);check(r.owner->feed({},f).status==MovieAdxStreamStatus::error&&f.kind==MovieAdxStreamError::failed_owner,"terminal no retry/default");}
    {Rig r(header(32));auto p=group();append(p,marker());check(r.owner->feed(p,f).encoded_ended,"postmarker setup");auto before=r.owner->snapshot(f);std::uint8_t extra=0;check(r.owner->feed(std::span(&extra,1),f).status==MovieAdxStreamStatus::error&&f.kind==MovieAdxStreamError::trailing_data,"later packet trailing data rejects");auto after=r.owner->snapshot(f);preserved(*before,*after);}
    {auto extra=header();extra.push_back(0);check(!NativeMovieAdxStream::create(extra,std::make_unique<Resolver>(),f)&&f.kind==MovieAdxStreamError::invalid_input,"extradata never discards trailing group/payload");extra=header();extra.pop_back();check(!NativeMovieAdxStream::create(extra,std::make_unique<Resolver>(),f)&&f.kind==MovieAdxStreamError::header,"incomplete extradata rejects");check(!NativeMovieAdxStream::create({},nullptr,f)&&f.kind==MovieAdxStreamError::profile,"no default resolver");}
    for(int mode=1;mode<=7;++mode){Rig r;r.resolver->mode=mode;auto before=r.owner->snapshot(f);check(r.owner->feed(header(),f).status==MovieAdxStreamStatus::error,"resolver error/mismatch named");auto after=r.owner->snapshot(f);preserved(*before,*after);check(!after->header&&after->failed,"no partially admitted header/profile");}
    {Rig r;r.resolver->callback_owner=r.owner.get();check(r.owner->feed(header(),f).header_available&&r.resolver->reentry_rejected,"resolver callback reentry rejected without poisoning outer admission");MovieAdxStreamFailure nested;bool okay=true;std::thread t([&]{okay=bool(r.owner->snapshot(nested));});t.join();check(!okay&&nested.kind==MovieAdxStreamError::wrong_thread&&r.owner->snapshot(f).has_value(),"wrong thread check does not poison owner");}
    {Rig r;auto bytes=header();auto before=r.owner->snapshot(f);fail_next=true;check(r.owner->feed(bytes,f).status==MovieAdxStreamStatus::error&&f.kind==MovieAdxStreamError::allocation,"genuine feed allocation failure");auto after=r.owner->snapshot(f);preserved(*before,*after);}
    {Rig r;auto h=header();h[0]=0;check(r.owner->feed(h,f).status==MovieAdxStreamStatus::error&&f.kind==MovieAdxStreamError::header,"invalid header no search");}
    {Rig r;auto h=header();check(r.owner->feed(std::span(h).first(17),f).status==MovieAdxStreamStatus::need_more,"partial header retained");auto before=r.owner->snapshot(f);check(r.owner->finish(f).status==MovieAdxStreamStatus::error&&f.kind==MovieAdxStreamError::incomplete,"partial header finish named");auto after=r.owner->snapshot(f);preserved(*before,*after);}
    {Rig r(header(1));auto p=group();auto end=marker(65539);append(p,end);auto out=r.owner->feed(p,f);check(out.status==MovieAdxStreamStatus::frame&&out.encoded_ended&&out.completed_marker==end&&out.frame->planes[0].size()==1,"full serialized marker extent owned, no fixed18 marker cap");}
    for(std::size_t n=0;n<18;++n){Rig r(header(32));auto p=group();auto end=marker();p.insert(p.end(),end.begin(),end.begin()+n);auto result=r.owner->feed(p,f);check(result.status!=MovieAdxStreamStatus::error&&!result.encoded_ended,"partial end marker does not complete");check(r.owner->finish(f).status==MovieAdxStreamStatus::error&&f.kind==MovieAdxStreamError::incomplete,"all truncated markers finish named");}
    std::puts("PASS owned native ADX stream staging/trim/marker transactions; no retail coefficient or movie admission");check(!std::fflush(stdout)&&!std::ferror(stdout),"complete stream output");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL ADX stream %s\n",e.what());return 1;}}
