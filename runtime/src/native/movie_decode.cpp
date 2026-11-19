#include "mka/movie_decode.hpp"
#include "mka/native_file.hpp"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#include <bit>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <deque>
#include <limits>
#include <new>
#include <optional>
#include <stdexcept>
#include <unordered_map>
extern "C" {
#include <libavutil/buffer.h>
}
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace mka::video {
namespace {
struct Api {
#ifdef _WIN32
    HMODULE utility=nullptr,codec=nullptr,format=nullptr;
    ~Api(){if(format)FreeLibrary(format);if(codec)FreeLibrary(codec);if(utility)FreeLibrary(utility);}
#endif
#define DECLARE(name) decltype(&name) name##_p=nullptr
    DECLARE(avutil_version);DECLARE(avcodec_version);DECLARE(avformat_version);
    DECLARE(av_malloc);DECLARE(av_free);DECLARE(av_frame_alloc);DECLARE(av_frame_free);DECLARE(av_frame_unref);DECLARE(av_frame_get_side_data);DECLARE(av_buffer_create);DECLARE(av_buffer_unref);
    DECLARE(av_packet_alloc);DECLARE(av_packet_free);DECLARE(av_packet_unref);DECLARE(av_new_packet);DECLARE(av_packet_copy_props);
    DECLARE(avio_alloc_context);DECLARE(avio_context_free);
    DECLARE(avformat_alloc_context);DECLARE(avformat_open_input);DECLARE(avformat_find_stream_info);
    DECLARE(avformat_close_input);DECLARE(av_read_frame);
    DECLARE(avcodec_find_decoder);DECLARE(avcodec_alloc_context3);DECLARE(avcodec_parameters_to_context);
    DECLARE(avcodec_open2);DECLARE(avcodec_free_context);DECLARE(avcodec_send_packet);DECLARE(avcodec_receive_frame);
#undef DECLARE
    bool load(const std::filesystem::path& dir,MovieDecodeFailure& f){
#ifdef _WIN32
        static_assert(sizeof(void*)==8&&std::endian::native==std::endian::little);
        const auto flags=LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS;
        const auto module=[&](const wchar_t* name){
            auto value=LoadLibraryExW((dir/name).c_str(),nullptr,flags);
            if(!value)f={MovieDecodeError::library_load,static_cast<int>(GetLastError()),"LoadLibraryExW pinned native media library"};
            return value;
        };
        utility=module(L"avutil-59.dll");if(!utility)return false;
        codec=module(L"avcodec-61.dll");if(!codec)return false;
        format=module(L"avformat-61.dll");if(!format)return false;
#define LOAD(module,name) name##_p=reinterpret_cast<decltype(name##_p)>(GetProcAddress(module,#name));if(!name##_p){f={MovieDecodeError::missing_symbol,0,#name};return false;}
        LOAD(utility,avutil_version);LOAD(codec,avcodec_version);LOAD(format,avformat_version);
        LOAD(utility,av_malloc);LOAD(utility,av_free);LOAD(utility,av_frame_alloc);LOAD(utility,av_frame_free);LOAD(utility,av_frame_unref);
        LOAD(utility,av_frame_get_side_data);LOAD(utility,av_buffer_create);LOAD(utility,av_buffer_unref);
        LOAD(codec,av_packet_alloc);LOAD(codec,av_packet_free);LOAD(codec,av_packet_unref);
        LOAD(codec,av_new_packet);LOAD(codec,av_packet_copy_props);
        LOAD(format,avio_alloc_context);LOAD(format,avio_context_free);LOAD(format,avformat_alloc_context);
        LOAD(format,avformat_open_input);LOAD(format,avformat_find_stream_info);LOAD(format,avformat_close_input);LOAD(format,av_read_frame);
        LOAD(codec,avcodec_find_decoder);LOAD(codec,avcodec_alloc_context3);LOAD(codec,avcodec_parameters_to_context);
        LOAD(codec,avcodec_open2);LOAD(codec,avcodec_free_context);LOAD(codec,avcodec_send_packet);LOAD(codec,avcodec_receive_frame);
#undef LOAD
        if(avutil_version_p()!=LIBAVUTIL_VERSION_INT||avcodec_version_p()!=LIBAVCODEC_VERSION_INT||avformat_version_p()!=LIBAVFORMAT_VERSION_INT){
            f={MovieDecodeError::library_version,0,"runtime versions must match pinned public headers"};return false;}
        return true;
#else
        f={MovieDecodeError::unsupported_host,0,"native media loader currently requires Windows x64"};return false;
#endif
    }
};
MovieTimestamp timestamp(std::int64_t value,AVRational base){
    return {value==AV_NOPTS_VALUE?0:value,base.num,base.den,value!=AV_NOPTS_VALUE};
}
}
struct NativeMovieDecoder::Impl {
    Api api;
    std::optional<NativeMoviePesIndex> source_index;
    std::optional<MovieSourceCursor> source_file;
    std::optional<MovieEsReader> source_video;
    std::uint8_t source_stream_id=0;
    MovieAssetFailure asset_error;
    MoviePesFailure source_error;
    bool source_failure(MovieDecodeError kind,const char* operation,MovieDecodeFailure& f){
        f={kind,static_cast<int>(source_error.kind),operation};f.source_failure=source_error;f.asset_failure=source_error.asset;return false;
    }
    content::FileRead read_bytes(void* data,std::size_t size){
        auto r=source_file->read(data,size,source_error);asset_error=source_error.asset;return r;
    }
    bool length(std::uint64_t& n,std::error_code& ec){
        auto value=source_file->length(source_error);ec=source_error.asset.host_error;asset_error=source_error.asset;if(!value)return false;n=*value;return true;
    }
    bool seek_position(std::int64_t offset,content::SeekOrigin origin,std::uint64_t& n,std::error_code& ec){
        const bool ok=source_file->seek(offset,origin,source_error);ec=source_error.asset.host_error;asset_error=source_error.asset;if(!ok)return false;n=source_file->position();return true;
    }
    AVIOContext* io=nullptr;
    AVFormatContext* format=nullptr;
    AVPacket* packet=nullptr;
    AVFrame* scratch=nullptr;
    struct Stream {int index=-1;AVCodecContext* decoder=nullptr;AVRational base{};};
    Stream video,audio;
    NativeMpeg2Timing timing;
    struct PictureTag {
        static constexpr std::uint64_t magic_value=0x4d4b415049435431ull;
        std::uint64_t magic=magic_value;
        const Impl* owner=nullptr;
        std::uint64_t serial=0;
        Mpeg2PictureTiming picture;
        MoviePictureSource source;
    };
    struct PendingPicture {std::uint64_t serial;Mpeg2PictureTiming picture;MoviePictureSource source;};
    std::unordered_map<const std::uint8_t*,PendingPicture> pending_pictures;
    std::uint64_t picture_serial=0;
    static void free_picture_tag(void*,std::uint8_t* data) noexcept {
        delete reinterpret_cast<PictureTag*>(data);
    }
    bool timing_failure(const Mpeg2TimingFailure& why,MovieDecodeFailure& f){
        f={MovieDecodeError::timing_parse,static_cast<int>(why.kind),
           std::string("native MPEG2 timing ")+mpeg2_timing_error_name(why.kind)+" at byte "+std::to_string(why.offset)};
        return false;
    }
    bool associate_picture(MovieVideoFrame& frame,MovieDecodeFailure& f){
        const auto* ref=scratch->opaque_ref;
        if(!ref||!ref->data||ref->size!=sizeof(PictureTag)){
            f={MovieDecodeError::timing_association,0,"missing or invalid native picture tag extent"};return false;
        }
        // Check registered address before treating arbitrary codec opaque bytes
        // as our C++ type. AVFrame holds a live AVBufferRef throughout this read.
        const auto it=pending_pictures.find(ref->data);
        if(it==pending_pictures.end()){
            f={MovieDecodeError::timing_association,0,"foreign, duplicate or unmatched native picture tag"};return false;
        }
        const auto* tag=reinterpret_cast<const PictureTag*>(ref->data);
        if(tag->magic!=PictureTag::magic_value||tag->owner!=this||!tag->serial||
           tag->serial!=it->second.serial||scratch->opaque!=tag||tag->picture!=it->second.picture||tag->source!=it->second.source){
            f={MovieDecodeError::timing_association,0,"native picture tag type/owner/serial/value mismatch"};return false;
        }
        // MPEG2 coding_type 1/2/3 and AVPictureType I/P/B share the documented
        // values. The codec's independent output classification also checks
        // association through B-picture reordering, beyond our own tag copies.
        if(tag->picture.coding_type!=static_cast<unsigned>(scratch->pict_type)){
            f={MovieDecodeError::timing_association,0,"native picture tag disagrees with codec output picture type"};return false;
        }
        frame.codec_timing.picture=tag->picture;
        frame.codec_timing.picture_source=tag->source;
        pending_pictures.erase(it);
        return true;
    }
    std::deque<MovieDecodedFrame> ready;
    std::error_code io_error;
    bool ended=false,failed=false;
    ~Impl(){
        if(scratch)api.av_frame_free_p(&scratch);
        if(packet)api.av_packet_free_p(&packet);
        if(video.decoder)api.avcodec_free_context_p(&video.decoder);
        if(audio.decoder)api.avcodec_free_context_p(&audio.decoder);
        if(format)api.avformat_close_input_p(&format);
        if(io){api.av_free_p(io->buffer);api.avio_context_free_p(&io);}
    }
    static int read(void* opaque,std::uint8_t* buffer,int size){
        auto& s=*static_cast<Impl*>(opaque);
        if(size<=0)return AVERROR(EINVAL);
        const auto r=s.read_bytes(buffer,static_cast<std::size_t>(size));
        if(r.error){s.io_error=r.error;return AVERROR(EIO);}
        if(r.bytes)return static_cast<int>(r.bytes);
        return r.eof?AVERROR_EOF:AVERROR(EIO);
    }
    static std::int64_t seek(void* opaque,std::int64_t offset,int whence){
        auto& s=*static_cast<Impl*>(opaque);std::error_code ec;
        whence&=~AVSEEK_FORCE;
        if(whence==AVSEEK_SIZE){
            std::uint64_t size;if(!s.length(size,ec)){s.io_error=ec?ec:std::make_error_code(std::errc::io_error);return AVERROR(EIO);}
            return size>std::uint64_t(INT64_MAX)?AVERROR(EOVERFLOW):static_cast<std::int64_t>(size);
        }
        content::SeekOrigin origin;
        if(whence==SEEK_SET)origin=content::SeekOrigin::begin;
        else if(whence==SEEK_CUR)origin=content::SeekOrigin::current;
        else if(whence==SEEK_END)origin=content::SeekOrigin::end;
        else return AVERROR(EINVAL);
        std::uint64_t position;
        if(!s.seek_position(offset,origin,position,ec)){s.io_error=ec?ec:std::make_error_code(std::errc::io_error);return AVERROR(EIO);}
        return position>std::uint64_t(INT64_MAX)?AVERROR(EOVERFLOW):static_cast<std::int64_t>(position);
    }
    bool open_stream(Stream& stream,unsigned i,MovieDecodeFailure& f){
        auto* source=format->streams[i];const auto* decoder=api.avcodec_find_decoder_p(source->codecpar->codec_id);
        if(!decoder){f={MovieDecodeError::unsupported_stream,source->codecpar->codec_id,"native codec unavailable"};return false;}
        stream.index=static_cast<int>(i);stream.base=source->time_base;stream.decoder=api.avcodec_alloc_context3_p(decoder);
        if(!stream.decoder){f={MovieDecodeError::allocation,0,"avcodec_alloc_context3"};return false;}
        int r=api.avcodec_parameters_to_context_p(stream.decoder,source->codecpar);
        if(r<0){f={MovieDecodeError::decode,r,"avcodec_parameters_to_context"};return false;}
        stream.decoder->pkt_timebase=source->time_base;
        if(source->codecpar->codec_type==AVMEDIA_TYPE_VIDEO){
            if(source->id<0x1e0||source->id>0x1ef){
                f={MovieDecodeError::source_association,source->id,"unverified MPEGPS video stream ID encoding"};return false;
            }
            source_stream_id=static_cast<std::uint8_t>(source->id&0xff);
            source_video=source_index->reader(source_stream_id,source_error);
            if(!source_video)return source_failure(MovieDecodeError::source_association,"MPEGPS video stream absent from same-file PES index",f);
            stream.decoder->flags|=AV_CODEC_FLAG_COPY_OPAQUE;
            if(stream.decoder->extradata_size<0||
               (stream.decoder->extradata_size&&!stream.decoder->extradata)){
                f={MovieDecodeError::timing_parse,0,"invalid MPEG2 timing extradata extent"};return false;
            }
            if(stream.decoder->extradata_size){
                Mpeg2TimingFailure why;
                if(!timing.prime({stream.decoder->extradata,static_cast<std::size_t>(stream.decoder->extradata_size)},why))return timing_failure(why,f);
            }
        }
        stream.decoder->err_recognition=AV_EF_CRCCHECK|AV_EF_BITSTREAM|AV_EF_BUFFER|AV_EF_EXPLODE;
        r=api.avcodec_open2_p(stream.decoder,decoder,nullptr);
        if(r<0){f={MovieDecodeError::decode,r,"avcodec_open2"};return false;}
        return true;
    }
    bool receive(Stream& stream,MovieDecodeFailure& f,bool draining=false){
        for(;;){
            const int r=api.avcodec_receive_frame_p(stream.decoder,scratch);
            if(r==AVERROR_EOF){
                if(stream.index==video.index&&!pending_pictures.empty()){
                    f={MovieDecodeError::timing_association,0,"decoder EOF with unmatched native picture tags"};return false;
                }
                return true;
            }
            if(r==AVERROR(EAGAIN)){
                if(draining){f={MovieDecodeError::decode,r,"decoder requested input after drain"};return false;}
                return true;
            }
            if(r<0){f={MovieDecodeError::decode,r,"avcodec_receive_frame"};return false;}
            if((scratch->flags&AV_FRAME_FLAG_CORRUPT)||scratch->decode_error_flags){f={MovieDecodeError::decode,scratch->decode_error_flags,"corrupt decoded movie frame"};return false;}
            if(stream.index==video.index){
                if(scratch->format!=AV_PIX_FMT_YUV420P||scratch->width<=0||scratch->height<=0){f={MovieDecodeError::unsupported_frame,scratch->format,"native movie requires proven YUV420P frame"};return false;}
                MovieVideoFrame frame;frame.width=scratch->width;frame.height=scratch->height;
                if(!associate_picture(frame,f))return false;
                frame.source_pts=timestamp(scratch->pts,stream.base);frame.timestamp=timestamp(scratch->best_effort_timestamp,stream.base);
                frame.sample_aspect_numerator=scratch->sample_aspect_ratio.num;frame.sample_aspect_denominator=scratch->sample_aspect_ratio.den;
                frame.color_range=scratch->color_range;frame.color_space=scratch->colorspace;frame.color_primaries=scratch->color_primaries;frame.color_transfer=scratch->color_trc;
                const auto* source=format->streams[stream.index];
                frame.codec_timing.codec_framerate={stream.decoder->framerate.num,stream.decoder->framerate.den};
                frame.codec_timing.demux_avg_frame_rate={source->avg_frame_rate.num,source->avg_frame_rate.den};
                frame.codec_timing.demux_r_frame_rate={source->r_frame_rate.num,source->r_frame_rate.den};
                if(const auto* gop=api.av_frame_get_side_data_p(scratch,AV_FRAME_DATA_GOP_TIMECODE)){
                    std::uint64_t raw=0;
                    if(!gop->data||gop->size!=sizeof(raw)){f={MovieDecodeError::unsupported_frame,0,"invalid native codec GOP timecode extent"};return false;}
                    std::memcpy(&raw,gop->data,sizeof(raw));
                    if(raw>0x1ffffffu){f={MovieDecodeError::unsupported_frame,0,"invalid native codec 25-bit GOP timecode"};return false;}
                    frame.codec_timing.gop_timecode=raw;
                }
                for(unsigned plane=0;plane!=3;++plane){
                    const std::size_t w=plane?(std::size_t(frame.width)+1)/2:frame.width,h=plane?(std::size_t(frame.height)+1)/2:frame.height;
                    const auto stride=std::int64_t(scratch->linesize[plane]);
                    if(!scratch->data[plane]||std::uint64_t(stride<0?-stride:stride)<w||h>SIZE_MAX/w){f={MovieDecodeError::unsupported_frame,0,"invalid decoded video plane extent"};return false;}
                    frame.planes[plane].resize(w*h);
                    for(std::size_t y=0;y<h;++y)std::memcpy(frame.planes[plane].data()+y*w,scratch->data[plane]+std::ptrdiff_t(y)*stride,w);
                }
                ready.emplace_back(std::move(frame));
            }else{
                const auto channels=scratch->ch_layout.nb_channels;
                if(scratch->format!=AV_SAMPLE_FMT_S16P||scratch->sample_rate<=0||scratch->nb_samples<=0||channels<=0||!scratch->extended_data){f={MovieDecodeError::unsupported_frame,scratch->format,"native movie requires proven planar S16 PCM"};return false;}
                MovieAudioFrame frame;frame.sample_rate=scratch->sample_rate;frame.source_pts=timestamp(scratch->pts,stream.base);
                frame.timestamp=timestamp(scratch->best_effort_timestamp,stream.base);frame.channels.resize(channels);
                for(int channel=0;channel<channels;++channel){
                    if(!scratch->extended_data[channel]){f={MovieDecodeError::unsupported_frame,0,"missing decoded PCM plane"};return false;}
                    frame.channels[channel].resize(scratch->nb_samples);
                    std::memcpy(frame.channels[channel].data(),scratch->extended_data[channel],std::size_t(scratch->nb_samples)*sizeof(std::int16_t));
                }
                ready.emplace_back(std::move(frame));
            }
            api.av_frame_unref_p(scratch);
        }
    }
    bool send(Stream& stream,const AVPacket* data,MovieDecodeFailure& f,std::optional<std::uint64_t> packet_es_begin={}){
        if(stream.index!=video.index){
            const int sent=api.avcodec_send_packet_p(stream.decoder,data);
            if(sent<0){f={MovieDecodeError::decode,sent,"avcodec_send_packet"};return false;}
            return receive(stream,f);
        }
        if(!data||data->size<0||(data->size&&!data->data)){
            f={MovieDecodeError::timing_parse,0,"invalid elementary video packet extent"};return false;
        }
        std::optional<Mpeg2PictureTiming> picture;Mpeg2TimingFailure why;
        if(!timing.parse({data->data,static_cast<std::size_t>(data->size)},picture,why))return timing_failure(why,f);
        // All original packet storage/properties remain borrowed and untouched.
        // send_packet internally refs them before returning. Only this header's
        // opaque fields refer to our separately owned native tag.
        AVPacket annotated=*data;annotated.opaque=nullptr;annotated.opaque_ref=nullptr;
        const auto unref=[&](AVBufferRef* ref){api.av_buffer_unref_p(&ref);};
        std::unique_ptr<AVBufferRef,decltype(unref)> owned_ref(nullptr,unref);
        const std::uint8_t* registered=nullptr;
        if(picture){
            if(picture_serial==std::numeric_limits<std::uint64_t>::max()){
                f={MovieDecodeError::timing_association,0,"native picture serial exhausted"};return false;
            }
            if(!packet_es_begin||picture->packet_picture_offset>static_cast<std::uint64_t>(data->size)||
               static_cast<std::uint64_t>(data->size)-picture->packet_picture_offset<4||
               picture->packet_picture_offset>UINT64_MAX-*packet_es_begin){
                f={MovieDecodeError::source_association,0,"invalid native picture prefix source offset"};return false;
            }
            const auto picture_es_byte=*packet_es_begin+picture->packet_picture_offset;
            auto provenance=source_index->lookup_picture_prefix(source_stream_id,picture_es_byte,source_error);
            if(!provenance)return source_failure(MovieDecodeError::source_association,"native picture prefix source range",f);
            auto tag=std::make_unique<PictureTag>();tag->owner=this;tag->serial=++picture_serial;tag->picture=*picture;
            tag->source={*packet_es_begin,picture_es_byte,*provenance,provenance->quarter_pts()};
            auto* bytes=reinterpret_cast<std::uint8_t*>(tag.get());
            auto* ref=api.av_buffer_create_p(bytes,sizeof(PictureTag),free_picture_tag,nullptr,AV_BUFFER_FLAG_READONLY);
            // Public buffer.h: failure leaves data untouched. unique_ptr frees
            // failure; successful AVBuffer takes sole deletion responsibility.
            if(!ref){f={MovieDecodeError::allocation,0,"native picture AVBuffer ownership"};return false;}
            auto* raw=tag.release();owned_ref.reset(ref);
            const auto inserted=pending_pictures.emplace(bytes,PendingPicture{raw->serial,raw->picture,raw->source});
            if(!inserted.second){f={MovieDecodeError::timing_association,0,"native picture tag address reused while pending"};return false;}
            registered=bytes;annotated.opaque=raw;annotated.opaque_ref=ref;
        }
        const int sent=api.avcodec_send_packet_p(stream.decoder,&annotated);
        if(sent<0){
            if(registered)pending_pictures.erase(registered);
            f={MovieDecodeError::decode,sent,"avcodec_send_packet annotated picture"};return false;
        }
        return receive(stream,f);
    }
    bool send_packet(Stream& stream,MovieDecodeFailure& f){
        std::optional<std::uint64_t> packet_es_begin;
        if(stream.index==video.index){
            if(packet->size<0||(packet->size&&!packet->data)){
                f={MovieDecodeError::source_validation,0,"invalid original video packet extent"};return false;
            }
            packet_es_begin=source_video->position();
            // Validate the FULL unsplit packet exactly once. Terminal control
            // splitting below must neither repeat nor omit these source bytes.
            if(!source_video->validate_next({packet->data,static_cast<std::size_t>(packet->size)},source_error))
                return source_failure(MovieDecodeError::source_validation,"original video packet does not match next indexed ES bytes",f);
        }
        // The pinned parser includes SEQ_END_CODE in the final picture packet,
        // but its strict decoder counts those32bits as excess slice bits.
        // The same decoder explicitly supports the standalone4-byte marker.
        // Split only this exact terminal MPEG2 control word: preserve all bytes
        // and ordering, keep picture timestamps on the picture, invent none
        // for the non-picture control packet. No error flags are relaxed.
        if(stream.index!=video.index||packet->size<=4||
           std::memcmp(packet->data+packet->size-4,"\0\0\1\267",4)!=0)return send(stream,packet,f,packet_es_begin);
        const auto release=[&](AVPacket* p){api.av_packet_free_p(&p);};
        std::unique_ptr<AVPacket,decltype(release)> part(api.av_packet_alloc_p(),release);
        if(!part){f={MovieDecodeError::allocation,0,"terminal sequence packet"};return false;}
        const int body_size=packet->size-4;
        for(unsigned piece=0;piece!=2;++piece){
            api.av_packet_unref_p(part.get());
            int r=api.av_new_packet_p(part.get(),piece?4:body_size);
            if(r>=0)r=api.av_packet_copy_props_p(part.get(),packet);
            if(r<0){f={MovieDecodeError::allocation,r,"terminal sequence packet ownership"};return false;}
            std::memcpy(part->data,packet->data+(piece?body_size:0),piece?4:body_size);
            if(piece){part->pts=part->dts=AV_NOPTS_VALUE;part->duration=0;}
            if(!send(stream,part.get(),f,packet_es_begin))return false;
        }
        std::fprintf(stderr,"[native-movie] preserved terminal MPEG2 sequence-end as separate control packet (picture_bytes=%d)\n",body_size);
        return true;
    }
};
NativeMovieDecoder::NativeMovieDecoder(std::unique_ptr<Impl> p):impl_(std::move(p)){}
NativeMovieDecoder::~NativeMovieDecoder()=default;
NativeMovieDecoder::NativeMovieDecoder(NativeMovieDecoder&&)noexcept=default;
NativeMovieDecoder& NativeMovieDecoder::operator=(NativeMovieDecoder&&)noexcept=default;
std::unique_ptr<NativeMovieDecoder> NativeMovieDecoder::open(const std::filesystem::path& libraries,const std::filesystem::path& path,MovieDecodeFailure& f){
    f={};try{
        std::error_code ec;const auto absolute=std::filesystem::absolute(path,ec);
        if(ec||path.empty()||absolute.filename().empty()){f={MovieDecodeError::source_index,ec.value(),"invalid exact movie source path"};return {};}
        const auto name=absolute.filename().u8string();const std::string relative(name.begin(),name.end());
        MovieAssetFailure why;auto store=NativeMovieAssetStore::create(absolute.parent_path(),{{"native-decoder-source",relative}},why);
        if(!store){f={MovieDecodeError::source_index,static_cast<int>(why.kind),"exact-path native movie catalog binding"};f.asset_failure=why;return {};}
        auto asset=store->open("native-decoder-source",why);
        if(!asset){f={MovieDecodeError::source_index,static_cast<int>(why.kind),"exact-path native movie opened asset"};f.asset_failure=why;return {};}
        return open(libraries,std::move(*asset),f);
    }catch(const std::bad_alloc&){f={MovieDecodeError::allocation,0,"native movie path ownership"};}
     catch(const std::length_error&){f={MovieDecodeError::allocation,0,"native movie path host capacity"};}
     catch(const std::filesystem::filesystem_error& e){f={MovieDecodeError::source_index,e.code().value(),"native movie exact path encoding"};}
    return {};
}
std::unique_ptr<NativeMovieDecoder> NativeMovieDecoder::open(const std::filesystem::path& libraries,NativeMovieAsset&& asset,MovieDecodeFailure& f){
    f={};if(!asset.valid()){f={MovieDecodeError::invalid_owner,0,"invalid or moved NativeMovieAsset"};f.asset_failure.kind=MovieAssetError::moved_asset;return {};}
    try{
        auto s=std::make_unique<Impl>();
        s->source_index=NativeMoviePesIndex::build(std::move(asset),s->source_error);
        if(!s->source_index){s->source_failure(MovieDecodeError::source_index,"native movie PES index",f);return {};}
        s->source_file=s->source_index->file_reader(s->source_error);
        if(!s->source_file){s->source_failure(MovieDecodeError::source_index,"native movie same-asset file cursor",f);return {};}
        if(!s->api.load(libraries,f))return {};
        return finish_open(std::move(s),f);
    }catch(const std::bad_alloc&){f={MovieDecodeError::allocation,0,"native movie asset ownership"};}
     catch(const std::length_error&){f={MovieDecodeError::allocation,0,"native movie asset host capacity"};}
    return {};
}
std::unique_ptr<NativeMovieDecoder> NativeMovieDecoder::finish_open(std::unique_ptr<Impl> s,MovieDecodeFailure& f){
    try{
        // AVIO starts with logical position zero. The consumed catalog owner
        // may have been inspected or left at EOF; reset the same open file
        // before installing callbacks so reads and absolute seeks agree.
        std::error_code ec;std::uint64_t position=0;
        if(!s->seek_position(0,content::SeekOrigin::begin,position,ec)||position!=0){
            f={MovieDecodeError::file_io,ec?ec.value():static_cast<int>(std::errc::io_error),"native movie reader rewind"};
            f.asset_failure=s->asset_error;f.source_failure=s->source_error;return {};
        }
        auto* buffer=static_cast<std::uint8_t*>(s->api.av_malloc_p(65536));if(!buffer){f={MovieDecodeError::allocation,0,"native AVIO cache"};return {};}
        s->io=s->api.avio_alloc_context_p(buffer,65536,0,s.get(),Impl::read,nullptr,Impl::seek);
        if(!s->io){s->api.av_free_p(buffer);f={MovieDecodeError::allocation,0,"avio_alloc_context"};return {};}
        s->format=s->api.avformat_alloc_context_p();if(!s->format){f={MovieDecodeError::allocation,0,"avformat_alloc_context"};return {};}
        s->format->pb=s->io;s->format->flags|=AVFMT_FLAG_CUSTOM_IO;
        int r=s->api.avformat_open_input_p(&s->format,nullptr,nullptr,nullptr);
        if(r<0){f={s->io_error?MovieDecodeError::file_io:MovieDecodeError::demux,r,"avformat_open_input owned reader"};f.asset_failure=s->asset_error;f.source_failure=s->source_error;return {};}
        r=s->api.avformat_find_stream_info_p(s->format,nullptr);
        if(r<0){f={s->io_error?MovieDecodeError::file_io:MovieDecodeError::demux,r,"avformat_find_stream_info"};f.asset_failure=s->asset_error;f.source_failure=s->source_error;return {};}
        for(unsigned i=0;i<s->format->nb_streams;++i){
            const auto* p=s->format->streams[i]->codecpar;
            if(p->codec_type==AVMEDIA_TYPE_VIDEO&&p->codec_id==AV_CODEC_ID_MPEG2VIDEO&&s->video.index<0){if(!s->open_stream(s->video,i,f))return {};}
            else if(p->codec_type==AVMEDIA_TYPE_AUDIO&&p->codec_id==AV_CODEC_ID_ADPCM_ADX&&s->audio.index<0){if(!s->open_stream(s->audio,i,f))return {};}
            else {f={MovieDecodeError::unsupported_stream,p->codec_id,"unverified movie stream or duplicate media type"};return {};}
        }
        if(s->video.index<0){f={MovieDecodeError::invalid_stream,0,"movie has no MPEG2 video"};return {};}
        s->packet=s->api.av_packet_alloc_p();s->scratch=s->api.av_frame_alloc_p();if(!s->packet||!s->scratch){f={MovieDecodeError::allocation,0,"native decoder packet/frame"};return {};}
        return std::unique_ptr<NativeMovieDecoder>(new NativeMovieDecoder(std::move(s)));
    }catch(const std::bad_alloc&){f={MovieDecodeError::allocation,0,"native movie ownership"};}
     catch(const std::length_error&){f={MovieDecodeError::allocation,0,"native movie host capacity"};}
    return {};
}
MovieReadResult NativeMovieDecoder::next(MovieDecodeFailure& f){
    f={};if(!impl_){f={MovieDecodeError::invalid_owner,0,"moved native movie decoder"};return {};}
    auto& s=*impl_;if(s.failed){f={MovieDecodeError::failed_owner,0,"native movie decoder previously failed"};return {};}
    try{
        while(s.ready.empty()&&!s.ended){
            const int r=s.api.av_read_frame_p(s.format,s.packet);
            if(r<0){
                if(r!=AVERROR_EOF){f={s.io_error?MovieDecodeError::file_io:MovieDecodeError::demux,r,"av_read_frame owned reader"};f.asset_failure=s.asset_error;f.source_failure=s.source_error;s.failed=true;return {};}
                if(s.io_error){f={MovieDecodeError::file_io,s.io_error.value(),"owned movie reader read/seek"};f.asset_failure=s.asset_error;f.source_failure=s.source_error;s.failed=true;return {};}
                if(!s.source_video||!s.source_video->finished()){
                    f={MovieDecodeError::source_validation,0,"demux EOF before complete indexed video ES coverage"};s.failed=true;return {};
                }
                for(auto* stream:{&s.video,&s.audio})if(stream->decoder){
                    const int sent=s.api.avcodec_send_packet_p(stream->decoder,nullptr);
                    if(sent<0&&sent!=AVERROR_EOF){f={MovieDecodeError::decode,sent,"native decoder flush"};s.failed=true;return {};}
                    if(!s.receive(*stream,f,true)){s.failed=true;return {};}
                }
                s.ended=true;break;
            }
            auto* stream=s.packet->stream_index==s.video.index?&s.video:s.packet->stream_index==s.audio.index?&s.audio:nullptr;
            if(s.packet->flags&AV_PKT_FLAG_CORRUPT){f={MovieDecodeError::demux,0,"corrupt movie packet"};s.api.av_packet_unref_p(s.packet);s.failed=true;return {};}
            if(!stream){f={MovieDecodeError::unsupported_stream,s.packet->stream_index,"unbound movie packet stream"};s.api.av_packet_unref_p(s.packet);s.failed=true;return {};}
            const bool sent=s.send_packet(*stream,f);s.api.av_packet_unref_p(s.packet);
            if(!sent){s.failed=true;return {};}
        }
        if(s.ready.empty())return {MovieReadStatus::end,{}};
        MovieReadResult result{MovieReadStatus::frame,std::move(s.ready.front())};s.ready.pop_front();return result;
    }catch(const std::bad_alloc&){f={MovieDecodeError::allocation,0,"decoded native frame ownership"};}
     catch(const std::length_error&){f={MovieDecodeError::allocation,0,"decoded frame host capacity"};}
    s.failed=true;return {};
}
} // namespace mka::video
