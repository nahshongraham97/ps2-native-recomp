// Offline codec-frame provenance vs separately verified raw PES index.
// Native decoding only: no game, presentation, audio output or device model.
#include "mka/movie_decode.hpp"
#include "native_test_sha256.hpp"
#include "data/movie_decode_oracle.hpp"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>

using namespace mka::video;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::string utf8(const std::filesystem::path& p){const auto u=p.generic_u8string();return {reinterpret_cast<const char*>(u.data()),u.size()};}
void quote(std::ostream& o,std::string_view s){o<<'"';for(unsigned char c:s){if(c=='"'||c=='\\')o<<'\\'<<c;
    else if(c<32){constexpr char h[]="0123456789abcdef";o<<"\\u00"<<h[c>>4]<<h[c&15];}else o<<c;}o<<'"';}
void optional(std::ostream& o,const std::optional<std::uint64_t>& v){if(v)o<<*v;else o<<"null";}
struct Row{std::size_t ordinal;MoviePictureSource source;};
}
int main(int argc,char** argv){try {
    check(argc==3,"usage: test_native_movie_source_association explicit_libraries MOVIEPS2_root");
    const auto libraries=std::filesystem::u8path(argv[1]);
    const auto root=std::filesystem::canonical(std::filesystem::u8path(argv[2]));
    std::vector<std::string> keys;
    for(const auto& file:std::filesystem::recursive_directory_iterator(root)){
        if(!file.is_regular_file())continue;auto ext=utf8(file.path().extension());
        for(auto& c:ext)if(c>='a'&&c<='z')c=char(c-'a'+'A');
        if(ext==".SFD")keys.push_back(utf8(file.path().lexically_relative(root)));
    }
    std::sort(keys.begin(),keys.end());check(!keys.empty(),"no actual SFD files");
    std::vector<MovieAssetCatalogEntry> catalog;for(const auto& key:keys)catalog.push_back({key,key});
    MovieAssetFailure af;auto store=NativeMovieAssetStore::create(root,catalog,af);check(bool(store),"explicit native catalog");
    std::uint64_t total=0,absent=0,cross=0;
    for(const auto& key:keys){
        std::fprintf(stderr,"[source-association] %s\n",key.c_str());
        auto reference=store->open(key,af);check(bool(reference),"reference index asset");MoviePesFailure pf;
        auto index=NativeMoviePesIndex::build(std::move(*reference),pf);check(bool(index),"reference native PES index");
        const auto streams=index->streams();check(streams.size()==1,"fixture requires observed one-video stream");
        const auto& stream=streams.front();
        auto input=store->open(key,af);check(bool(input),"decoder asset");MovieDecodeFailure df;
        auto decoder=NativeMovieDecoder::open(libraries,std::move(*input),df);
        if(!decoder){std::cerr<<"open failure "<<int(df.kind)<<' '<<df.operation<<'\n';return 1;}
        std::map<std::uint64_t,Row> rows;
        std::uint64_t frames=0,audio_frames=0,samples=0,missing=0,crossed=0;
        const native_movie_oracle::Asset* golden=nullptr;
        for(const auto& item:native_movie_oracle::assets)if(key==item.name)golden=&item;
        std::unique_ptr<NativeTestSha256> video_hash,audio_hash;
        if(golden){video_hash=std::make_unique<NativeTestSha256>();audio_hash=std::make_unique<NativeTestSha256>();}
        for(;;){auto next=decoder->next(df);
            if(next.status==MovieReadStatus::error){std::cerr<<"decode failure "<<int(df.kind)<<' '<<df.operation<<'\n';return 1;}
            if(next.status==MovieReadStatus::end)break;
            if(auto* frame=std::get_if<MovieVideoFrame>(&next.frame)){
                ++frames;check(frame->codec_timing.picture.has_value(),"owned picture timing missing");
                check(frame->codec_timing.picture_source.has_value(),"owned picture provenance missing");
                const auto& source=*frame->codec_timing.picture_source;const auto& picture=*frame->codec_timing.picture;
                check(source.picture_es_byte>=source.packet_es_begin &&
                    source.picture_es_byte-source.packet_es_begin==picture.packet_picture_offset,"packet-relative picture offset");
                const auto span=std::upper_bound(stream.spans.begin(),stream.spans.end(),source.picture_es_byte,
                    [](std::uint64_t p,const auto& s){return p<s.es_begin;});
                check(span!=stream.spans.begin(),"prefix precedes stream");const auto owner=std::prev(span);
                check(source.picture_es_byte>=owner->es_begin && source.picture_es_byte<owner->es_end,"prefix first-byte range");
                check(source.pes==*owner,"frame PES provenance differs from independently verified index");
                check(source.quarter_pts==owner->quarter_pts(),"raw quarter timestamp preservation");
                check(rows.emplace(source.picture_es_byte,Row{std::size_t(owner-stream.spans.begin()),source}).second,"duplicate frame source prefix");
                if(!source.pes.pts33)++missing;
                if(source.pes.es_end-source.picture_es_byte<4)++crossed;
                if(video_hash)for(const auto& plane:frame->planes)video_hash->add(plane.data(),plane.size());
            }else{
                const auto& a=std::get<MovieAudioFrame>(next.frame);++audio_frames;
                if(!a.channels.empty())samples+=a.channels.front().size();
                if(audio_hash)for(const auto& plane:a.channels)audio_hash->add(plane.data(),plane.size()*sizeof(std::int16_t));
            }
        }
        // Sort observations by input ES offset only for comparison; decoded
        // frames and actual codec output retain their original display order.
        std::ostringstream compact;compact<<'[';bool first=true;
        for(const auto& [offset,row]:rows){if(!first)compact<<',';first=false;
            compact<<'['<<offset<<','<<row.ordinal<<',';optional(compact,row.source.pes.pts33);compact<<',';
            optional(compact,row.source.quarter_pts);compact<<']';}
        compact<<']';const auto text=compact.str();NativeTestSha256 hash;hash.add(text.data(),text.size());
        const auto mapping=hash.finish();bool golden_match=true;
        if(golden){golden_match=frames==golden->video.frames && audio_frames==golden->audio.frames &&
            samples==golden->audio.samples && video_hash->finish()==golden->video.sha256 && audio_hash->finish()==golden->audio.sha256;}
        std::cout<<"{\"file\":";quote(std::cout,key);std::cout<<",\"frames\":"<<frames<<",\"absent\":"<<missing
            <<",\"cross\":"<<crossed<<",\"picture_pes_mapping_sha256\":";quote(std::cout,mapping);
        std::cout<<",\"golden_present\":"<<(golden?"true":"false")<<",\"golden_match\":"<<(golden_match?"true":"false")<<"}\n"<<std::flush;
        check(golden_match,"independent full YUV/PCM/count golden changed");total+=frames;absent+=missing;cross+=crossed;
    }
    std::cout<<"{\"complete\":true,\"assets\":"<<keys.size()<<",\"frames\":"<<total<<",\"absent\":"<<absent<<",\"cross\":"<<cross<<"}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"native source association FAIL: "<<e.what()<<'\n';return 1;}}
