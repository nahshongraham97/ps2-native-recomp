#include "mka/movie_pes_index.hpp"
#include "native_test_sha256.hpp"
#include <fstream>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <sstream>
#include <array>
#include <stdexcept>
using namespace mka::video;
using Bytes=std::vector<std::uint8_t>;
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
Bytes pack{0,0,1,0xba,0x21,0,1,0,1,0x80,0,1};
Bytes ts(std::uint64_t t,unsigned prefix){return {std::uint8_t(prefix*16|((t>>29)&14)|1),std::uint8_t(t>>22),std::uint8_t((t>>14)&254|1),std::uint8_t(t>>7),std::uint8_t((t<<1)&254|1)};}
void pes(Bytes& file,unsigned id,Bytes header,Bytes data){auto n=header.size()+data.size();file.insert(file.end(),{0,0,1,std::uint8_t(id),std::uint8_t(n>>8),std::uint8_t(n)});file.insert(file.end(),header.begin(),header.end());file.insert(file.end(),data.begin(),data.end());}
auto store(const std::filesystem::path& root,const std::vector<MovieAssetCatalogEntry>& catalog){MovieAssetFailure f;auto s=NativeMovieAssetStore::create(root,catalog,f);check(bool(s),"asset store");return std::move(*s);}
void write(const std::filesystem::path& p,const Bytes& b){std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),b.size());check(bool(f),"write fixture");}
void synthetic(){auto parent=std::filesystem::canonical(std::filesystem::temp_directory_path());auto root=parent/("mka-pes-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));check(std::filesystem::create_directory(root),"exclusive fixture");struct Cleanup{std::filesystem::path p,parent;~Cleanup(){std::error_code e;auto actual=std::filesystem::canonical(p,e);if(!e&&actual==p&&actual.is_absolute()&&actual.parent_path()==parent)std::filesystem::remove_all(actual,e);}} cleanup{root,parent};
 Bytes f=pack;auto pts=ts((1ull<<33)-1,3),dts=ts(123456,1);pts.insert(pts.end(),dts.begin(),dts.end());pes(f,0xe0,pts,{0,0});pes(f,0xe0,{0x0f},{1,0,8,9});pes(f,0xe1,ts(4,2),{7});f.insert(f.end(),{0,0,1,0xb9,255});write(root/"test.sfd",f);
 MoviePesFailure e;MovieAssetFailure a;std::optional<NativeMovieAssetStore> s=store(root,{{"test","test.sfd"}});auto asset=s->open("test",a);check(bool(asset),"open");check(asset->seek(9,mka::content::SeekOrigin::begin,a),"nonzero input cursor");auto index=NativeMoviePesIndex::build(std::move(*asset),e);check(bool(index),"build synthetic");auto file_cursor=index->file_reader(e);check(bool(file_cursor),"same-open file cursor");
 check(file_cursor->length(e)==f.size(),"file cursor indexed length");
 std::array<std::uint8_t,12> raw{};check(file_cursor->read(raw.data(),raw.size(),e).bytes==raw.size()&&std::equal(raw.begin(),raw.end(),pack.begin()),"same-open file header bytes");
 check(file_cursor->seek(-2,mka::content::SeekOrigin::end,e),"file cursor native end-relative seek");
 auto before=file_cursor->position();check(!file_cursor->seek(INT64_MIN,mka::content::SeekOrigin::current,e)&&file_cursor->position()==before,"negative overflow seek transactional");
 std::array<std::uint8_t,8> tail{};auto short_read=file_cursor->read(tail.data(),tail.size(),e);check(short_read.bytes==2&&short_read.eof&&!short_read.error,"normal source EOF shortread");
 check(file_cursor->seek(2,mka::content::SeekOrigin::current,e),"past EOF logical seek");check(file_cursor->read(tail.data(),1,e).eof,"past EOF read");
 check(file_cursor->seek(4,mka::content::SeekOrigin::begin,e),"source cursor explicit position");
 auto spans=index->streams();check(spans.size()==2&&spans[0].bytes==6,"dynamic streams");auto v=index->lookup_picture_prefix(0xe0,0,e);check(v&&v->pts33==(1ull<<33)-1&&v->dts33==123456&&v->quarter_pts()==(((1ull<<33)-1)>>2),"raw 33 timestamps");check(!index->lookup_picture_prefix(0xe0,1,e)&&e.kind==MoviePesError::bad_picture_prefix,"nonprefix rejected");v=index->lookup(0xe0,2,e);check(v&&!v->pts33,"missing timestamp never inherited");check(!index->lookup(0xe0,6,e)&&e.kind==MoviePesError::range,"half-open EOF");check(!index->lookup(0xe2,0,e)&&e.kind==MoviePesError::invalid_stream,"absent stream");
 std::array<std::uint8_t,6>b{};check(index->read_range(0xe0,0,b,e)&&b==std::array<std::uint8_t,6>{0,0,1,0,8,9},"cross PES exact bytes");auto r=index->reader(0xe0,e);check(bool(r),"reader");auto wrong=b;wrong[3]=99;check(!r->validate_next(wrong,e)&&r->position()==0&&e.kind==MoviePesError::byte_mismatch,"mismatch transactional");check(file_cursor->position()==4,"index reads do not alter AVIO cursor");
 index.reset();s.reset();auto one=file_cursor->read(tail.data(),1,e);check(one.bytes==1&&tail[0]==f[4],"same actual file survives index and store release");
 auto moved=std::move(*file_cursor);check(!file_cursor->length(e)&&e.kind==MoviePesError::moved,"moved source cursor rejected");
 check(r->validate_next(b,e)&&r->finished(),"owned file survives index/store source move");check(!r->validate_next(std::span(b.data(),1),e)&&r->position()==6,"past EOF rejected");
 s=store(root,{{"test","test.sfd"}});
 auto reject=[&](Bytes bytes,MoviePesError kind){write(root/"test.sfd",bytes);auto x=s->open("test",a);check(bool(x),"negative open");auto n=NativeMoviePesIndex::build(std::move(*x),e);check(!n&&e.kind==kind,"named malformed rejection");};
 auto bad=f;bad[12]=7;reject(bad,MoviePesError::bad_prefix);bad=f;bad[18]&=254;reject(bad,MoviePesError::bad_timestamp);bad=f;bad[18]=0x80;reject(bad,MoviePesError::unsupported_header);bad=f;bad[16]=bad[17]=0;reject(bad,MoviePesError::unsupported_zero_length);bad=f;bad.pop_back();bad.pop_back();reject(bad,MoviePesError::truncated);bad=f;bad.back()=0;reject(bad,MoviePesError::bad_tail);reject(pack,MoviePesError::missing_end);
 // A subsequent physical truncation cannot publish partial bytes or advance validation.
 write(root/"test.sfd",f);auto current=s->open("test",a);auto retained=NativeMoviePesIndex::build(std::move(*current),e);check(bool(retained),"truncation index");
 auto verify=retained->reader(0xe0,e);std::filesystem::resize_file(root/"test.sfd",12);
 std::array<std::uint8_t,6> unchanged{4,4,4,4,4,4};check(!retained->read_range(0xe0,0,unchanged,e)&&unchanged==std::array<std::uint8_t,6>{4,4,4,4,4,4},"failed read publishes nothing");check(!verify->validate_next(b,e)&&verify->position()==0,"truncation validation transactional");
 auto truncated_cursor=retained->file_reader(e);check(truncated_cursor->seek(12,mka::content::SeekOrigin::begin,e),"truncated source cursor");
 std::array<std::uint8_t,4> preserve{9,9,9,9};auto rr=truncated_cursor->read(preserve.data(),preserve.size(),e);check(rr.error&&rr.bytes==0&&truncated_cursor->position()==12&&preserve==std::array<std::uint8_t,4>{9,9,9,9},"source cursor physical truncation fail atomic");
 std::cout<<"SYNTHETIC PASS\n";
}
std::string optional(std::optional<std::uint64_t> x){return x?std::to_string(*x):"null";}
void corpus(const std::filesystem::path& root){std::vector<MovieAssetCatalogEntry> catalog;for(const auto& p:std::filesystem::recursive_directory_iterator(root))if(p.is_regular_file()&&(p.path().extension()==".SFD"||p.path().extension()==".sfd")){auto rel=p.path().lexically_relative(root).generic_string();catalog.push_back({rel,rel});}std::sort(catalog.begin(),catalog.end(),[](const auto&a,const auto&b){return a.logical_utf8<b.logical_utf8;});auto source=store(root,catalog);
 for(const auto& entry:catalog){MovieAssetFailure a;auto asset=source.open(entry.logical_utf8,a);check(bool(asset),"corpus open");NativeTestSha256 sourcehash;std::array<std::uint8_t,131072> buffer{};for(;;){auto read=asset->read(buffer.data(),buffer.size(),a);check(!read.error,"source hash read");sourcehash.add(buffer.data(),read.bytes);if(read.eof)break;}MoviePesFailure e;auto index=NativeMoviePesIndex::build(std::move(*asset),e);if(!index)throw std::runtime_error("corpus index error "+std::to_string(int(e.kind))+" at "+std::to_string(e.offset)+" "+entry.logical_utf8);auto streams=index->streams();check(streams.size()==1,"oracle corpus single video stream");auto& stream=streams[0];std::string rows="[";bool comma=false;for(const auto& p:stream.spans){if(comma)rows+=',';comma=true;rows+="["+std::to_string(p.packet_offset)+","+std::to_string(p.payload_offset)+","+std::to_string(p.packet_end)+","+std::to_string(p.es_begin)+","+std::to_string(p.es_end)+","+optional(p.pts33)+","+optional(p.dts33)+"]";}rows+=']';NativeTestSha256 rh;rh.add(rows.data(),rows.size());NativeTestSha256 eshash;std::uint64_t at=0,pictures=0,absent=0,cross=0;std::uint32_t window=0xffffffff;std::string mappings="[";comma=false;
 while(at<stream.bytes){auto n=std::size_t(std::min<std::uint64_t>(buffer.size(),stream.bytes-at));check(index->read_range(stream.stream_id,at,std::span(buffer.data(),n),e),"corpus ES read");eshash.add(buffer.data(),n);for(std::size_t i=0;i<n;++i){window=(window<<8)|buffer[i];if(window==0x100&&at+i>=3){auto pos=at+i-3;auto p=index->lookup_picture_prefix(stream.stream_id,pos,e);check(bool(p),"prefix FIRSTBYTE span");auto it=std::upper_bound(stream.spans.begin(),stream.spans.end(),pos,[](auto p,const auto& v){return p<v.es_end;});auto ordinal=it-stream.spans.begin();if(comma)mappings+=',';comma=true;mappings+="["+std::to_string(pos)+","+std::to_string(ordinal)+","+optional(p->pts33)+","+optional(p->quarter_pts())+"]";++pictures;if(!p->pts33)++absent;if(pos+4>p->es_end)++cross;}}at+=n;}
 mappings+=']';NativeTestSha256 mh;mh.add(mappings.data(),mappings.size());auto reader=index->reader(stream.stream_id,e);std::array<std::uint8_t,37> first{};auto n=std::size_t(std::min<std::uint64_t>(37,stream.bytes));check(index->read_range(stream.stream_id,0,std::span(first.data(),n),e)&&reader->validate_next(std::span(first.data(),n),e),"corpus exact advancing validation");
 std::cout<<entry.logical_utf8<<'\t'<<stream.spans.size()<<'\t'<<stream.bytes<<'\t'<<sourcehash.finish()<<'\t'<<eshash.finish()<<'\t'<<rh.finish()<<'\t'<<pictures<<'\t'<<mh.finish()<<'\t'<<absent<<'\t'<<cross<<'\n';
 }
}
int main(int argc,char**argv){try{if(argc==3&&std::string(argv[1])=="--corpus")corpus(std::filesystem::path(argv[2]));else if(argc==1)synthetic();else throw std::runtime_error("usage: test_native_movie_pes_index [--corpus ROOT]");return 0;}catch(const std::exception& e){std::cerr<<"PES FAIL: "<<e.what()<<'\n';return 1;}}
