#include "mka/movie_pes_index.hpp"
#include <array>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>
using namespace mka::video;
namespace {
using Bytes=std::vector<std::uint8_t>;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void json_string(const std::string& text){std::putchar('"');for(unsigned char c:text){if(c=='"'||c=='\\'){std::putchar('\\');std::putchar(c);}else if(c<32)std::printf("\\u%04x",unsigned(c));else std::putchar(c);}std::putchar('"');}
struct Temp {
 std::filesystem::path parent,path;
 Temp(){parent=std::filesystem::canonical(std::filesystem::temp_directory_path());path=parent/("mka-program-end-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));check(std::filesystem::create_directory(path),"exclusive synthetic directory");path=std::filesystem::canonical(path);check(path.is_absolute()&&path.parent_path()==parent,"resolved synthetic boundary");}
 ~Temp(){std::error_code ec;auto actual=std::filesystem::canonical(path,ec);if(ec||actual!=path||!actual.is_absolute()||actual.parent_path()!=parent){std::fprintf(stderr,"program-end cleanup refused changed boundary\n");return;}std::filesystem::remove_all(actual,ec);if(ec)std::fprintf(stderr,"program-end cleanup error %d\n",ec.value());}
};
Bytes pack(){return {0,0,1,0xba,0x21,0,1,0,1,0x80,0,1};}
void pes(Bytes& b,std::uint8_t id,const Bytes& payload){const auto n=payload.size()+1;check(n<=65535,"synthetic PES length");b.insert(b.end(),{0,0,1,id,std::uint8_t(n>>8),std::uint8_t(n),15});b.insert(b.end(),payload.begin(),payload.end());}
void write(const std::filesystem::path& file,const Bytes& b){std::ofstream out(file,std::ios::binary);check(bool(out),"synthetic open");out.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size()));out.flush();check(bool(out),"synthetic write/flush");out.close();check(!out.fail(),"synthetic close");}
std::optional<NativeMoviePesIndex> index_file(const std::filesystem::path& dir,const Bytes& bytes,MoviePesFailure& why){write(dir/"case.sfd",bytes);MovieAssetFailure af;auto store=NativeMovieAssetStore::create(dir,{{"case","case.sfd"}},af);check(bool(store),"synthetic catalog");auto asset=store->open("case",af);check(bool(asset),"synthetic asset");return NativeMoviePesIndex::build(std::move(*asset),why);}
void synthetic(){
 Temp temp;MoviePesFailure why;
 for(std::size_t padding:std::array<std::size_t,4>{0,1,4097,10000}){
  auto bytes=pack();const Bytes payload{0,0,1,0xb9,99};pes(bytes,0xe0,payload);pes(bytes,0xc0,{7,8});const auto end=bytes.size();bytes.insert(bytes.end(),{0,0,1,0xb9});bytes.insert(bytes.end(),padding,255);write(temp.path/"owned.sfd",bytes);
  MovieAssetFailure af;auto store=NativeMovieAssetStore::create(temp.path,{{"owned","owned.sfd"}},af);check(bool(store),"owned store");auto asset=store->open("owned",af);check(asset&&asset->seek(5,mka::content::SeekOrigin::begin,af),"nonzero initial cursor");auto index=NativeMoviePesIndex::build(std::move(*asset),why);check(index&&why.kind==MoviePesError::none,"B9 and validated padding");
  const auto observation=index->program_end();check(observation==MovieProgramEndObservation{end,4,padding},"exact B9 offset/extent/padding");check(index->streams().size()==1&&index->streams()[0].bytes==payload.size()&&index->audio_streams().size()==1&&index->audio_streams()[0].bytes==2,"video/audio enumeration unchanged and embedded B9 opaque");
  auto cursor=index->file_reader(why);auto reader=index->reader(0xe0,why);check(cursor&&cursor->position()==0&&reader,"independent same-backing readers");check(cursor->seek(static_cast<std::int64_t>(end),mka::content::SeekOrigin::begin,why),"source B9 seek");check(reader->validate_next(payload,why)&&reader->finished()&&cursor->position()==end,"ES validation preserves file-reader cursor");
  NativeMoviePesIndex moved(std::move(*index));check(!index->program_end()&&moved.program_end()==observation,"moved index observation");index.reset();{NativeMoviePesIndex last(std::move(moved));}store.reset();asset.reset();
  std::array<std::uint8_t,4> code{};auto read=cursor->read(code.data(),code.size(),why);check(!read.error&&read.bytes==4&&code==std::array<std::uint8_t,4>{0,0,1,0xb9},"reader retains actual opened file after all index/catalog owners");check(observation->file_offset==end&&observation->byte_extent==4&&observation->trailing_padding_bytes==padding,"owned observation survives all index teardown");
 }
 auto without=pack();pes(without,0xe0,{0,0,1,0xb9,9});check(!index_file(temp.path,without,why)&&why.kind==MoviePesError::missing_end,"complete EOF without actual top-level B9 retains missing_end");
 auto short_code=pack();short_code.insert(short_code.end(),{0,0,1});check(!index_file(temp.path,short_code,why)&&why.kind==MoviePesError::truncated&&why.offset==12,"truncated control does not produce observation");
 auto bad_tail=pack();bad_tail.insert(bad_tail.end(),{0,0,1,0xb9,255,0});check(!index_file(temp.path,bad_tail,why)&&why.kind==MoviePesError::bad_tail&&why.offset==17,"invalid postB9 tail rejects entire owner");
 auto bad_prefix=pack();bad_prefix.insert(bad_prefix.end(),{0,0,2,0xb9});check(!index_file(temp.path,bad_prefix,why)&&why.kind==MoviePesError::bad_prefix,"B9 literal without grammar prefix not accepted");
 auto empty=Bytes{};check(!index_file(temp.path,empty,why)&&why.kind==MoviePesError::missing_end,"empty input admission unchanged");
}
void corpus(const std::filesystem::path& root,const std::filesystem::path& catalog){
 std::ifstream input(catalog,std::ios::binary);check(bool(input),"catalog open");std::vector<MovieAssetCatalogEntry> entries;std::set<std::string> keys,paths;std::string line;
 while(std::getline(input,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();const auto tab=line.find('\t');check(tab!=std::string::npos&&tab>0&&tab+1<line.size()&&line.find('\t',tab+1)==std::string::npos,"strict2 column catalog");auto key=line.substr(0,tab),path=line.substr(tab+1);check(keys.insert(key).second&&paths.insert(path).second,"unique catalog");entries.push_back({key,path});}
 check(input.eof()&&!input.bad()&&entries.size()==38,"complete38 catalog");MovieAssetFailure af;auto store=NativeMovieAssetStore::create(root,entries,af);check(bool(store),"actual explicit asset store");
 for(const auto& entry:entries){auto asset=store->open(entry.logical_utf8,af);check(bool(asset),"actual opened asset");const auto length=asset->length(af);check(length.has_value(),"actual asset extent");MoviePesFailure why;auto index=NativeMoviePesIndex::build(std::move(*asset),why);check(bool(index)&&why.kind==MoviePesError::none,"actual source index");const auto end=index->program_end();check(end&&end->byte_extent==4&&end->file_offset<=*length&&*length-end->file_offset>=4&&end->trailing_padding_bytes==*length-end->file_offset-4,"actual observed B9 extent");
  auto cursor=index->file_reader(why);check(cursor&&end->file_offset<=INT64_MAX&&cursor->seek(static_cast<std::int64_t>(end->file_offset),mka::content::SeekOrigin::begin,why),"actual same-owner control reader");std::array<std::uint8_t,4>b{};auto read=cursor->read(b.data(),b.size(),why);check(!read.error&&read.bytes==4&&b==std::array<std::uint8_t,4>{0,0,1,0xb9},"actual original B9 bytes at indexed offset");index.reset();asset.reset();
  std::printf("{\"key\":");json_string(entry.logical_utf8);std::printf(",\"file_bytes\":%llu,\"program_end_offset\":%llu,\"program_end_bytes\":%llu,\"trailing_padding_bytes\":%llu}\n",(unsigned long long)*length,(unsigned long long)end->file_offset,(unsigned long long)end->byte_extent,(unsigned long long)end->trailing_padding_bytes);
 }
 std::puts("PASS program-end actual38 original-file observations; no lifecycle completion");
}
}
int main(int argc,char**argv){try{check(argc==1||argc==3,"usage: test_movie_program_end [asset_root catalog.tsv]");synthetic();if(argc==3)corpus(std::filesystem::u8path(argv[1]),std::filesystem::u8path(argv[2]));std::puts("PASS native program-end grammar/owned observations; no lifecycle completion");check(!std::fflush(stdout)&&!std::ferror(stdout),"completion output");return 0;}catch(const std::exception&e){std::fprintf(stderr,"FAIL program-end %s\n",e.what());return 1;}}
