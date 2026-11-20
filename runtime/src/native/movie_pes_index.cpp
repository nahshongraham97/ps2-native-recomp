#include "mka/movie_pes_index.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
namespace mka::video {
struct MoviePesBacking {NativeMovieAsset asset; std::mutex mutex;std::uint64_t file_bytes=0;std::vector<MoviePesStream> streams;std::optional<MovieProgramEndObservation> program_end;explicit MoviePesBacking(NativeMovieAsset&& a):asset(std::move(a)){};};
namespace {
bool fail(MoviePesFailure& f,MoviePesError k,std::uint64_t p){f.kind=k;f.offset=p;return false;}
struct Input {
 NativeMovieAsset& a;MoviePesFailure& f;std::uint64_t size=0,pos=0;
 bool take(void* p,std::size_t n){if(pos>size||n>size-pos)return fail(f,MoviePesError::truncated,pos);auto r=a.read(p,n,f.asset);if(r.error)return fail(f,MoviePesError::io,pos);if(r.bytes!=n)return fail(f,MoviePesError::truncated,pos);pos+=n;return true;}
 bool seek(std::uint64_t p){if(p>size||p>INT64_MAX)return fail(f,MoviePesError::overflow,p);if(!a.seek(static_cast<std::int64_t>(p),content::SeekOrigin::begin,f.asset))return fail(f,MoviePesError::io,p);pos=p;return true;}
};
bool stamp(Input& in,std::uint64_t end,unsigned prefix,std::optional<std::uint64_t>& out){
 std::array<unsigned char,5>b{};if(in.pos>end||end-in.pos<5)return fail(in.f,MoviePesError::truncated,in.pos);if(!in.take(b.data(),5))return false;
 if((b[0]>>4)!=prefix||!(b[0]&1)||!(b[2]&1)||!(b[4]&1))return fail(in.f,MoviePesError::bad_timestamp,in.pos-5);
 out=(std::uint64_t(b[0]&14)<<29)|(std::uint64_t((b[1]<<8)|b[2])>>1<<15)|(std::uint64_t((b[3]<<8)|b[4])>>1);return true;
}
bool scan(MoviePesBacking& state,std::uint64_t size,MoviePesFailure& f){
 Input in{state.asset,f,size,0};bool first=true;
 while(in.pos<size){auto start=in.pos;std::array<unsigned char,4> c{};if(!in.take(c.data(),4))return false;
 if(c[0]||c[1]||c[2]!=1)return fail(f,MoviePesError::bad_prefix,start);
 if(first&&c[3]!=0xba)return fail(f,MoviePesError::missing_pack,start);first=false;
 if(c[3]==0xba){std::array<unsigned char,8>b{};if(!in.take(b.data(),8))return false;
 unsigned bit=0;auto get=[&](unsigned n){unsigned v=0;while(n--){v=(v<<1)|((b[bit/8]>>(7-bit%8))&1);++bit;}return v;};
 if(get(4)!=2)return fail(f,MoviePesError::unsupported_header,start);get(3);if(get(1)!=1)return fail(f,MoviePesError::bad_pack,start);get(15);if(get(1)!=1)return fail(f,MoviePesError::bad_pack,start);get(15);if(get(1)!=1||get(1)!=1)return fail(f,MoviePesError::bad_pack,start);get(22);if(get(1)!=1)return fail(f,MoviePesError::bad_pack,start);
 }else if(c[3]==0xb9){const auto padding=size-in.pos;std::array<unsigned char,4096>b{};while(in.pos<size){auto n=static_cast<std::size_t>(std::min<std::uint64_t>(b.size(),size-in.pos));auto at=in.pos;if(!in.take(b.data(),n))return false;for(std::size_t i=0;i<n;++i)if(b[i]!=255)return fail(f,MoviePesError::bad_tail,at+i);}state.program_end=MovieProgramEndObservation{start,4,padding};return true;
 }else if(c[3]==0xbb||c[3]==0xbe||c[3]==0xbf||(c[3]>=0xc0&&c[3]<=0xef)){
 std::array<unsigned char,2>b{};if(!in.take(b.data(),2))return false;auto n=unsigned(b[0])*256+b[1];if(!n)return fail(f,MoviePesError::unsupported_zero_length,start);if(c[3]==0xbb&&n<6)return fail(f,MoviePesError::bad_system_header,start);if(n>size-in.pos)return fail(f,MoviePesError::truncated,start);auto end=in.pos+n;
 if(c[3]>=0xc0&&c[3]<=0xef){auto it=std::find_if(state.streams.begin(),state.streams.end(),[&](const auto& s){return s.stream_id==c[3];});if(it==state.streams.end()){state.streams.push_back({c[3],0,{}});it=state.streams.end()-1;}
 MovieVideoPesSpan span;span.stream_id=c[3];span.packet_offset=start;span.packet_end=end;
 unsigned char flag=0;do{if(in.pos>=end)return fail(f,MoviePesError::truncated,in.pos);if(!in.take(&flag,1))return false;}while(flag==255);
 if((flag&0xc0)==0x40){unsigned char low;if(in.pos>=end||!in.take(&low,1))return fail(f,MoviePesError::truncated,in.pos);if(in.pos>=end||!in.take(&flag,1))return fail(f,MoviePesError::truncated,in.pos);}
 // Timestamp first byte has already been consumed; restore to its exact location.
 if((flag>>4)==2||(flag>>4)==3){if(!in.seek(in.pos-1))return false;if(!stamp(in,end,flag>>4,span.pts33))return false;if((flag>>4)==3&&!stamp(in,end,1,span.dts33))return false;}
 else if(flag!=15)return fail(f,MoviePesError::unsupported_header,in.pos-1);
 span.payload_offset=in.pos;span.es_begin=it->bytes;if(end-in.pos>UINT64_MAX-it->bytes)return fail(f,MoviePesError::overflow,in.pos);it->bytes+=end-in.pos;span.es_end=it->bytes;it->spans.push_back(span);
 }if(!in.seek(end))return false;
 }else return fail(f,MoviePesError::unsupported_packet,start);
 }return fail(f,MoviePesError::missing_end,in.pos);
}
const MoviePesStream* stream(const MoviePesBacking& b,std::uint8_t id){auto it=std::find_if(b.streams.begin(),b.streams.end(),[&](const auto& s){return s.stream_id==id;});return it==b.streams.end()?nullptr:&*it;}
const MovieVideoPesSpan* find(const MoviePesStream& s,std::uint64_t p){auto it=std::upper_bound(s.spans.begin(),s.spans.end(),p,[](auto p,const auto& v){return p<v.es_end;});return it==s.spans.end()?nullptr:&*it;}
bool restore(MoviePesBacking& b,std::uint64_t p,MoviePesFailure& f){MovieAssetFailure a;if(p>INT64_MAX||!b.asset.seek(static_cast<std::int64_t>(p),content::SeekOrigin::begin,a)){auto old=f.kind;f={MoviePesError::cursor_restore,p,a,old};return false;}return true;}
bool read(MoviePesBacking& b,const MoviePesStream& s,std::uint64_t p,std::span<std::uint8_t> out,MoviePesFailure& f){
 if(p>s.bytes||out.size()>s.bytes-p)return fail(f,MoviePesError::range,p);
 auto original=b.asset.position(f.asset);if(!original)return fail(f,MoviePesError::io,p);if(*original>INT64_MAX)return fail(f,MoviePesError::overflow,*original);bool ok=true;std::size_t done=0;
 while(done<out.size()){auto* v=find(s,p);if(!v){ok=fail(f,MoviePesError::range,p);break;}auto at=v->payload_offset+(p-v->es_begin);if(at>INT64_MAX){ok=fail(f,MoviePesError::overflow,at);break;}auto n=static_cast<std::size_t>(std::min<std::uint64_t>(v->es_end-p,out.size()-done));if(!b.asset.seek(static_cast<std::int64_t>(at),content::SeekOrigin::begin,f.asset)){ok=fail(f,MoviePesError::io,at);break;}auto r=b.asset.read(out.data()+done,n,f.asset);if(r.error||r.bytes!=n){ok=fail(f,r.error?MoviePesError::io:MoviePesError::truncated,at);break;}done+=n;p+=n;}
 return restore(b,*original,f)&&ok;
}
}
std::optional<NativeMoviePesIndex> NativeMoviePesIndex::build(NativeMovieAsset&& asset,MoviePesFailure& f){f={};try{auto b=std::make_shared<MoviePesBacking>(std::move(asset));auto p=b->asset.position(f.asset);if(!p){fail(f,MoviePesError::io,0);return {};}if(*p>INT64_MAX){fail(f,MoviePesError::overflow,*p);return {};}bool ok=false;try{auto size=b->asset.length(f.asset);if(!size)fail(f,MoviePesError::io,0);else if(!b->asset.seek(0,content::SeekOrigin::begin,f.asset))fail(f,MoviePesError::io,0);else {b->file_bytes=*size;ok=scan(*b,*size,f);}}catch(const std::bad_alloc&){fail(f,MoviePesError::allocation,0);}catch(const std::length_error&){fail(f,MoviePesError::overflow,0);}if(!restore(*b,*p,f)||!ok)return {};return NativeMoviePesIndex(std::move(b));}catch(const std::bad_alloc&){fail(f,MoviePesError::allocation,0);return {};}}
std::optional<MovieSourceCursor> NativeMoviePesIndex::file_reader(MoviePesFailure& f)const {f={};if(!state_){fail(f,MoviePesError::moved,0);return {};}return MovieSourceCursor(state_);}
std::optional<std::uint64_t> MovieSourceCursor::length(MoviePesFailure& f)const {f={};if(!state_){fail(f,MoviePesError::moved,position_);return {};}return state_->file_bytes;}
bool MovieSourceCursor::seek(std::int64_t offset,content::SeekOrigin origin,MoviePesFailure& f){
 f={};if(!state_)return fail(f,MoviePesError::moved,position_);
 std::uint64_t base=0;
 switch(origin){case content::SeekOrigin::begin:break;case content::SeekOrigin::current:base=position_;break;case content::SeekOrigin::end:base=state_->file_bytes;break;default:return fail(f,MoviePesError::range,position_);}
 if(base>INT64_MAX)return fail(f,MoviePesError::overflow,base);
 std::uint64_t next=0;
 if(offset<0){auto magnitude=std::uint64_t(-(offset+1))+1;if(magnitude>base)return fail(f,MoviePesError::range,position_);next=base-magnitude;}
 else {if(std::uint64_t(offset)>std::uint64_t(INT64_MAX)-base)return fail(f,MoviePesError::overflow,position_);next=base+std::uint64_t(offset);}
 if(next>INT64_MAX)return fail(f,MoviePesError::overflow,next);position_=next;return true;
}
content::FileRead MovieSourceCursor::read(void* data,std::size_t size,MoviePesFailure& f){
 f={};const auto error=[&](){return content::FileRead{0,false,f.asset.host_error?f.asset.host_error:std::make_error_code(std::errc::io_error)};};
 if(!state_){fail(f,MoviePesError::moved,position_);return error();}
 if(size&&!data){fail(f,MoviePesError::range,position_);return error();}
 if(position_>=state_->file_bytes)return {0,size!=0,{}};
 const auto n=static_cast<std::size_t>(std::min<std::uint64_t>(size,state_->file_bytes-position_));if(!n)return {};
 std::lock_guard lock(state_->mutex);auto original=state_->asset.position(f.asset);if(!original){fail(f,MoviePesError::io,position_);return error();}
 if(*original>INT64_MAX){fail(f,MoviePesError::overflow,*original);return error();}
 bool ok=false;
 try {std::vector<std::uint8_t> bytes(n);
 if(!state_->asset.seek(static_cast<std::int64_t>(position_),content::SeekOrigin::begin,f.asset))fail(f,MoviePesError::io,position_);
 else {auto got=state_->asset.read(bytes.data(),n,f.asset);if(got.error||got.bytes!=n)fail(f,got.error?MoviePesError::io:MoviePesError::truncated,position_);else ok=true;}
 if(!restore(*state_,*original,f)||!ok)return error();
 std::copy(bytes.begin(),bytes.end(),static_cast<std::uint8_t*>(data));position_+=n;return {n,n<size,{}};
 }catch(const std::bad_alloc&){fail(f,MoviePesError::allocation,position_);}catch(const std::length_error&){fail(f,MoviePesError::overflow,position_);}
 restore(*state_,*original,f);return error();
}
std::optional<MovieProgramEndObservation> NativeMoviePesIndex::program_end()const noexcept{return state_?state_->program_end:std::nullopt;}
std::vector<MoviePesStream> NativeMoviePesIndex::streams()const{std::vector<MoviePesStream> out;if(state_)for(const auto& s:state_->streams)if(s.stream_id>=0xe0&&s.stream_id<=0xef)out.push_back(s);return out;}
std::vector<MoviePesStream> NativeMoviePesIndex::audio_streams()const{std::vector<MoviePesStream> out;if(state_)for(const auto& s:state_->streams)if(s.stream_id>=0xc0&&s.stream_id<=0xdf)out.push_back(s);return out;}
std::optional<MovieVideoPesSpan> NativeMoviePesIndex::lookup(std::uint8_t id,std::uint64_t p,MoviePesFailure& f)const{f={};if(!state_){fail(f,MoviePesError::moved,p);return {};}auto* s=stream(*state_,id);if(!s){fail(f,MoviePesError::invalid_stream,p);return {};}auto* v=find(*s,p);if(!v){fail(f,MoviePesError::range,p);return {};}return *v;}
std::optional<MovieVideoPesSpan> NativeMoviePesIndex::lookup_picture_prefix(std::uint8_t id,std::uint64_t p,MoviePesFailure& f)const {
 f={};if(!state_){fail(f,MoviePesError::moved,p);return {};}if(id<0xe0||id>0xef){fail(f,MoviePesError::invalid_stream,p);return {};}
 std::array<std::uint8_t,4> bytes{};if(!read_range(id,p,bytes,f))return {};
 if(bytes!=std::array<std::uint8_t,4>{0,0,1,0}){fail(f,MoviePesError::bad_picture_prefix,p);return {};}
 return lookup(id,p,f);
}
bool NativeMoviePesIndex::read_range(std::uint8_t id,std::uint64_t p,std::span<std::uint8_t> out,MoviePesFailure& f)const{f={};if(!state_)return fail(f,MoviePesError::moved,p);auto* s=stream(*state_,id);if(!s)return fail(f,MoviePesError::invalid_stream,p);std::lock_guard lock(state_->mutex);
 if(p>s->bytes||out.size()>s->bytes-p)return fail(f,MoviePesError::range,p);
 try {std::vector<std::uint8_t> temporary(out.size());if(!read(*state_,*s,p,temporary,f))return false;std::copy(temporary.begin(),temporary.end(),out.begin());return true;}
 catch(const std::bad_alloc&){return fail(f,MoviePesError::allocation,p);}catch(const std::length_error&){return fail(f,MoviePesError::overflow,p);}}
std::optional<MovieEsReader> NativeMoviePesIndex::reader(std::uint8_t id,MoviePesFailure& f)const{f={};if(!state_){fail(f,MoviePesError::moved,0);return {};}if(!stream(*state_,id)){fail(f,MoviePesError::invalid_stream,0);return {};}return MovieEsReader(state_,id);}
bool MovieEsReader::validate_next(std::span<const std::uint8_t> bytes,MoviePesFailure& f){f={};if(!state_)return fail(f,MoviePesError::moved,position_);auto* s=stream(*state_,stream_);std::lock_guard lock(state_->mutex);if(position_>s->bytes||bytes.size()>s->bytes-position_)return fail(f,MoviePesError::range,position_);std::array<std::uint8_t,4096>b{};std::size_t done=0;while(done<bytes.size()){auto n=std::min(b.size(),bytes.size()-done);if(!read(*state_,*s,position_+done,std::span(b.data(),n),f))return false;auto mismatch=std::mismatch(b.begin(),b.begin()+n,bytes.begin()+done);if(mismatch.first!=b.begin()+n)return fail(f,MoviePesError::byte_mismatch,position_+done+std::size_t(mismatch.first-b.begin()));done+=n;}position_+=bytes.size();return true;}
bool MovieEsReader::finished()const noexcept{auto* s=state_?stream(*state_,stream_):nullptr;return s&&position_==s->bytes;}
}
