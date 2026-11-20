#include "mka/movie_decode.hpp"
#include "movie_source_syntax_wire.hpp"
#include <fstream>
#include <map>
#include <set>
using namespace mka::video;
using namespace native_syntax_test;
int main(int argc,char** argv){try{
 check(argc==4,"usage: test_native_movie_source_syntax_corpus DLLdir asset_root catalog.tsv");
 std::ifstream catalog(std::filesystem::u8path(argv[3]),std::ios::binary);check(bool(catalog),"explicit source catalog");std::vector<MovieAssetCatalogEntry> entries;std::set<std::string> keys,paths;std::string line;
 while(std::getline(catalog,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();const auto at=line.find('\t');check(at!=line.npos&&at&&at+1<line.size()&&line.find('\t',at+1)==line.npos,"strict two-column catalog");auto key=line.substr(0,at),path=line.substr(at+1);check(keys.insert(key).second&&paths.insert(path).second,"unique source catalog");entries.push_back({key,path});}
 check(catalog.eof()&&!catalog.bad()&&entries.size()==38,"complete38 source catalog");MovieAssetFailure af;auto store=NativeMovieAssetStore::create(std::filesystem::u8path(argv[2]),entries,af);check(bool(store),"actual source asset store");
 for(const auto& entry:entries){
  auto asset=store->open(entry.logical_utf8,af);check(bool(asset),"owned reference source");MoviePesFailure pf;auto pes=NativeMoviePesIndex::build(std::move(*asset),pf);check(bool(pes),"actual PS grammar");auto reader=pes->file_reader(pf);check(bool(reader),"same opened source cursor");
  NativeTestSha256 fh;std::array<std::uint8_t,65536> block{};for(;;){auto r=reader->read(block.data(),block.size(),pf);check(!r.error,"complete source hash read");if(!r.bytes)break;fh.add(block.data(),r.bytes);}const auto asset_sha=fh.finish();
  auto streams=pes->streams();check(streams.size()==1&&streams[0].bytes<=std::vector<std::uint8_t>{}.max_size(),"actual one video stream host extent");std::vector<std::uint8_t> es(static_cast<std::size_t>(streams[0].bytes));check(pes->read_range(streams[0].stream_id,0,es,pf),"exact source ES read");const auto es_sha=digest(es);
  auto identity=std::make_shared<const MovieSourceObservationIdentity>();MovieSourceSyntaxFailure sf;auto index=NativeMovieSourceSyntaxIndex::build(*pes,streams[0].stream_id,entry.logical_utf8,identity,sf);
  if(!index){std::fprintf(stderr,"[syntax-index] %s kind=%d ES=%llu unit=%d\n",entry.logical_utf8.c_str(),int(sf.kind),(unsigned long long)sf.es_byte,int(sf.unit.kind));throw std::runtime_error("actual original source syntax index");}
  std::map<std::uint64_t,std::string> expected;NativeTestSha256 source_rows;source_rows.add("[",1);bool comma=false;std::uint64_t absent=0,cross=0;
  for(const auto& record:index->pictures()){check(record&&record->source_key==entry.logical_utf8&&record->identity==identity,"bound reference record");const auto wire=source_record(*record);check(expected.emplace(record->picture_es_byte,string_digest(wire)).second,"unique source picture ES");if(comma)source_rows.add(",",1);comma=true;source_rows.add(wire.data(),wire.size());if(!record->pes.pts33)++absent;if(record->picture_es_byte+4>record->pes.es_end)++cross;}
  source_rows.add("]",1);const auto source_sha=source_rows.finish();auto held=index->pictures().front();const auto held_wire=source_record(*held);
  auto input=store->open(entry.logical_utf8,af);check(bool(input),"actual decoder catalog owner");MovieDecodeFailure df;auto decoder=NativeMovieDecoder::open(std::filesystem::u8path(argv[1]),std::move(*input),df);
  if(!decoder){std::fprintf(stderr,"[syntax-decode-open] %s kind=%d operation=%s syntax=%d at=%llu\n",entry.logical_utf8.c_str(),int(df.kind),df.operation.c_str(),int(df.syntax_failure.kind),(unsigned long long)df.syntax_failure.es_byte);throw std::runtime_error("actual decoder open");}
  const auto decoder_identity=decoder->observation_identity();check(decoder_identity&&decoder_identity!=identity,"actual per-open source identities");Array attachments;std::set<std::uint64_t> seen;std::shared_ptr<const MovieSourceSyntaxPicture> held_decoded;std::string held_decoded_wire;bool moved=false;
  for(;;){auto r=decoder->next(df);if(r.status==MovieReadStatus::error){std::fprintf(stderr,"[syntax-decode-next] %s kind=%d operation=%s syntax=%d\n",entry.logical_utf8.c_str(),int(df.kind),df.operation.c_str(),int(df.syntax_failure.kind));throw std::runtime_error("actual decoder record association");}if(r.status==MovieReadStatus::end)break;
   if(auto* frame=std::get_if<MovieVideoFrame>(&r.frame)){
    const auto& record=frame->source_syntax;check(record&&record->source_key==entry.logical_utf8&&record->identity==decoder_identity,"decoded record actual native source binding");check(frame->codec_timing.picture&&frame->codec_timing.picture_source,"actual decoded source/timing");const auto& source=*frame->codec_timing.picture_source;check(record->picture_es_byte==source.picture_es_byte&&record->pes==source.pes&&record->quarter_pts==source.quarter_pts,"decoded exact first-byte source provenance");auto match=expected.find(source.picture_es_byte);const auto wire=source_record(*record);check(match!=expected.end()&&match->second==string_digest(wire)&&seen.insert(source.picture_es_byte).second,"exact immutable source record once per decoded picture");Array a;a.number(source.picture_es_byte);a.raw(json_string(match->second));attachments.raw(a.finish());
    if(!held_decoded){held_decoded=record;held_decoded_wire=wire;}
    if(!moved){auto next=std::make_unique<NativeMovieDecoder>(std::move(*decoder));check(!decoder->observation_identity()&&decoder->next(df).status==MovieReadStatus::error&&df.kind==MovieDecodeError::invalid_owner,"moved decoder rejects old owner");decoder=std::move(next);check(decoder->observation_identity()==decoder_identity,"decoder move retains syntax identity");moved=true;}
   }
  }
  check(seen.size()==expected.size()&&held_decoded&&decoder->next(df).status==MovieReadStatus::end,"complete exact decoded-picture coverage and stable EOF");decoder.reset();input.reset();index.reset();reader.reset();pes.reset();asset.reset();identity.reset();es.clear();es.shrink_to_fit();
  check(source_record(*held)==held_wire&&source_record(*held_decoded)==held_decoded_wire&&held_decoded->identity==decoder_identity,"owned records survive input/index/decoder/library teardown");
  std::printf("{\"key\":%s,\"asset_sha256\":\"%s\",\"video_es_sha256\":\"%s\",\"pictures\":%zu,\"source_array_sha256\":\"%s\",\"attachments\":%s,\"cross_pes_prefixes\":%llu,\"absent_pts\":%llu}\n",json_string(entry.logical_utf8).c_str(),asset_sha.c_str(),es_sha.c_str(),expected.size(),source_sha.c_str(),attachments.finish().c_str(),(unsigned long long)cross,(unsigned long long)absent);
 }
 store.reset();std::puts("PASS native source syntax38 and exact decoded-picture attachments; no SDK admission or playback");check(!std::fflush(stdout)&&!std::ferror(stdout),"complete source-syntax corpus receipt");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"[source-syntax-corpus] %s\n",e.what());return 1;}}
