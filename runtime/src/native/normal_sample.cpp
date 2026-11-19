#include "mka/normal_sample.hpp"
#include <algorithm>
#include <new>
#include <stdexcept>
namespace mka::sound {
std::optional<NormalCount2Encoded> convert_normal_count2(
    const SamplePayload& payload, NormalCount2Head& head, NormalSampleFailure& failure) {
    failure={};
    auto fail=[&](NormalSampleError e)->std::optional<NormalCount2Encoded>{failure.kind=e;return std::nullopt;};
    if(payload.plane_count()!=2) return fail(NormalSampleError::unsupported_count);
    const auto header=payload.header();
    const std::uint32_t window_word=std::uint32_t(header[4])|(std::uint32_t(header[5])<<8)|
        (std::uint32_t(header[6])<<16)|(std::uint32_t(header[7])<<24);
    // Other source windows do not yet have a proved effective byte contract.
    if(window_word!=65536) return fail(NormalSampleError::unsupported_window_encoding);
    const std::size_t window=window_word/4u;
    const std::size_t head_distance=0x4000; // Proven source variant, not allocation capacity.
    const std::uint64_t length=payload.bytes_per_plane();
    if(!length) return fail(NormalSampleError::unsupported_zero_plane_size);
    if(length%16) return fail(NormalSampleError::misaligned_plane_size);
    const auto raw=payload.encoded_payload();
    if(length*2!=raw.size || !raw.data) return fail(NormalSampleError::inconsistent_payload);
    NormalCount2Head next=head;
    try {
        NormalCount2Encoded result;
        result.original_header=header;
        if(length>result.planes[0].max_size() || length>result.planes[1].max_size()) return fail(NormalSampleError::host_capacity);
        for(auto& plane:result.planes) plane.resize(static_cast<std::size_t>(length));
        for(std::size_t pos=0;pos<length;) {
            const std::size_t count=std::min<std::size_t>(window,static_cast<std::size_t>(length)-pos);
            const auto* input=raw.data+2*pos;
            if(2*count>=head_distance+next.bytes.size()) {
                std::copy_n(input+head_distance,next.bytes.size(),next.bytes.begin());
                next.known=true;
            }
            if(!next.known) return fail(NormalSampleError::unknown_initial_head);
            // 2D5FC: even blocks stay in plane0; reverse-copy destinations for
            // odd blocks are B+16..B+count. Only [B,B+count) was transferred.
            std::copy(next.bytes.begin(),next.bytes.end(),result.planes[1].begin()+pos);
            for(std::size_t block=0;block<count/16;++block) {
                std::copy_n(input+32*block,16,result.planes[0].begin()+pos+16*block);
                if(block+1<count/16)
                    std::copy_n(input+32*block+16,16,result.planes[1].begin()+pos+16*(block+1));
            }
            pos+=count;
        }
        head=next;
        return result;
    } catch(const std::bad_alloc&){return fail(NormalSampleError::allocation_failed);}
      catch(const std::length_error&){return fail(NormalSampleError::host_capacity);}
}
std::optional<NormalCount2Decoded> decode_normal_count2(
    const SamplePayload& payload,NormalCount2Head& head,std::array<AdpcmHistory,2>& histories,
    NormalSampleFailure& failure) {
    failure={};auto next_head=head;auto next_histories=histories;
    auto encoded=convert_normal_count2(payload,next_head,failure);
    if(!encoded)return std::nullopt;
    std::array<AdpcmMono,2> pcm;
    for(std::size_t plane=0;plane<2;++plane) {
        const auto& bytes=encoded->planes[plane];
        auto result=decode_adpcm_mono(bytes.data(),bytes.size(),next_histories[plane],failure.codec);
        if(!result){failure.kind=NormalSampleError::codec_failed;return std::nullopt;}
        pcm[plane]=std::move(*result);
    }
    NormalCount2Decoded result{std::move(*encoded),std::move(pcm)};
    head=next_head;histories=next_histories;
    return result;
}
} // namespace mka::sound
