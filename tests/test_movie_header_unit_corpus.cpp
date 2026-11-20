#include "mka/movie_header_unit.hpp"
#include "mka/movie_pes_index.hpp"
#include "mka/movie_prefix_events.hpp"
#include "native_test_sha256.hpp"
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
using namespace mka::video;
namespace {
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
std::string digest(std::span<const std::uint8_t> bytes){NativeTestSha256 h;h.add(bytes.data(),bytes.size());return h.finish();}
std::string json_string(std::string_view v){std::string out="\"";for(unsigned char c:v){if(c=='"'||c=='\\'){out+='\\';out+=char(c);}else if(c<32){char b[7];std::snprintf(b,sizeof b,"\\u%04x",unsigned(c));out+=b;}else out+=char(c);}out+='"';return out;}
struct Array {
 std::string s="[";bool first=true;
 void raw(std::string_view v){if(!first)s+=',';first=false;s+=v;}
 void number(std::uint64_t n){raw(std::to_string(n));}
 void flag(bool b){number(b?1:0);}
 void hash(std::span<const std::uint8_t> b){raw(json_string(digest(b)));}
 std::string finish(){return s+"]";}
};
struct Fields {std::string kind,wire;};
Fields fields(const MovieHeaderUnitPayload& payload){return std::visit([](const auto& p)->Fields{
 using T=std::decay_t<decltype(p)>;Array a;std::string kind;
 if constexpr(std::is_same_v<T,MovieSequenceHeader>){kind="sequence";
  a.number(p.width);a.number(p.height);a.number(p.aspect_ratio_information);a.number(p.frame_rate_code);a.number(p.bit_rate_value);a.number(p.vbv_buffer_size);a.flag(p.constrained_parameters);
  if(p.intra_matrix)a.hash(*p.intra_matrix);else a.raw("null");if(p.non_intra_matrix)a.hash(*p.non_intra_matrix);else a.raw("null");
 }else if constexpr(std::is_same_v<T,MovieSequenceExtension>){kind="sequence_extension";
  a.number(p.profile_and_level);a.flag(p.progressive_sequence);a.number(p.chroma_format);a.number(p.horizontal_size_extension);a.number(p.vertical_size_extension);a.number(p.bit_rate_extension);a.number(p.vbv_buffer_size_extension);a.flag(p.low_delay);a.number(p.frame_rate_extension_n);a.number(p.frame_rate_extension_d);
 }else if constexpr(std::is_same_v<T,MovieDisplayExtension>){kind="display_extension";
  a.number(p.video_format);if(p.color){Array c;c.number(p.color->primaries);c.number(p.color->transfer);c.number(p.color->matrix);a.raw(c.finish());}else a.raw("null");a.number(p.horizontal_size);a.number(p.vertical_size);
 }else if constexpr(std::is_same_v<T,MoviePictureCodingExtension>){kind="picture_extension";
  Array codes;for(auto c:p.f_code)codes.number(c);a.raw(codes.finish());a.number(p.intra_dc_precision);a.number(p.picture_structure);a.flag(p.top_field_first);a.flag(p.frame_pred_frame_dct);a.flag(p.concealment_motion_vectors);a.flag(p.q_scale_type);a.flag(p.intra_vlc_format);a.flag(p.alternate_scan);a.flag(p.repeat_first_field);a.flag(p.chroma_420_type);a.flag(p.progressive_frame);if(p.composite_display){const auto& c=*p.composite_display;a.number((std::uint32_t(c.v_axis)<<19)|(std::uint32_t(c.field_sequence)<<16)|(std::uint32_t(c.sub_carrier)<<15)|(std::uint32_t(c.burst_amplitude)<<8)|c.sub_carrier_phase);}else a.raw("null");
 }else if constexpr(std::is_same_v<T,MovieGopHeader>){kind="gop";
  a.number(p.raw_time_code);a.flag(p.drop_frame);a.number(p.hours);a.number(p.minutes);a.number(p.seconds);a.number(p.pictures);a.flag(p.closed_gop);a.flag(p.broken_link);
 }else if constexpr(std::is_same_v<T,MoviePictureHeader>){kind="picture";
  a.number(p.temporal_reference);a.number(p.coding_type);a.number(p.vbv_delay);
  const auto direction=[&](const std::optional<MoviePictureDirection>& d){if(!d){a.raw("null");return;}Array v;v.flag(d->full_pel);v.number(d->f_code);a.raw(v.finish());};direction(p.forward);direction(p.backward);a.number(p.extra_information.size());a.hash(p.extra_information);
 }else if constexpr(std::is_same_v<T,MovieUserDataHeader>){kind="userdata";a.number(p.payload.size());a.hash(p.payload);
 }else{static_assert(std::is_same_v<T,MovieSequenceEndHeader>);kind="sequence_end";}
 return {kind,a.finish()};},payload);}
}
int main(int argc,char** argv){try{
 check(argc==3,"usage: test_native_movie_header_unit_corpus asset_root catalog.tsv");
 std::ifstream catalog(std::filesystem::u8path(argv[2]),std::ios::binary);check(bool(catalog),"explicit two-column source catalog");std::vector<MovieAssetCatalogEntry> entries;std::set<std::string> keys,paths;std::string line;
 while(std::getline(catalog,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();const auto at=line.find('\t');check(at!=line.npos&&at&&at+1<line.size()&&line.find('\t',at+1)==line.npos,"strict source catalog row");auto key=line.substr(0,at),path=line.substr(at+1);check(keys.insert(key).second&&paths.insert(path).second,"unique original source identity");entries.push_back({key,path});}
 check(catalog.eof()&&!catalog.bad()&&entries.size()==38,"complete explicit38 catalog");MovieAssetFailure af;auto store=NativeMovieAssetStore::create(std::filesystem::u8path(argv[1]),entries,af);check(bool(store),"actual native source store");
 for(const auto& entry:entries){
  auto asset=store->open(entry.logical_utf8,af);check(bool(asset),"actual source asset");MoviePesFailure pf;auto index=NativeMoviePesIndex::build(std::move(*asset),pf);check(bool(index),"original program-stream index");auto cursor=index->file_reader(pf);check(bool(cursor),"retained original file reader");auto length=cursor->length(pf);check(length.has_value(),"original file extent");
  NativeTestSha256 file_hash;std::uint64_t file_bytes=0;std::array<std::uint8_t,65536> block{};
  for(;;){const auto read=cursor->read(block.data(),block.size(),pf);check(!read.error,"original file read");if(!read.bytes)break;file_hash.add(block.data(),read.bytes);check(read.bytes<=UINT64_MAX-file_bytes,"source file extent no overflow");file_bytes+=read.bytes;}check(file_bytes==*length,"complete original file hashing");const auto asset_sha=file_hash.finish();
  const auto streams=index->streams();check(streams.size()==1&&streams[0].bytes<=std::vector<std::uint8_t>{}.max_size(),"one native-host sized original video ES");std::vector<std::uint8_t> es(static_cast<std::size_t>(streams[0].bytes));check(index->read_range(streams[0].stream_id,0,es,pf),"complete source video bytes");const auto es_sha=digest(es);
  MoviePrefixFailure prefix_failure;auto scanner=NativeMoviePrefixEvents::create(0,prefix_failure);check(scanner.has_value(),"raw prefix scanner");auto scanned=scanner->feed(0,es,prefix_failure);check(scanned.has_value()&&scanner->finish(prefix_failure).has_value(),"complete raw prefix stream");
  std::map<std::string,std::uint64_t> counts;std::uint64_t count=0;NativeTestSha256 unit_hash;unit_hash.add("[",1);std::optional<MovieHeaderUnit> held;std::string held_wire,held_raw;
  for(std::size_t i=0;i<scanned->events.size();++i){const auto& prefix=scanned->events[i];const auto code=prefix.raw_codeword&255u;
   if(code!=0xb3&&code!=0xb5&&code!=0xb8&&code!=0&&code!=0xb7&&code!=0xb2){check(code>=1&&code<=0xaf,"unmapped original video header prefix");continue;}
   const auto begin=prefix.es_prefix_first_byte;check(begin<es.size(),"unit offset within source");auto result=parse_movie_header_unit(std::span<const std::uint8_t>(es).subspan(static_cast<std::size_t>(begin)));
   check(result.status==MovieHeaderUnitStatus::parsed&&result.failure.kind==MovieHeaderUnitError::none&&result.unit.has_value(),"known original header must parse without fallback");auto& unit=*result.unit;
   const auto next=i+1<scanned->events.size()?scanned->events[i+1].es_prefix_first_byte:es.size();check(unit.consumed_bytes>=4&&unit.consumed_bytes<=next-begin&&unit.bytes.size()==unit.consumed_bytes&&std::equal(unit.bytes.begin(),unit.bytes.end(),es.begin()+begin),"exact owned consumed header bytes distinct from following raw prefix");
   auto body=fields(unit.payload);Array wire;wire.number(begin);wire.number(begin+unit.consumed_bytes);wire.number(next);wire.number(prefix.classification);wire.raw(json_string(body.kind));wire.raw(body.wire);wire.hash(unit.bytes);auto row=wire.finish();if(count)unit_hash.add(",",1);unit_hash.add(row.data(),row.size());++count;++counts[body.kind];
   if(!held){held=std::move(unit);held_wire=fields(held->payload).wire;held_raw=digest(held->bytes);}
  }
  unit_hash.add("]",1);const auto units_sha=unit_hash.finish();check(count&&held.has_value(),"actual source units and owned lifetime sample");
  scanner.reset();scanned.reset();cursor.reset();index.reset();asset.reset();es.clear();es.shrink_to_fit();check(fields(held->payload).wire==held_wire&&digest(held->bytes)==held_raw,"owned parsed source values after file/index/input teardown");
  std::printf("{\"key\":%s,\"asset_sha256\":\"%s\",\"video_es_bytes\":%llu,\"video_es_sha256\":\"%s\",\"unit_count\":%llu,\"type_counts\":{",json_string(entry.logical_utf8).c_str(),asset_sha.c_str(),(unsigned long long)streams[0].bytes,es_sha.c_str(),(unsigned long long)count);bool first=true;for(const auto& [kind,n]:counts){if(!first)std::putchar(',');first=false;std::printf("%s:%llu",json_string(kind).c_str(),(unsigned long long)n);}std::printf("},\"unit_array_sha256\":\"%s\"}\n",units_sha.c_str());
 }
 std::puts("PASS native MPEG2 header units38 source bitfields/endpoints; no parser-batch/admission/player");check(!std::fflush(stdout)&&!std::ferror(stdout),"complete unit corpus output");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"[header-unit-corpus] %s\n",e.what());return 1;}}
