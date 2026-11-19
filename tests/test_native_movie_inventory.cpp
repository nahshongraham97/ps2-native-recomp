// Offline full-asset codec coverage. No presentation/audio device/game execution.
#include "mka/movie_decode.hpp"
#include "native_test_sha256.hpp"
#include "data/movie_decode_oracle.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>
namespace {
using namespace mka::video;
std::string path_bytes(const std::filesystem::path& p){const auto s=p.generic_u8string();return {reinterpret_cast<const char*>(s.data()),s.size()};}
void quote(std::ostream& out,const std::string& s){out<<'"';const char* hex="0123456789abcdef";for(unsigned char c:s){if(c=='"'||c=='\\')out<<'\\'<<char(c);else if(c<32)out<<"\\u00"<<hex[c>>4]<<hex[c&15];else out<<char(c);}out<<'"';}
void add(std::uint64_t& a,std::uint64_t b){if(b>UINT64_MAX-a)throw std::runtime_error("inventory uint64 counter overflow");a+=b;}
struct Hash {NativeTestSha256 sha;std::uint64_t bytes=0,fnv=14695981039346656037ull;bool with_fnv=false;explicit Hash(bool enabled=false):with_fnv(enabled){}
 void put(const void* p,std::size_t n){add(bytes,n);sha.add(p,n);const auto* b=static_cast<const unsigned char*>(p);if(with_fnv)for(std::size_t i=0;i<n;++i)fnv=(fnv^b[i])*1099511628211ull;}
};
struct Range {
 std::uint64_t count=0;std::int64_t first=0,last=0,minimum=0,maximum=0;
 std::map<std::int64_t,std::uint64_t> deltas;
 void put(std::int64_t v){
  if(!count)first=minimum=maximum=v;
  else {if((last<0&&v>INT64_MAX+last)||(last>0&&v<INT64_MIN+last))throw std::runtime_error("timestamp delta overflow");add(deltas[v-last],1);}
  last=v;minimum=std::min(minimum,v);maximum=std::max(maximum,v);add(count,1);
 }
};
struct Times {std::uint64_t missing=0;std::map<std::pair<std::int32_t,std::int32_t>,Range> bases;
 void put(const MovieTimestamp& t){if(!t.present){add(missing,1);return;}bases[{t.numerator,t.denominator}].put(t.value);}
 bool equals(const native_movie_oracle::Times& e)const{return missing==e.missing&&bases.size()==1&&bases.begin()->first==std::pair<std::int32_t,std::int32_t>{1,90000}&&bases.begin()->second.first==e.first&&bases.begin()->second.last==e.last;}
 void json(std::ostream& out)const{out<<"{\"missing\":"<<missing<<",\"timebases\":[";bool comma=false;for(const auto& item:bases){if(comma)out<<',';comma=true;const auto& t=item.second;out<<"{\"numerator\":"<<item.first.first<<",\"denominator\":"<<item.first.second<<",\"count\":"<<t.count<<",\"first\":"<<t.first<<",\"last\":"<<t.last<<",\"min\":"<<t.minimum<<",\"max\":"<<t.maximum<<",\"observed_deltas\":[";bool delta_comma=false;for(const auto& d:t.deltas){if(delta_comma)out<<',';delta_comma=true;out<<"{\"ticks\":"<<d.first<<",\"count\":"<<d.second<<'}';}out<<"]}";}out<<"]}";}
};
const char* decode_name(MovieDecodeError e){switch(e){
#define CASE(x) case MovieDecodeError::x:return #x;
 CASE(none) CASE(unsupported_host) CASE(library_load) CASE(missing_symbol) CASE(library_version) CASE(file_io) CASE(allocation) CASE(invalid_stream) CASE(unsupported_stream) CASE(unsupported_frame) CASE(demux) CASE(decode) CASE(invalid_owner) CASE(failed_owner)
#undef CASE
 }return "unknown_decode_error";}
const char* asset_name(MovieAssetError e){switch(e){
#define CASE(x) case MovieAssetError::x:return #x;
 CASE(none) CASE(moved_store) CASE(moved_asset) CASE(invalid_utf8) CASE(invalid_name) CASE(absolute_path) CASE(traversal) CASE(iso_provider_suffix) CASE(invalid_root) CASE(invalid_host_path) CASE(duplicate_key) CASE(root_escape) CASE(not_regular_file) CASE(association_changed) CASE(unknown_key) CASE(file_open) CASE(file_read) CASE(file_seek) CASE(file_position) CASE(file_length) CASE(opened_path_unavailable) CASE(opened_path_mismatch) CASE(allocation_failed) CASE(host_capacity)
#undef CASE
 }return "unknown_asset_error";}
struct Record {
 std::string key,source_sha,video_sha,audio_sha,stage,message;bool complete=false,golden_present=false,golden_match=false;
 std::uint64_t source_bytes=0,video_frames=0,audio_frames=0,audio_samples=0,scalar_samples=0,video_bytes=0,audio_bytes=0;
 std::uint64_t video_fnv=0,audio_fnv=0;MovieDecodeFailure decode;MovieAssetFailure asset;
 Times video_pts,video_best,audio_pts,audio_best;
 using Shape=std::tuple<std::uint32_t,std::uint32_t,std::int32_t,std::int32_t,std::int32_t,std::int32_t,std::int32_t,std::int32_t>;
 std::set<Shape> video_shapes;std::set<std::pair<std::uint32_t,std::size_t>> audio_shapes;
 bool ok()const{return complete&&message.empty()&&(!golden_present||golden_match);}
 void json(std::ostream& out)const{
  out<<"{\"type\":\"asset\",\"logical_relative_key\":";quote(out,key);out<<",\"status\":";quote(out,ok()?"complete":"failed");out<<",\"decoded_hash_scope\":";quote(out,complete?"complete_stream":"decoded_prefix");
  out<<",\"source_sha256\":";quote(out,source_sha);out<<",\"source_bytes\":"<<source_bytes<<",\"video_frames\":"<<video_frames<<",\"audio_frames\":"<<audio_frames<<",\"audio_sample_frames\":"<<audio_samples<<",\"audio_scalar_samples\":"<<scalar_samples;
  out<<",\"video_bytes\":"<<video_bytes<<",\"video_sha256\":";quote(out,video_sha);out<<",\"audio_bytes\":"<<audio_bytes<<",\"audio_sha256\":";quote(out,audio_sha);
  out<<",\"video_raw_pts\":";video_pts.json(out);out<<",\"video_best_timestamp\":";video_best.json(out);out<<",\"audio_raw_pts\":";audio_pts.json(out);out<<",\"audio_best_timestamp\":";audio_best.json(out);
  out<<",\"video_nominal_rate\":\"not_exposed_by_decoder_API; observed_timestamp_deltas_reported\",\"video_hash_order\":\"per_frame_Y_U_V\",\"audio_hash_order\":\"per_frame_channel_vectors_s16_hostLE\",\"video_metadata\":[";bool comma=false;for(const auto& item:video_shapes){if(comma)out<<',';comma=true;auto [w,h,sn,sd,range,space,prim,transfer]=item;out<<"{\"width\":"<<w<<",\"height\":"<<h<<",\"sar_num\":"<<sn<<",\"sar_den\":"<<sd<<",\"color_range\":"<<range<<",\"color_space\":"<<space<<",\"color_primaries\":"<<prim<<",\"color_transfer\":"<<transfer<<'}';}
  out<<"],\"audio_metadata\":[";comma=false;for(const auto& item:audio_shapes){if(comma)out<<',';comma=true;out<<"{\"sample_rate\":"<<item.first<<",\"channels\":"<<item.second<<'}';}
  out<<"],\"fixed_golden\":";quote(out,!golden_present?"not_available":!complete?"not_compared_incomplete_decode":golden_match?"match":"mismatch");
  out<<",\"failure\":{\"stage\":";quote(out,stage);out<<",\"message\":";quote(out,message);out<<",\"decode_kind\":";quote(out,decode_name(decode.kind));out<<",\"native_code\":"<<decode.native_code<<",\"operation\":";quote(out,decode.operation);
  out<<",\"asset_kind\":";quote(out,asset_name(asset.kind));out<<",\"asset_host_code\":"<<asset.host_error.value()<<",\"asset_catalog_index\":"<<asset.catalog_index<<"}}";
 }
};
Record inspect(const std::string& key,const NativeMovieAssetStore& store,const std::filesystem::path& libraries){
 Record r;r.key=key;for(const auto& g:native_movie_oracle::assets)if(key==g.name)r.golden_present=true;
 try{
  Hash source,video(r.golden_present),audio(r.golden_present);r.stage="catalog_open";auto asset=store.open(key,r.asset);if(!asset){r.message="NativeMovieAssetStore::open";return r;}
  r.stage="source_hash";std::array<unsigned char,65536> buffer;
  for(;;){auto read=asset->read(buffer.data(),buffer.size(),r.asset);if(read.error){r.message="NativeMovieAsset::read source hash";return r;}source.put(buffer.data(),read.bytes);if(read.eof)break;if(!read.bytes){r.message="source read made no progress";return r;}}
  r.source_sha=source.sha.finish();r.source_bytes=source.bytes;
  r.stage="decoder_open";auto decoder=NativeMovieDecoder::open(libraries,std::move(*asset),r.decode);if(!decoder){r.asset=r.decode.asset_failure;r.message="NativeMovieDecoder::open same opened asset";return r;}
  r.stage="decode";
  for(;;){auto next=decoder->next(r.decode);if(next.status==MovieReadStatus::error){r.asset=r.decode.asset_failure;r.message="NativeMovieDecoder::next";break;}if(next.status==MovieReadStatus::end){r.complete=true;break;}
   if(auto* v=std::get_if<MovieVideoFrame>(&next.frame)){add(r.video_frames,1);for(const auto& p:v->planes)video.put(p.data(),p.size());r.video_pts.put(v->source_pts);r.video_best.put(v->timestamp);r.video_shapes.emplace(v->width,v->height,v->sample_aspect_numerator,v->sample_aspect_denominator,v->color_range,v->color_space,v->color_primaries,v->color_transfer);}
   else {const auto& a=std::get<MovieAudioFrame>(next.frame);add(r.audio_frames,1);if(!a.channels.empty())add(r.audio_samples,a.channels.front().size());for(const auto& p:a.channels){add(r.scalar_samples,p.size());audio.put(p.data(),p.size()*sizeof(std::int16_t));}r.audio_pts.put(a.source_pts);r.audio_best.put(a.timestamp);r.audio_shapes.emplace(a.sample_rate,a.channels.size());}
  }
  r.video_sha=video.sha.finish();r.audio_sha=audio.sha.finish();r.video_bytes=video.bytes;r.audio_bytes=audio.bytes;r.video_fnv=video.fnv;r.audio_fnv=audio.fnv;
  for(const auto& g:native_movie_oracle::assets)if(key==g.name){r.golden_present=true;r.golden_match=r.complete&&r.video_frames==g.video.frames&&r.audio_frames==g.audio.frames&&r.audio_samples==g.audio.samples&&r.video_bytes==g.video.bytes&&r.audio_bytes==g.audio.bytes&&r.video_fnv==g.video.fnv&&r.audio_fnv==g.audio.fnv&&r.video_sha==g.video.sha256&&r.audio_sha==g.audio.sha256&&r.video_pts.equals(g.video.pts)&&r.video_best.equals(g.video.best)&&r.audio_pts.equals(g.audio.pts)&&r.audio_best.equals(g.audio.best);if(r.complete&&!r.golden_match){r.stage="fixed_golden";r.message="exact-name independent decoded hash/count/timestamp golden mismatch";}}
  if(r.ok())r.stage.clear();
 }catch(const std::exception& e){r.message=e.what();}return r;
}
}
int main(int argc,char** argv){try{
 if(argc!=3)throw std::runtime_error("usage: test_native_movie_inventory explicit_codec_library_directory MOVIEPS2_root");
 const auto libraries=std::filesystem::u8path(argv[1]),root=std::filesystem::canonical(std::filesystem::u8path(argv[2]));
 if(!std::filesystem::is_directory(root))throw std::runtime_error("MOVIEPS2 root is not a directory");
 std::vector<std::string> keys;
 for(const auto& entry:std::filesystem::recursive_directory_iterator(root)){
  if(!entry.is_regular_file())continue;auto extension=path_bytes(entry.path().extension());for(auto& c:extension)if(c>='a'&&c<='z')c=char(c-'a'+'A');
  if(extension==".SFD")keys.push_back(path_bytes(entry.path().lexically_relative(root)));
 }
 std::sort(keys.begin(),keys.end());if(keys.empty())throw std::runtime_error("no actual SFD assets found");
 std::vector<MovieAssetCatalogEntry> catalog;for(const auto& key:keys)catalog.push_back({key,key});MovieAssetFailure failure;
 auto store=NativeMovieAssetStore::create(root,catalog,failure);
 if(!store){std::ostringstream line;line<<"{\"type\":\"inventory_failure\",\"operation\":\"explicit_catalog_creation\",\"asset_kind\":";quote(line,asset_name(failure.kind));line<<",\"catalog_index\":"<<failure.catalog_index<<",\"host_code\":"<<failure.host_error.value()<<'}';std::cout<<line.str()<<std::endl;return 1;}
 std::cout<<"{\"type\":\"inventory_begin\",\"actual_sfd_files\":"<<keys.size()<<",\"scope\":\"offline full decode observations; no game, retail call reachability, presentation or audible fidelity claim\"}"<<std::endl;
 std::size_t failures=0,complete=0;
 for(std::size_t i=0;i<keys.size();++i){std::fprintf(stderr,"[native-movie-inventory] %zu/%zu %s\n",i+1,keys.size(),keys[i].c_str());auto result=inspect(keys[i],*store,libraries);if(result.ok())++complete;else ++failures;std::ostringstream line;result.json(line);std::cout<<line.str()<<std::endl;}
 std::cout<<"{\"type\":\"inventory_end\",\"actual_sfd_files\":"<<keys.size()<<",\"complete\":"<<complete<<",\"failed\":"<<failures<<",\"retail_fidelity\":\"NOT_EVALUATED\"}"<<std::endl;
 return failures?1:0;
 }catch(const std::exception& e){std::ostringstream line;line<<"{\"type\":\"inventory_failure\",\"operation\":\"filesystem_or_inventory\",\"message\":";quote(line,e.what());line<<'}';std::cout<<line.str()<<std::endl;return 1;}}
