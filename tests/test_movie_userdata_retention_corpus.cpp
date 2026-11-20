#include "mka/movie_userdata_retention.hpp"
#include "mka/movie_pes_index.hpp"
#include "mka/movie_prefix_events.hpp"
#include "native_test_sha256.hpp"
#include <cstdio>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>
using namespace mka::video;
namespace {
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
std::string digest(std::span<const std::uint8_t> b){NativeTestSha256 h;h.add(b.data(),b.size());return h.finish();}
std::string json_string(std::string_view text){std::string out="\"";for(unsigned char c:text){if(c=='"'||c=='\\'){out+='\\';out+=char(c);}else if(c<32){char b[7];std::snprintf(b,sizeof b,"\\u%04x",unsigned(c));out+=b;}else out+=char(c);}return out+'"';}
std::string state_wire(unsigned kind,const MovieUserdataRetentionState& s){return "["+std::to_string(kind)+","+std::to_string(unsigned(s.phase))+","+std::to_string(s.field_counter)+","+std::to_string(unsigned(s.bound))+","+std::to_string(s.bytes.size())+","+json_string(digest(s.bytes))+"]";}
}
int main(int argc,char** argv){try{
 check(argc==3,"usage: test_native_movie_userdata_retention_corpus asset_root catalog.tsv");
 std::ifstream file(std::filesystem::u8path(argv[2]),std::ios::binary);check(bool(file),"explicit original38 two-column catalog");std::vector<MovieAssetCatalogEntry> entries;std::set<std::string> keys,paths;std::string line;
 while(std::getline(file,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();const auto at=line.find('\t');check(at!=line.npos&&at&&at+1<line.size()&&line.find('\t',at+1)==line.npos,"strict catalog columns");auto key=line.substr(0,at),path=line.substr(at+1);check(keys.insert(key).second&&paths.insert(path).second,"unique native source entries");entries.push_back({key,path});}
 check(file.eof()&&!file.bad()&&entries.size()==38,"complete original38 set");MovieAssetFailure af;auto store=NativeMovieAssetStore::create(std::filesystem::u8path(argv[1]),entries,af);check(bool(store),"native original asset store");
 for(const auto& entry:entries){
  auto asset=store->open(entry.logical_utf8,af);check(bool(asset),"actual original opened asset");MoviePesFailure pf;auto index=NativeMoviePesIndex::build(std::move(*asset),pf);check(bool(index),"native original PS index");auto cursor=index->file_reader(pf);check(bool(cursor),"same retained opened-file cursor");auto length=cursor->length(pf);check(length.has_value(),"original source length");NativeTestSha256 file_hash;std::uint64_t total=0;std::array<std::uint8_t,65536> block{};
  for(;;){auto read=cursor->read(block.data(),block.size(),pf);check(!read.error,"original file hashing");if(!read.bytes)break;check(read.bytes<=UINT64_MAX-total,"source size checked");total+=read.bytes;file_hash.add(block.data(),read.bytes);}check(total==*length,"complete file hash");const auto file_sha=file_hash.finish();
  const auto streams=index->streams();check(streams.size()==1&&streams[0].bytes<=std::vector<std::uint8_t>{}.max_size(),"one native-host sized video ES");std::vector<std::uint8_t> es(static_cast<std::size_t>(streams[0].bytes));check(index->read_range(streams[0].stream_id,0,es,pf),"original complete video ES");const auto es_sha=digest(es);
  MoviePrefixFailure prefix_failure;auto scanner=NativeMoviePrefixEvents::create(0,prefix_failure);check(scanner.has_value(),"source prefix owner");auto prefixes=scanner->feed(0,es,prefix_failure);check(prefixes.has_value()&&scanner->finish(prefix_failure).has_value(),"complete source prefixes");
  auto identity=std::make_shared<const MovieSourceObservationIdentity>();MovieUserdataRetentionFailure rf;auto retention=NativeMovieUserdataRetention::create(entry.logical_utf8,identity,true,rf);check(retention.has_value(),"explicit bound normal-factory retention fixture");
  unsigned primary=0;std::uint64_t coding=0,userdata=0,event_count=0;NativeTestSha256 event_hash;event_hash.add("[",1);
  for(const auto& prefix:prefixes->events){const auto code=prefix.raw_codeword&255u;if(code==0xb3||code==0xb7)primary=1;else if(code==0xb8)primary=2;else if(code==0)primary=3;
   if(code!=0xb3&&code!=0xb5&&code!=0xb8&&code!=0&&code!=0xb7&&code!=0xb2){check(code>=1&&code<=0xaf,"unmapped original header prefix");continue;}
   const auto begin=prefix.es_prefix_first_byte;check(begin<es.size(),"source unit offset");auto parsed=parse_movie_header_unit(std::span<const std::uint8_t>(es).subspan(static_cast<std::size_t>(begin)));check(parsed.status==MovieHeaderUnitStatus::parsed&&parsed.unit&&parsed.failure.kind==MovieHeaderUnitError::none,"actual original unit parsed without fallback");const auto& unit=*parsed.unit;std::string kind;
   if(std::holds_alternative<MoviePictureCodingExtension>(unit.payload)){kind="coding_extension";++coding;check(retention->coding_extension(entry.logical_utf8,identity,unit,rf),"actual coded-picture phase event");}
   else if(std::holds_alternative<MovieUserDataHeader>(unit.payload)){kind="userdata";++userdata;check(retention->append(entry.logical_utf8,identity,static_cast<MovieUserdataPrimaryKind>(primary),unit,rf),"actual whole B2 current retention event");}
   else continue;
   auto s=retention->snapshot(rf);check(s&&s->identity==identity&&s->source_key==entry.logical_utf8&&s->bound,"owned source-bound retention snapshot");
   const std::string wire="["+json_string(kind)+","+std::to_string(begin)+","+std::to_string(primary)+","+std::to_string(unsigned(s->phase))+","+std::to_string(s->field_counter)+","+std::to_string(unsigned(s->bound))+","+std::to_string(s->bytes.size())+","+json_string(digest(s->bytes))+"]";
   if(event_count)event_hash.add(",",1);event_hash.add(wire.data(),wire.size());++event_count;
  }
  event_hash.add("]",1);const auto events_sha=event_hash.finish();auto held=retention->snapshot(rf);check(held&&event_count==coding+userdata&&coding&&userdata,"complete original current-retention events");const auto final=state_wire(primary,*held);
  retention.reset();identity.reset();prefixes.reset();scanner.reset();cursor.reset();index.reset();asset.reset();es.clear();es.shrink_to_fit();check(state_wire(primary,*held)==final&&held->identity,"owned bytes and native epoch survive all source/retention teardown");
  std::printf("{\"key\":%s,\"asset_sha256\":\"%s\",\"video_es_sha256\":\"%s\",\"coding_events\":%llu,\"b2_events\":%llu,\"type_counts\":{\"coding_extension\":%llu,\"userdata\":%llu},\"final_state\":%s,\"event_array_sha256\":\"%s\"}\n",json_string(entry.logical_utf8).c_str(),file_sha.c_str(),es_sha.c_str(),(unsigned long long)coding,(unsigned long long)userdata,(unsigned long long)coding,(unsigned long long)userdata,final.c_str(),events_sha.c_str());
 }
 std::puts("PASS native current userdata retention38 owned bytes/phase; no callbacks or selected-output binding");check(!std::fflush(stdout)&&!std::ferror(stdout),"complete current-retention fixture output");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"[userdata-retention-corpus] %s\n",e.what());return 1;}}
