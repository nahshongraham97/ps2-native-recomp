// Source composition only. No player, clocks, presentation or audio output.
#include "mka/movie_source.hpp"
#include "mka/movie_pes_index.hpp"
#include "native_test_sha256.hpp"
#include "data/movie_decode_oracle.hpp"
#include <charconv>
#include <cstdio>
#include <fstream>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
using namespace mka::video;
namespace {
void check(bool value,const char* label) {if(!value) throw std::runtime_error(label);}
struct Hash {
    std::uint64_t fnv=14695981039346656037ull,bytes=0;
    NativeTestSha256 sha;
    void add(const void* p,std::size_t n) {
        sha.add(p,n);bytes+=n;const auto* b=static_cast<const std::uint8_t*>(p);
        for(std::size_t i=0;i<n;++i) {fnv^=b[i];fnv*=1099511628211ull;}
    }
};
struct Times {
    std::optional<std::int64_t> first,last;std::uint64_t missing=0;
    void add(const MovieTimestamp& t) {
        if(!t.present) {++missing;return;}
        check(t.numerator==1 && t.denominator==90000,"explicit codec timestamp timebase");
        if(!first) first=t.value;last=t.value;
    }
    void verify(const native_movie_oracle::Times& e) const {
        check(first && last && *first==e.first && *last==e.last && missing==e.missing,"independent timestamp golden");
    }
};
std::vector<MovieAssetCatalogEntry> bindings(const char* path) {
    std::ifstream file(path,std::ios::binary);check(bool(file),"open explicit38-binding fixture manifest");
    std::vector<MovieAssetCatalogEntry> entries;std::set<std::string> keys;std::string line;
    while(std::getline(file,line)) {
        if(!line.empty() && line.back()=='\r') line.pop_back();
        std::array<std::string_view,4> fields{};std::string_view tail(line);
        for(std::size_t i=0;i<4;++i) {
            auto tab=tail.find('\t');
            if(i==3) {check(tab==tail.npos,"extra manifest field");fields[i]=tail;}
            else {check(tab!=tail.npos,"missing manifest field");fields[i]=tail.substr(0,tab);tail.remove_prefix(tab+1);}
        }
        check(!fields[0].empty() && keys.emplace(fields[0]).second,"nonempty unique fixture key");
        std::uint64_t bytes=0;auto parsed=std::from_chars(fields[1].data(),fields[1].data()+fields[1].size(),bytes);
        check(parsed.ec==std::errc{} && parsed.ptr==fields[1].data()+fields[1].size() && bytes>=8,"manifest byte extent");
        for(auto text:{fields[2],fields[3]}) {
            check(text.size()==16,"manifest endpoint byte identity");
            for(char c:text) check((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F'),"manifest hex identity");
        }
        entries.push_back({std::string(fields[0]),std::string(fields[0])});
    }
    check(file.eof()&&!file.bad()&&entries.size()==38,"complete38-binding fixture manifest");return entries;
}
std::unique_ptr<NativeRetailMovieSource> open(const NativeMovieAssetStore& store,
        const std::filesystem::path& libs,std::int64_t id,MovieViewSelection view,std::int64_t code=0) {
    MovieSourceFailure f;auto source=NativeRetailMovieSource::open(store,libs,id,view,code,f);
    if(!source) {
        std::fprintf(stderr,"source open ID=%lld kind=%s decoder=%d operation=%s\n",static_cast<long long>(id),
            movie_source_error_name(f.kind),int(f.decoder_failure.kind),f.decoder_failure.operation.c_str());
        throw std::runtime_error("native source open");
    }
    check(f.kind==MovieSourceError::none&&source->valid()&&source->request(),"published owned source");return source;
}
MovieVideoFrame first_video(NativeRetailMovieSource& source) {
    MovieSourceFailure f;
    for(;;) {auto item=source.next(f);check(item.status==MovieReadStatus::frame&&f.kind==MovieSourceError::none,"first video may follow audio");
        if(auto* v=std::get_if<MovieVideoFrame>(&item.frame)) return std::move(*v);}
}
void association(const NativeMoviePesIndex& index,std::uint8_t stream,const MovieVideoFrame& v) {
    check(v.codec_timing.picture && v.codec_timing.picture_source,"owned picture/source metadata");
    const auto& source=*v.codec_timing.picture_source;
    check(source.picture_es_byte>=source.packet_es_begin &&
        source.picture_es_byte-source.packet_es_begin==v.codec_timing.picture->packet_picture_offset,"picture input offset exact");
    MoviePesFailure f;auto owner=index.lookup_picture_prefix(stream,source.picture_es_byte,f);
    check(owner && f.kind==MoviePesError::none && *owner==source.pes && source.quarter_pts==owner->quarter_pts(),
        "picture firstbyte matches separately owned actual source index, no bestPTS inference");
}
}
int main(int argc,char**argv) try {
    check(argc==5,"usage: test_native_movie_source libraries MOVIEPS2root verified38assets.tsv NEWfixtureoutput");
    const auto libs=std::filesystem::u8path(argv[1]),root=std::filesystem::u8path(argv[2]),out=std::filesystem::u8path(argv[4]);
    check(std::filesystem::create_directories(out),"fixture output must be fresh/nonexistent");
    const auto entries=bindings(argv[3]);MovieAssetFailure af;
    auto store=NativeMovieAssetStore::create(root,entries,af);check(bool(store),"explicit actual38 asset bindings");
    const auto badlibs=out/"absent-native-libraries";MovieSourceFailure f;
    for(std::int64_t id:std::array<std::int64_t,4>{-1,33,INT64_MIN,INT64_MAX}) {
        check(!NativeRetailMovieSource::open(*store,badlibs,id,MovieViewSelection::normal,0,f) &&
              f.kind==MovieSourceError::catalog&&f.catalog_failure.kind==MovieCatalogError::invalid_id,"invalidID before file/library");
    }
    check(!NativeRetailMovieSource::open(*store,badlibs,0,static_cast<MovieViewSelection>(99),0,f)&&
          f.kind==MovieSourceError::catalog&&f.catalog_failure.kind==MovieCatalogError::invalid_view,"invalidview before file/library");
    unsigned missing=0;
    for(auto pair:std::array<std::pair<int,MovieViewSelection>,4>{{{7,MovieViewSelection::normal},{7,MovieViewSelection::alternate},
            {31,MovieViewSelection::normal},{31,MovieViewSelection::alternate}}}) {
        check(!NativeRetailMovieSource::open(*store,badlibs,pair.first,pair.second,0,f)&&f.kind==MovieSourceError::asset&&
              f.asset_failure.kind==MovieAssetError::unknown_key&&f.decoder_failure.kind==MovieDecodeError::none,"exact missing key before codec");++missing;
    }
    check(!NativeRetailMovieSource::open(*store,badlibs,0,MovieViewSelection::normal,0,f)&&
          f.kind==MovieSourceError::decoder&&f.decoder_failure.kind==MovieDecodeError::library_load&&
          !f.decoder_failure.operation.empty(),"good asset/bad codec preserves detailed decoder error");
    {std::ofstream invalid(out/"invalid.sfd",std::ios::binary);invalid<<"not a movie packet";check(bool(invalid),"write new owned invalid fixture");}
    auto invalid_store=NativeMovieAssetStore::create(out,{{"LOGON.SFD","invalid.sfd"}},af);check(bool(invalid_store),"explicit invalid synthetic binding");
    check(!NativeRetailMovieSource::open(*invalid_store,badlibs,0,MovieViewSelection::normal,0,f)&&
          f.kind==MovieSourceError::decoder&&f.decoder_failure.kind==MovieDecodeError::source_index&&
          f.decoder_failure.source_failure.kind==MoviePesError::bad_prefix&&f.decoder_failure.source_failure.offset==0,
          "actual synthetic owned binding indexed before DLL, no retail path reopen");invalid_store.reset();
    std::set<std::string> done;unsigned goldens=0;std::uint64_t videos_total=0;
    for(std::int64_t id=0;id<33;++id) for(auto view:{MovieViewSelection::normal,MovieViewSelection::alternate}) {
        MovieCatalogFailure cf;auto semantic_request=RetailMovieCatalog{}.resolve(id,view,0,cf);check(bool(semantic_request),"semantic request");
        if(semantic_request->logical_key=="BLITZN.SFD"||semantic_request->logical_key=="BLITZW.SFD"||semantic_request->logical_key=="KRYPT/V_FIN.SFD") continue;
        if(!done.insert(semantic_request->logical_key).second) continue;
        auto source=open(*store,libs,id,view,3);
        auto* self=source.get();*source=std::move(*self);
        check(source->valid()&&source->request()&&source->request()->logical_key==semantic_request->logical_key,
              "self-move retains request/decoder ownership identity");
        check(source->request()->logical_key==semantic_request->logical_key&&source->request()->logical_extent.width==semantic_request->logical_extent.width&&
              source->request()->logical_extent.height==semantic_request->logical_extent.height&&source->request()->subtitle_tag=="<FRE>","owned exact request metadata");
        auto moved=std::make_unique<NativeRetailMovieSource>(std::move(*source));
        check(!source->valid()&&!source->request()&&source->next(f).status==MovieReadStatus::error&&
              f.kind==MovieSourceError::invalid_owner,"moved source fails named");source=std::move(moved);
        auto index_asset=store->open(semantic_request->logical_key,af);check(bool(index_asset),"independent opened source index");
        MoviePesFailure pf;auto index=NativeMoviePesIndex::build(std::move(*index_asset),pf);check(bool(index),"independent PES index");
        const auto streams=index->streams();check(streams.size()==1,"actual onevideo stream");
        const native_movie_oracle::Asset* golden=nullptr;
        for(const auto& candidate:native_movie_oracle::assets) if(semantic_request->logical_key==candidate.name) golden=&candidate;
        Hash vh,ah;Times vp,vb,ap,ab;std::uint64_t videos=0,audios=0,samples=0;
        std::optional<MovieVideoFrame> retained_video;std::optional<MovieAudioFrame> retained_audio;
        for(;;) {
            auto item=source->next(f);check(f.kind==MovieSourceError::none&&item.status!=MovieReadStatus::error,"source decode next");
            if(item.status==MovieReadStatus::end) {check(golden!=nullptr,"nongolden must provide firstvideo before EOF");break;}
            if(auto* v=std::get_if<MovieVideoFrame>(&item.frame)) {
                association(*index,streams[0].stream_id,*v);++videos;
                for(const auto& plane:v->planes) vh.add(plane.data(),plane.size());vp.add(v->source_pts);vb.add(v->timestamp);
                if(!retained_video) retained_video=std::move(*v);
                if(!golden) break;
            } else {
                auto& a=std::get<MovieAudioFrame>(item.frame);++audios;
                check(a.sample_rate==48000&&a.channels.size()==2&&!a.channels[0].empty()&&a.channels[0].size()==a.channels[1].size(),"actual ADX ownedplanes");
                check(a.channel_layout.order==MovieAudioChannelOrder::unspecified&&a.channel_layout.channel_count==2&&
                      !a.channel_layout.mask&&a.channel_layout.channels.empty(),"actual ADX UNSPEC no stereo inference");
                samples+=a.channels[0].size();for(const auto& plane:a.channels) ah.add(plane.data(),plane.size()*sizeof(std::int16_t));
                ap.add(a.source_pts);ab.add(a.timestamp);if(!retained_audio) retained_audio=std::move(a);
            }
        }
        const auto video_sha=vh.sha.finish(),audio_sha=ah.sha.finish();
        if(golden) {
            check(videos==golden->video.frames&&audios==golden->audio.frames&&samples==golden->audio.samples,"full independent count golden");
            check(vh.bytes==golden->video.bytes&&ah.bytes==golden->audio.bytes&&vh.fnv==golden->video.fnv&&ah.fnv==golden->audio.fnv&&
                  video_sha==golden->video.sha256&&audio_sha==golden->audio.sha256,"all full decoded YUV/PCM independent SHA/FNV/bytes");
            vp.verify(golden->video.pts);vb.verify(golden->video.best);ap.verify(golden->audio.pts);ab.verify(golden->audio.best);
            check(source->next(f).status==MovieReadStatus::end&&f.kind==MovieSourceError::none,"EOF stable after full drain");++goldens;
        }
        check(retained_video.has_value(),"owned video retained");
        if(id==0&&view==MovieViewSelection::normal) {
            auto second=open(*store,libs,id,view);auto second_video=first_video(*second);
            check(second_video.planes==retained_video->planes,"independent secondsource beginsatfirst frame");
        }
        Hash before;for(const auto& plane:retained_video->planes) before.add(plane.data(),plane.size());
        if(retained_audio) for(const auto& plane:retained_audio->channels) before.add(plane.data(),plane.size()*2);
        source.reset();Hash after;for(const auto& plane:retained_video->planes) after.add(plane.data(),plane.size());
        if(retained_audio) for(const auto& plane:retained_audio->channels) after.add(plane.data(),plane.size()*2);
        check(before.sha.finish()==after.sha.finish(),"owned frames survive source/decoder release");
        videos_total+=videos;
        const auto& first_source=*retained_video->codec_timing.picture_source;
        const auto pts_text=first_source.pes.pts33?std::to_string(*first_source.pes.pts33):"null";
        const auto quarter_text=first_source.quarter_pts?std::to_string(*first_source.quarter_pts):"null";
        std::printf("{\"kind\":\"asset\",\"key\":\"%s\",\"id\":%lld,\"view\":\"%s\",\"golden_present\":%s,\"golden_match\":%s,\"full_video_frames\":%llu,\"observed_audio_frames\":%llu,\"observed_samples_per_channel\":%llu,\"yuv_sha256\":\"%s\",\"pcm_sha256\":\"%s\",\"first_video_es\":%llu,\"first_video_pes_packet\":%llu,\"first_video_pes_pts33\":%s,\"first_video_quarter_pts\":%s,\"scope\":\"%s\"}\n",
            semantic_request->logical_key.c_str(),static_cast<long long>(id),view==MovieViewSelection::normal?"normal":"alternate",
            golden?"true":"false",golden?"true":"null",static_cast<unsigned long long>(videos),static_cast<unsigned long long>(audios),
            static_cast<unsigned long long>(samples),video_sha.c_str(),audio_sha.c_str(),
            static_cast<unsigned long long>(first_source.picture_es_byte),static_cast<unsigned long long>(first_source.pes.packet_offset),
            pts_text.c_str(),quarter_text.c_str(),golden?"full-golden":"first-video-only");
        check(std::fflush(stdout)==0&&!std::ferror(stdout),"asset result output");
    }
    check(done.size()==38&&goldens==3&&missing==4,"complete unique asset/golden/missing counts");
    auto retained_source=open(*store,libs,0,MovieViewSelection::normal,1);store.reset();
    check(retained_source->request()->subtitle_tag=="<SPA>","ownedrequest after entirestore destruction");
    auto independent_frame=first_video(*retained_source);check(!independent_frame.planes[0].empty(),"actual read after store destruction");
    retained_source.reset();
    std::printf("{\"kind\":\"complete\",\"assets\":38,\"full_goldens\":3,\"first_video_only\":35,\"missing_requests\":4,\"observed_video_frames\":%llu,\"scope\":\"owned source composition only; no player/clock/mixer/game acceptance\"}\n",static_cast<unsigned long long>(videos_total));
    check(std::fflush(stdout)==0&&!std::ferror(stdout),"summary output");return 0;
} catch(const std::exception& error) {std::fprintf(stderr,"FAIL movie source: %s\n",error.what());return 1;}
