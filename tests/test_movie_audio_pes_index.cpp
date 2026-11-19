#include "mka/movie_pes_index.hpp"
#include <array>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <stdexcept>
using namespace mka::video;
using Bytes=std::vector<std::uint8_t>;
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
Bytes stamp(std::uint64_t t,unsigned prefix){return {std::uint8_t(prefix*16|((t>>29)&14)|1),std::uint8_t(t>>22),std::uint8_t((t>>14)&254|1),std::uint8_t(t>>7),std::uint8_t((t<<1)&254|1)};}
void packet(Bytes& b,unsigned id,const Bytes& h,const Bytes& p){const auto n=h.size()+p.size();b.insert(b.end(),{0,0,1,std::uint8_t(id),std::uint8_t(n>>8),std::uint8_t(n)});b.insert(b.end(),h.begin(),h.end());b.insert(b.end(),p.begin(),p.end());}
void write(const std::filesystem::path& p,const Bytes& b){std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),b.size());check(bool(f),"fixture write");}
int main(){try{
    const auto parent=std::filesystem::canonical(std::filesystem::temp_directory_path());const auto root=parent/("mka-audio-pes-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));check(std::filesystem::create_directory(root),"exclusive temp directory");
    struct Cleanup{std::filesystem::path path,parent;~Cleanup(){std::error_code ec;const auto actual=std::filesystem::canonical(path,ec);if(!ec&&actual.is_absolute()&&actual==path&&actual.parent_path()==parent)std::filesystem::remove_all(actual,ec);}} cleanup{root,parent};
    Bytes data{0,0,1,0xba,0x21,0,1,0,1,0x80,0,1};
    auto pts=stamp((1ull<<33)-1,3),dts=stamp(123,1);pts.insert(pts.end(),dts.begin(),dts.end());packet(data,0xc0,pts,{0,0});packet(data,0xe0,{15},{0,0,1,0,9});packet(data,0xc1,stamp(4,2),{7,8,9});packet(data,0xc0,{15},{1,0,55,66});data.insert(data.end(),{0,0,1,0xb9,255});write(root/"mixed.sfd",data);
    MovieAssetFailure af;auto store=NativeMovieAssetStore::create(root,{{"mixed","mixed.sfd"}},af);check(bool(store),"explicit store");auto asset=store->open("mixed",af);check(asset&&asset->seek(9,mka::content::SeekOrigin::begin,af),"opened actual asset/nonzero cursor");MoviePesFailure f;auto index=NativeMoviePesIndex::build(std::move(*asset),f);check(bool(index),"mixed audio/video index");
    auto video=index->streams(),audio=index->audio_streams();check(video.size()==1&&video[0].stream_id==0xe0&&video[0].bytes==5,"legacy video-only enumeration");check(audio.size()==2&&audio[0].stream_id==0xc0&&audio[0].bytes==6&&audio[1].stream_id==0xc1&&audio[1].bytes==3,"independent dynamic audio IDs");
    auto first=index->lookup(0xc0,0,f);check(first&&first->pts33==(1ull<<33)-1&&first->dts33==123&&first->quarter_pts()==(((1ull<<33)-1)>>2),"raw33 audio timestamps");auto second=index->lookup(0xc0,2,f);check(second&&!second->pts33&&!second->dts33,"absent audio timestamp not inherited");check(index->lookup(0xc1,0,f)->pts33==4,"other stream timestamp independent");check(!index->lookup(0xc0,6,f)&&f.kind==MoviePesError::range,"half-open audio range");
    std::array<std::uint8_t,6> bytes{};check(index->read_range(0xc0,0,bytes,f)&&bytes==std::array<std::uint8_t,6>{0,0,1,0,55,66},"exact audio bytes across video interleave");check(!index->lookup_picture_prefix(0xc0,0,f)&&f.kind==MoviePesError::invalid_stream,"audio prefix cannot become video picture");check(index->lookup_picture_prefix(0xe0,0,f).has_value(),"video picture behavior retained");
    auto source=index->file_reader(f);check(source&&source->seek(4,mka::content::SeekOrigin::begin,f),"same-open independent file cursor");auto reader=index->reader(0xc0,f);check(bool(reader),"audio advancing reader");auto wrong=bytes;wrong[4]=0;check(!reader->validate_next(wrong,f)&&f.kind==MoviePesError::byte_mismatch&&reader->position()==0,"audio mismatch atomic");check(source->position()==4,"range validation leaves independent source cursor unchanged");
    std::array<std::uint8_t,4> keep{7,7,7,7};check(!index->read_range(0xc1,1,keep,f)&&keep==std::array<std::uint8_t,4>{7,7,7,7},"out of range output unpublished");check(!index->reader(0xc2,f)&&f.kind==MoviePesError::invalid_stream,"missing audio stream named");
    std::optional<NativeMoviePesIndex> moved(std::move(*index));check(!index->reader(0xc0,f)&&f.kind==MoviePesError::moved,"moved index rejects");check(moved->audio_streams()[0].bytes==6,"moved owner enumeration");index.reset();moved.reset();store.reset();asset.reset();check(reader->validate_next(bytes,f)&&reader->finished(),"reader retains opened backing after owners released");std::uint8_t byte=0;check(source->read(&byte,1,f).bytes==1&&byte==data[4],"file cursor retains same source");
    auto store2=NativeMovieAssetStore::create(root,{{"mixed","mixed.sfd"}},af);check(bool(store2),"negative source store");
    auto malformed=data;malformed[18]&=254;write(root/"mixed.sfd",malformed);auto bad=store2->open("mixed",af);check(bad&&!NativeMoviePesIndex::build(std::move(*bad),f)&&f.kind==MoviePesError::bad_timestamp,"audio timestamp markers validated");
    write(root/"mixed.sfd",data);auto again=store2->open("mixed",af);auto retained=NativeMoviePesIndex::build(std::move(*again),f);check(bool(retained),"physical truncation setup");auto validation=retained->reader(0xc0,f);std::filesystem::resize_file(root/"mixed.sfd",12);keep={7,7,7,7};check(!retained->read_range(0xc0,0,keep,f)&&keep==std::array<std::uint8_t,4>{7,7,7,7},"physical short read publishes no bytes");check(!validation->validate_next(bytes,f)&&validation->position()==0,"physical truncation does not advance audio validation");
    std::puts("PASS same-owned audio PES spans/readers; no decoder or timestamp-clock binding");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL audio PES %s\n",e.what());return 1;}}
