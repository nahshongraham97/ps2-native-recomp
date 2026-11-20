#pragma once
#include "mka/movie_source_syntax_index.hpp"
#include "native_test_sha256.hpp"
#include <cstdio>
#include <stdexcept>
#include <string>
#include <type_traits>
namespace native_syntax_test {
using namespace mka::video;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
std::string digest(std::span<const std::uint8_t> bytes){NativeTestSha256 h;h.add(bytes.data(),bytes.size());return h.finish();}
std::string json_string(std::string_view v){std::string out="\"";for(unsigned char c:v){if(c=='"'||c=='\\'){out+='\\';out+=char(c);}else if(c<32){char b[7];std::snprintf(b,sizeof b,"\\u%04x",unsigned(c));out+=b;}else out+=char(c);}out+='"';return out;}
struct Array {
 std::string s="[";bool first=true;
 void raw(std::string_view v){if(!first)s+=',';first=false;s+=v;}
 void number(std::uint64_t n){raw(std::to_string(n));}
 void flag(bool b){number(b?1:0);}
 void hash(std::span<const std::uint8_t> b){raw(json_string(digest(b)));}
 std::string finish(){return s+"]";}
};
struct Fields {std::string kind,wire;};
Fields fields(const MovieHeaderUnitPayload& payload){return std::visit([](const auto& p)->Fields{
 using T=std::decay_t<decltype(p)>;Array a;std::string kind;
 if constexpr(std::is_same_v<T,MovieSequenceHeader>){kind="sequence";
  a.number(p.width);a.number(p.height);a.number(p.aspect_ratio_information);a.number(p.frame_rate_code);a.number(p.bit_rate_value);a.number(p.vbv_buffer_size);a.flag(p.constrained_parameters);
  if(p.intra_matrix)a.hash(*p.intra_matrix);else a.raw("null");if(p.non_intra_matrix)a.hash(*p.non_intra_matrix);else a.raw("null");
 }else if constexpr(std::is_same_v<T,MovieSequenceExtension>){kind="sequence_extension";
  a.number(p.profile_and_level);a.flag(p.progressive_sequence);a.number(p.chroma_format);a.number(p.horizontal_size_extension);a.number(p.vertical_size_extension);a.number(p.bit_rate_extension);a.number(p.vbv_buffer_size_extension);a.flag(p.low_delay);a.number(p.frame_rate_extension_n);a.number(p.frame_rate_extension_d);
 }else if constexpr(std::is_same_v<T,MovieQuantMatrixExtension>){kind="quant_matrix_extension";
  for(const auto* matrix:{&p.intra_matrix,&p.non_intra_matrix,&p.chroma_intra_matrix,&p.chroma_non_intra_matrix}){if(*matrix)a.hash(**matrix);else a.raw("null");}
 }else if constexpr(std::is_same_v<T,MovieDisplayExtension>){kind="display_extension";
  a.number(p.video_format);if(p.color){Array c;c.number(p.color->primaries);c.number(p.color->transfer);c.number(p.color->matrix);a.raw(c.finish());}else a.raw("null");a.number(p.horizontal_size);a.number(p.vertical_size);
 }else if constexpr(std::is_same_v<T,MoviePictureCodingExtension>){kind="picture_extension";
  Array codes;for(auto c:p.f_code)codes.number(c);a.raw(codes.finish());a.number(p.intra_dc_precision);a.number(p.picture_structure);a.flag(p.top_field_first);a.flag(p.frame_pred_frame_dct);a.flag(p.concealment_motion_vectors);a.flag(p.q_scale_type);a.flag(p.intra_vlc_format);a.flag(p.alternate_scan);a.flag(p.repeat_first_field);a.flag(p.chroma_420_type);a.flag(p.progressive_frame);if(p.composite_display){const auto& c=*p.composite_display;a.number((std::uint32_t(c.v_axis)<<19)|(std::uint32_t(c.field_sequence)<<16)|(std::uint32_t(c.sub_carrier)<<15)|(std::uint32_t(c.burst_amplitude)<<8)|c.sub_carrier_phase);}else a.raw("null");
 }else if constexpr(std::is_same_v<T,MovieGopHeader>){kind="gop";
  a.number(p.raw_time_code);a.flag(p.drop_frame);a.number(p.hours);a.number(p.minutes);a.number(p.seconds);a.number(p.pictures);a.flag(p.closed_gop);a.flag(p.broken_link);
 }else if constexpr(std::is_same_v<T,MoviePictureHeader>){kind="picture";
  a.number(p.temporal_reference);a.number(p.coding_type);a.number(p.vbv_delay);
  const auto direction=[&](const std::optional<MoviePictureDirection>& d){if(!d){a.raw("null");return;}Array v;v.flag(d->full_pel);v.number(d->f_code);a.raw(v.finish());};direction(p.forward);direction(p.backward);a.number(p.extra_information.size());a.hash(p.extra_information);
 }else if constexpr(std::is_same_v<T,MovieUserDataHeader>){kind="userdata";a.number(p.payload.size());a.hash(p.payload);
 }else{static_assert(std::is_same_v<T,MovieSequenceEndHeader>);kind="sequence_end";}
 return {kind,a.finish()};},payload);}
std::string nullable(std::optional<std::uint64_t> p){return p?std::to_string(*p):"null";}
std::string source_record(const MovieSourceSyntaxPicture& p){
 Array a;a.number(p.picture_es_byte);a.number(p.sequence_es_byte);a.number(p.sequence_extension_es_byte);a.number(p.coding_extension_es_byte);
 Array pes;pes.number(p.pes.stream_id);pes.number(p.pes.packet_offset);pes.number(p.pes.payload_offset);pes.number(p.pes.packet_end);pes.number(p.pes.es_begin);pes.number(p.pes.es_end);pes.raw(nullable(p.pes.pts33));pes.raw(nullable(p.pes.dts33));a.raw(pes.finish());a.raw(nullable(p.quarter_pts));
 a.raw(fields(p.sequence).wire);a.raw(fields(p.sequence_extension).wire);a.raw(p.display_extension?fields(*p.display_extension).wire:"null");a.raw(nullable(p.gop_es_byte));a.raw(p.gop?fields(*p.gop).wire:"null");a.raw(fields(p.picture).wire);a.raw(fields(p.coding).wire);
 check(p.quant_matrix_units&&p.quant_history_begin<=p.quant_history_end&&p.quant_history_end<=p.quant_matrix_units->size(),"owned flat quant history bounds");Array quant;
 for(auto i=p.quant_history_begin;i<p.quant_history_end;++i){const auto& q=(*p.quant_matrix_units)[i];check(std::holds_alternative<MovieQuantMatrixExtension>(q.unit.payload),"typed quant history");Array row;row.number(q.es_byte);row.raw(fields(q.unit.payload).wire);row.hash(q.unit.bytes);quant.raw(row.finish());}a.raw(quant.finish());
 Array user;for(const auto& u:p.userdata){const auto* data=std::get_if<MovieUserDataHeader>(&u.unit.payload);check(data,"typed source userdata");Array row;row.number(u.es_byte);row.number(u.unit.consumed_bytes);row.hash(u.unit.bytes);row.number(data->payload.size());row.hash(data->payload);user.raw(row.finish());}a.raw(user.finish());return a.finish();
}
std::string string_digest(std::string_view s){return digest(std::span(reinterpret_cast<const std::uint8_t*>(s.data()),s.size()));}
} // namespace native_syntax_test
