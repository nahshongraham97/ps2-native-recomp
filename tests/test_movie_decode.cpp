#include "mka/movie_decode.hpp"
#include "native_test_sha256.hpp"
#include "data/movie_decode_oracle.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <stdexcept>

namespace {
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct Hash {
    std::uint64_t value=14695981039346656037ull;
    std::uint64_t bytes=0;
    NativeTestSha256 sha;
    void add(const void* p,std::size_t n){sha.add(p,n);bytes+=n;const auto* b=static_cast<const std::uint8_t*>(p);for(std::size_t i=0;i<n;++i){value^=b[i];value*=1099511628211ull;}}
};
struct Times {
    std::optional<std::int64_t> first,last;
    std::uint64_t missing=0;
    void add(const mka::video::MovieTimestamp& t){if(!t.present){++missing;return;}if(!first)first=t.value;last=t.value;check(t.numerator==1&&t.denominator==90000,"exact original native timebase");}
    void verify(const native_movie_oracle::Times& e)const {check(first&&last&&*first==e.first&&*last==e.last&&missing==e.missing,"exact independent timestamp oracle");}
};
void save(const std::filesystem::path& path,const void* data,std::size_t size){
    std::ofstream out(path,std::ios::binary);out.write(static_cast<const char*>(data),size);check(bool(out),"write owned decoded artifact");
}
}
int main(int argc,char** argv){
    using namespace mka::video;
    try{
        const bool catalog_mode=argc==5&&std::string(argv[4])=="--catalog";
        check(argc==4||catalog_mode,"usage: test_native_movie_decode library_directory movie_directory NEW_output_directory [--catalog]");
        const auto libs=std::filesystem::u8path(argv[1]),assets=std::filesystem::u8path(argv[2]),out=std::filesystem::u8path(argv[3]);
        check(std::filesystem::create_directories(out),"output must be new");
        MovieDecodeFailure f;
        check(!NativeMovieDecoder::open(out/"absent-libraries",assets/"LOGON.SFD",f)&&f.kind==MovieDecodeError::library_load,"missing native codec libraries fail by name");
        check(!NativeMovieDecoder::open(libs,out/"absent-movie.sfd",f)&&f.kind==MovieDecodeError::file_io,"missing movie fails through NativeFile");
        const char invalid[]="this is not a movie";save(out/"invalid.sfd",invalid,sizeof(invalid));
        check(!NativeMovieDecoder::open(libs,out/"invalid.sfd",f)&&f.kind==MovieDecodeError::demux,"invalid media never becomes successful empty movie");
        // Invalid media through an already-open asset must retain the same strict demux failure.
        MovieAssetFailure af;
        auto invalid_store=NativeMovieAssetStore::create(out,{{"invalid.sfd","invalid.sfd"}},af);
        check(bool(invalid_store),"invalid media catalog fixture");
        auto invalid_asset=invalid_store->open("invalid.sfd",af);check(bool(invalid_asset),"opened invalid media fixture");
        check(!NativeMovieDecoder::open(libs,std::move(*invalid_asset),f)&&f.kind==MovieDecodeError::demux,"catalog media validation not bypassed");
        check(!invalid_asset->valid(),"failed decoder consumed owned asset without reopen");
        check(!NativeMovieDecoder::open(libs,std::move(*invalid_asset),f)&&f.kind==MovieDecodeError::invalid_owner&&f.asset_failure.kind==MovieAssetError::moved_asset,"moved asset fails by exact owner error");
        for(const auto& expected:native_movie_oracle::assets){
            const auto* name=expected.name;
            std::unique_ptr<NativeMovieDecoder> decoder;
            if(catalog_mode){
                auto catalog=NativeMovieAssetStore::create(assets,{{name,name}},af);check(bool(catalog),"explicit verified test catalog");
                auto opened=catalog->open(name,af);check(bool(opened),"catalog NativeFile already opened");
                if(std::string(name)=="LOGON.SFD"){
                    std::array<std::uint8_t,64> header{};
                    const auto inspected=opened->read(header.data(),header.size(),af);
                    check(inspected.bytes==header.size()&&!inspected.error&&opened->position(af)==header.size(),"inspected asset retains nonzero cursor before decoder handoff");
                }else if(std::string(name)=="TITLEN.SFD"){
                    check(opened->seek(1,mka::content::SeekOrigin::end,af),"asset positioned past EOF before decoder handoff");
                    std::uint8_t byte=0;const auto end=opened->read(&byte,1,af);
                    check(end.bytes==0&&end.eof&&!end.error,"asset EOF flag set before decoder handoff");
                }else{
                    check(opened->seek(12345,mka::content::SeekOrigin::begin,af)&&opened->position(af)==12345,"asset explicitly sought before decoder handoff");
                }
                // Reader lifetime is independent of the catalog and consumes its existing file.
                catalog.reset();decoder=NativeMovieDecoder::open(libs,std::move(*opened),f);
                check(!opened->valid(),"decoder consumed existing asset");
                if(decoder)check(!NativeMovieDecoder::open(libs,std::move(*opened),f)&&f.kind==MovieDecodeError::invalid_owner,"moved real asset rejected");
                // The negative owner check above must not affect the independent decoder.
            }else decoder=NativeMovieDecoder::open(libs,assets/name,f);
            if(!decoder){std::fprintf(stderr,"open %s failed kind=%d native=%d operation=%s\n",name,int(f.kind),f.native_code,f.operation.c_str());return 1;}
            NativeMovieDecoder moved(std::move(*decoder));
            check(decoder->next(f).status==MovieReadStatus::error&&f.kind==MovieDecodeError::invalid_owner,"moved decoder cannot access old ownership");
            std::uint64_t videos=0,audios=0,samples=0,missing_pts=0;int peak=0;Hash vh,ah;
            Times video_pts,video_best,audio_pts,audio_best;
            std::optional<MovieVideoFrame> first_video;std::optional<MovieAudioFrame> first_audio;
            for(;;){
                auto next=moved.next(f);
                if(next.status==MovieReadStatus::error){std::fprintf(stderr,"decode %s failed kind=%d native=%d operation=%s\n",name,int(f.kind),f.native_code,f.operation.c_str());return 1;}
                if(next.status==MovieReadStatus::end)break;
                if(auto* v=std::get_if<MovieVideoFrame>(&next.frame)){
                    check(v->width==640&&v->height==448,"actual pinned movie dimensions from native probe");
                    check(v->planes[0].size()==640*448&&v->planes[1].size()==320*224&&v->planes[2].size()==320*224,"owned normalized YUV420P extent");
                    for(const auto& p:v->planes)vh.add(p.data(),p.size());
                    video_pts.add(v->source_pts);video_best.add(v->timestamp);
                    ++videos;if(!v->timestamp.present)++missing_pts;else check(v->timestamp.numerator>0&&v->timestamp.denominator>0,"video native timebase explicit");
                    if(!first_video)first_video=std::move(*v);
                }else{
                    auto& a=std::get<MovieAudioFrame>(next.frame);
                    check(a.sample_rate==48000&&a.channels.size()==2&&!a.channels[0].empty()&&a.channels[0].size()==a.channels[1].size(),"actual ADX rate/channels and owned PCM extent");
                    for(const auto& c:a.channels){ah.add(c.data(),c.size()*2);for(const auto x:c)peak=std::max(peak,x<0?-int(x):int(x));}
                    audio_pts.add(a.source_pts);audio_best.add(a.timestamp);
                    ++audios;samples+=a.channels[0].size();if(!a.timestamp.present)++missing_pts;else check(a.timestamp.numerator>0&&a.timestamp.denominator>0,"audio native timebase explicit");
                    if(!first_audio)first_audio=std::move(a);
                }
            }
            check(videos>0&&audios>0&&samples>0&&peak>0,"actual movie yields both video and nonzero PCM");
            check(videos==expected.video.frames&&audios==expected.audio.frames&&samples==expected.audio.samples,"exact independent video/audio counts including final decoder drain");
            check(vh.value==expected.video.fnv&&ah.value==expected.audio.fnv&&vh.bytes==expected.video.bytes&&ah.bytes==expected.audio.bytes,"exact independent decoded bytes/count/FNV");
            check(vh.sha.finish()==expected.video.sha256&&ah.sha.finish()==expected.audio.sha256,"SHA256 of every decoded video and PCM byte matches independent default decoder");
            video_pts.verify(expected.video.pts);video_best.verify(expected.video.best);audio_pts.verify(expected.audio.pts);audio_best.verify(expected.audio.best);
            check(moved.next(f).status==MovieReadStatus::end&&f.kind==MovieDecodeError::none,"real EOF is stable");
            Hash before;for(const auto& p:first_video->planes)before.add(p.data(),p.size());for(const auto& p:first_audio->channels)before.add(p.data(),p.size()*2);
            {NativeMovieDecoder closed(std::move(moved));} // All library frame/decoder storage is released.
            Hash after;for(const auto& p:first_video->planes)after.add(p.data(),p.size());for(const auto& p:first_audio->channels)after.add(p.data(),p.size()*2);
            check(before.value==after.value,"retained decoded planes/PCM survive decoder and DLL lifetime");
            for(unsigned p=0;p!=3;++p)save(out/(std::string(name)+".first.plane"+std::to_string(p)),first_video->planes[p].data(),first_video->planes[p].size());
            for(unsigned c=0;c!=2;++c)save(out/(std::string(name)+".first.channel"+std::to_string(c)+".s16"),first_audio->channels[c].data(),first_audio->channels[c].size()*2);
            std::printf("[native-movie-decode] %s video_frames=%llu audio_frames=%llu samples_per_channel=%llu peak=%d missing_pts=%llu yuv_fnv64=%016llX pcm_fnv64=%016llX\n",name,(unsigned long long)videos,(unsigned long long)audios,(unsigned long long)samples,peak,(unsigned long long)missing_pts,(unsigned long long)vh.value,(unsigned long long)ah.value);
        }
        std::puts("PASS native filesystem/MPEG2/ADX decoder ownership and actual media output; no game/A-V-sync/presentation/fidelity claim");
        return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
}
