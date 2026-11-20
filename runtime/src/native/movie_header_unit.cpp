#include "mka/movie_header_unit.hpp"
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>
#include <type_traits>
namespace mka::video {
namespace {
struct Bits {
    std::span<const std::uint8_t> data;std::size_t byte=0;unsigned bit=0;
    bool get(unsigned count,std::uint32_t& value) noexcept {
        value=0;for(unsigned i=0;i<count;++i){if(byte==data.size())return false;
            value=(value<<1)|((data[byte]>>(7-bit))&1u);if(++bit==8){bit=0;++byte;}}
        return true;
    }
    std::size_t consumed() const noexcept {return byte+(bit!=0);}
};
MovieHeaderUnitResult reject(MovieHeaderUnitStatus s,MovieHeaderUnitError e,std::uint64_t at=0) noexcept {
    return {s,{e,at},{}};
}
}
MovieHeaderUnitResult parse_movie_header_unit(std::span<const std::uint8_t> input) noexcept {
    using S=MovieHeaderUnitStatus;using E=MovieHeaderUnitError;
    if(input.size()>std::numeric_limits<std::uint64_t>::max())return reject(S::error,E::host_capacity);
    // Reject already-known bad prefix bytes, but do not read incomplete bytes.
    const std::uint8_t prefix[3]{0,0,1};
    for(std::size_t i=0;i<input.size()&&i<3;++i)if(input[i]!=prefix[i])return reject(S::malformed,E::invalid_prefix,i);
    if(input.size()<4)return reject(S::need_more,E::incomplete_prefix,input.size());
    const auto code=input[3];Bits b{input.subspan(4)};std::uint32_t v=0;
    const auto more=[&](){return reject(S::need_more,E::incomplete_header,4+b.byte);};
    const auto malformed=[&](E e){return reject(S::malformed,e,4+b.byte);};
    const auto read=[&](unsigned n,auto& out){if(!b.get(n,v))return false;out=static_cast<std::remove_reference_t<decltype(out)>>(v);return true;};
    try {
        MovieHeaderUnit unit;
        if(code==0xb3){
            MovieSequenceHeader x;
            if(!read(12,x.width)||!read(12,x.height)||!read(4,x.aspect_ratio_information)||!read(4,x.frame_rate_code)||!read(18,x.bit_rate_value)||!b.get(1,v))return more();
            if(v!=1)return malformed(E::invalid_marker);
            if(!read(10,x.vbv_buffer_size)||!read(1,x.constrained_parameters))return more();
            if(!x.width||!x.height||!x.aspect_ratio_information||x.aspect_ratio_information>4||!x.frame_rate_code||x.frame_rate_code>8)return malformed(E::invalid_value);
            if(!b.get(1,v))return more();
            if(v){x.intra_matrix.emplace();for(auto& e:*x.intra_matrix)if(!read(8,e))return more();}
            if(!b.get(1,v))return more();
            if(v){x.non_intra_matrix.emplace();for(auto& e:*x.non_intra_matrix)if(!read(8,e))return more();}
            unit.payload=std::move(x);
        }else if(code==0xb5){
            if(!b.get(4,v))return more();const auto id=v;
            if(id==1){
                MovieSequenceExtension x;
                if(!read(8,x.profile_and_level)||!read(1,x.progressive_sequence)||!read(2,x.chroma_format)||!read(2,x.horizontal_size_extension)||!read(2,x.vertical_size_extension)||!read(12,x.bit_rate_extension)||!b.get(1,v))return more();
                if(v!=1)return malformed(E::invalid_marker);
                if(!read(8,x.vbv_buffer_size_extension)||!read(1,x.low_delay)||!read(2,x.frame_rate_extension_n)||!read(5,x.frame_rate_extension_d))return more();
                if(!x.chroma_format)return malformed(E::invalid_value);
                unit.payload=x;
            }else if(id==2){
                MovieDisplayExtension x;if(!read(3,x.video_format)||!b.get(1,v))return more();
                if(v){MovieDisplayColor color;if(!read(8,color.primaries)||!read(8,color.transfer)||!read(8,color.matrix))return more();x.color=color;}
                if(!read(14,x.horizontal_size)||!b.get(1,v))return more();if(v!=1)return malformed(E::invalid_marker);
                if(!read(14,x.vertical_size))return more();unit.payload=x;
            }else if(id==3){
                MovieQuantMatrixExtension x;
                for(auto* matrix:{&x.intra_matrix,&x.non_intra_matrix,&x.chroma_intra_matrix,&x.chroma_non_intra_matrix}){
                    if(!b.get(1,v))return more();
                    if(v){matrix->emplace();for(auto& coefficient:**matrix)if(!read(8,coefficient))return more();}
                }
                unit.payload=std::move(x);
            }else if(id==8){
                MoviePictureCodingExtension x;for(auto& e:x.f_code){if(!read(4,e))return more();if(!e)return malformed(E::invalid_value);}
                if(!read(2,x.intra_dc_precision)||!read(2,x.picture_structure))return more();
                if(!x.picture_structure)return malformed(E::invalid_value);
                if(!read(1,x.top_field_first)||!read(1,x.frame_pred_frame_dct)||!read(1,x.concealment_motion_vectors)||!read(1,x.q_scale_type)||!read(1,x.intra_vlc_format)||!read(1,x.alternate_scan)||!read(1,x.repeat_first_field)||!read(1,x.chroma_420_type)||!read(1,x.progressive_frame)||!b.get(1,v))return more();
                if(v){MovieCompositeDisplay y;if(!read(1,y.v_axis)||!read(3,y.field_sequence)||!read(1,y.sub_carrier)||!read(7,y.burst_amplitude)||!read(8,y.sub_carrier_phase))return more();x.composite_display=y;}unit.payload=x;
            }else return reject(S::unsupported,E::unsupported_extension,4);
        }else if(code==0xb8){
            MovieGopHeader x;if(!read(25,x.raw_time_code)||!read(1,x.closed_gop)||!read(1,x.broken_link))return more();
            if(!(x.raw_time_code&(1u<<12)))return malformed(E::invalid_marker);
            x.drop_frame=(x.raw_time_code>>24)!=0;x.hours=(x.raw_time_code>>19)&31;x.minutes=(x.raw_time_code>>13)&63;x.seconds=(x.raw_time_code>>6)&63;x.pictures=x.raw_time_code&63;
            unit.payload=x;
        }else if(code==0){
            MoviePictureHeader x;if(!read(10,x.temporal_reference)||!read(3,x.coding_type)||!read(16,x.vbv_delay))return more();
            if(x.coding_type<1||x.coding_type>3)return reject(S::unsupported,E::invalid_value,4+b.byte);
            if(x.coding_type==2||x.coding_type==3){MoviePictureDirection y;if(!read(1,y.full_pel)||!read(3,y.f_code))return more();if(!y.f_code)return malformed(E::invalid_value);x.forward=y;}
            if(x.coding_type==3){MoviePictureDirection y;if(!read(1,y.full_pel)||!read(3,y.f_code))return more();if(!y.f_code)return malformed(E::invalid_value);x.backward=y;}
            for(;;){if(!b.get(1,v))return more();if(!v)break;if(!b.get(8,v))return more();x.extra_information.push_back(static_cast<std::uint8_t>(v));}
            unit.payload=std::move(x);
        }else if(code==0xb2){
            std::size_t end=4;for(;;){
                if(input.size()-end<4)return reject(S::need_more,E::unterminated_userdata,end);
                if(input[end]==0&&input[end+1]==0&&input[end+2]==1)break;++end;
            }
            MovieUserDataHeader x;x.payload.assign(input.begin()+4,input.begin()+end);unit.payload=std::move(x);unit.consumed_bytes=end;
        }else if(code==0xb7){unit.payload=MovieSequenceEndHeader{};}
        else return reject(S::unsupported,E::unsupported_start_code,3);
        if(!unit.consumed_bytes)unit.consumed_bytes=4+b.consumed();
        unit.bytes.assign(input.begin(),input.begin()+static_cast<std::size_t>(unit.consumed_bytes));
        return {S::parsed,{},{std::move(unit)}};
    }catch(const std::bad_alloc&){return reject(S::error,E::allocation);}
     catch(const std::length_error&){return reject(S::error,E::host_capacity);}
}
} // namespace mka::video
