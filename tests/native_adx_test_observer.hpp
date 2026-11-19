#pragma once
#include "mka/movie_decode.hpp"
#include "data/movie_native_adx_source_oracle.hpp"
#include "native_test_sha256.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>

// CPU/file test observer only. Canonical sample serialization is independent
// of output batching, host endian and old FFmpeg AVFrame timestamps.
class NativeAdxTestObserver {
public:
    void add(const mka::video::MovieAudioFrame& frame) {
        using namespace mka::video;
        require(!finished_,"native ADX observer already finalized");
        require(frame.sample_rate==48000&&frame.channels.size()==2&&
            !frame.channels[0].empty()&&frame.channels[0].size()==frame.channels[1].size(),
            "native ADX source rate/plane extents");
        require(frame.channel_layout.order==MovieAudioChannelOrder::unspecified&&
            frame.channel_layout.channel_count==2&&!frame.channel_layout.mask&&
            frame.channel_layout.channels.empty(),"native source plane ordinals, no guessed speakers");
        require(!frame.source_pts.present&&!frame.timestamp.present,
            "native ADX has no invented decoded AVFrame timestamps");
        require(frame.source.has_value(),"native ADX retains typed source provenance");
        const auto& source=*frame.source;
        require(source.first_source_frame==published_&&source.raw_decoded_frames>0&&
            source.raw_decoded_frames%32==0&&source.raw_decoded_frames>=frame.channels[0].size()&&
            source.es_begin<source.es_end&&!source.packets.empty()&&!source.pes_spans.empty(),
            "native ADX contiguous source index/raw count/encoded ownership");
        require(raw_<=std::numeric_limits<std::uint64_t>::max()-source.raw_decoded_frames&&
            published_<=std::numeric_limits<std::uint64_t>::max()-frame.channels[0].size(),
            "native ADX observer count overflow");
        std::array<std::uint8_t,4096> bytes{};
        for(std::size_t first=0;first<frame.channels[0].size();){
            const auto count=std::min(bytes.size()/4,frame.channels[0].size()-first);
            for(std::size_t i=0;i<count;++i)for(unsigned p=0;p<2;++p){
                const auto value=static_cast<std::uint16_t>(frame.channels[p][first+i]);
                bytes[4*i+2*p]=static_cast<std::uint8_t>(value);
                bytes[4*i+2*p+1]=static_cast<std::uint8_t>(value>>8);
            }
            hash_.add(bytes.data(),4*count);first+=count;
        }
        published_+=frame.channels[0].size();raw_+=source.raw_decoded_frames;++frames_;
    }
    void verify(std::string_view key) {
        require(!finished_,"native ADX observer already finalized");
        const native_movie_adx_source_oracle::Asset* expected=nullptr;
        for(const auto& candidate:native_movie_adx_source_oracle::assets)
            if(key==candidate.name)expected=&candidate;
        require(expected!=nullptr,"no source-certified native ADX oracle for key");
        require(frames_>0&&published_==expected->published_frames&&raw_==expected->raw_decoded_frames,
            "exact declared N and full-history source counts");
        require(hash_.finish()==expected->pcm_interleaved_s16le_sha256,
            "every native PCM sample matches original-byte integer model");
        finished_=true;
    }
private:
    static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
    NativeTestSha256 hash_;
    std::uint64_t published_=0,raw_=0,frames_=0;
    bool finished_=false;
};
