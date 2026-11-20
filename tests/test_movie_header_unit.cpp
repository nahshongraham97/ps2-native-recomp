#include "mka/movie_header_unit.hpp"
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <utility>
using namespace mka::video;
namespace {int allocation_after=-1;}
void* operator new(std::size_t n){if(allocation_after==0){allocation_after=-1;throw std::bad_alloc();}if(allocation_after>0)--allocation_after;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {std::free(p);}void operator delete(void* p,std::size_t) noexcept {std::free(p);}
namespace {
using Bytes=std::vector<std::uint8_t>;
void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
struct Written {
 Bytes bytes;unsigned bit=0;
 explicit Written(std::uint8_t code):bytes{0,0,1,code}{}
 void put(unsigned count,std::uint32_t value){for(unsigned i=0;i<count;++i){if(!bit)bytes.push_back(0);bytes.back()|=((value>>(count-1-i))&1u)<<(7-bit);bit=(bit+1)%8;}}
};
Bytes sequence(bool intra=true,bool nonintra=true){Written b(0xb3);b.put(12,640);b.put(12,448);b.put(4,3);b.put(4,4);b.put(18,12345);b.put(1,1);b.put(10,112);b.put(1,0);b.put(1,intra);if(intra)for(unsigned i=0;i<64;++i)b.put(8,i+1);b.put(1,nonintra);if(nonintra)for(unsigned i=0;i<64;++i)b.put(8,255-i);return b.bytes;}
Bytes sequence_ext(){Written b(0xb5);b.put(4,1);b.put(8,0x48);b.put(1,1);b.put(2,1);b.put(2,2);b.put(2,1);b.put(12,123);b.put(1,1);b.put(8,9);b.put(1,1);b.put(2,2);b.put(5,17);return b.bytes;}
Bytes display(bool color){Written b(0xb5);b.put(4,2);b.put(3,5);b.put(1,color);if(color){b.put(8,1);b.put(8,2);b.put(8,3);}b.put(14,640);b.put(1,1);b.put(14,448);return b.bytes;}
Bytes coding(bool composite=false,unsigned structure=3){Written b(0xb5);b.put(4,8);for(unsigned i=1;i<=4;++i)b.put(4,i);b.put(2,2);b.put(2,structure);for(auto v:{1u,0u,1u,0u,1u,0u,1u,1u,1u})b.put(1,v);b.put(1,composite);if(composite)b.put(20,0xabcde);return b.bytes;}
Bytes gop(){Written b(0xb8);b.put(25,(1u<<24)|(12u<<19)|(34u<<13)|(1u<<12)|(56u<<6)|17u);b.put(1,1);b.put(1,0);return b.bytes;}
Bytes picture(unsigned type,unsigned extra=0){Written b(0);b.put(10,777);b.put(3,type);b.put(16,0xabcd);if(type==2||type==3){b.put(1,1);b.put(3,3);}if(type==3){b.put(1,0);b.put(3,5);}for(unsigned i=0;i<extra;++i){b.put(1,1);b.put(8,0x80+i);}b.put(1,0);return b.bytes;}
MovieHeaderUnit parsed(const Bytes& bytes,std::size_t count){auto result=parse_movie_header_unit(bytes);check(result.status==MovieHeaderUnitStatus::parsed&&result.failure.kind==MovieHeaderUnitError::none&&result.unit&&result.unit->consumed_bytes==count&&result.unit->bytes==Bytes(bytes.begin(),bytes.begin()+count),"owned exact unit bytes/endpoint");return std::move(*result.unit);}
void partials(const Bytes& bytes){for(std::size_t i=0;i<bytes.size();++i){auto q=parse_movie_header_unit(std::span(bytes).first(i));check(q.status==MovieHeaderUnitStatus::need_more&&!q.unit,"every byte-truncated valid header is need-more without output");}}
void failure(const Bytes& b,MovieHeaderUnitStatus s,MovieHeaderUnitError e){auto r=parse_movie_header_unit(b);check(r.status==s&&r.failure.kind==e&&!r.unit,"explicit rejected outcome has no unit");}
}
int main(){try{
 for(auto [i,n]:{std::pair{false,false},std::pair{true,false},std::pair{false,true},std::pair{true,true}}){const auto b=sequence(i,n);const auto count=12+64*unsigned(i)+64*unsigned(n);check(b.size()==count,"independent sequence bit endpoint literal");partials(b);auto unit=parsed(b,count);const auto& x=std::get<MovieSequenceHeader>(unit.payload);check(x.width==640&&x.height==448&&x.aspect_ratio_information==3&&x.frame_rate_code==4&&x.bit_rate_value==12345&&x.vbv_buffer_size==112&&!x.constrained_parameters,"sequence serialized fields");check(bool(x.intra_matrix)==i&&bool(x.non_intra_matrix)==n,"matrix presence is actual optional absence");if(i)check((*x.intra_matrix)[0]==1&&(*x.intra_matrix)[63]==64,"intra matrix serialized ordering");if(n)check((*x.non_intra_matrix)[0]==255&&(*x.non_intra_matrix)[63]==192,"nonintra matrix serialized ordering");}
 {const auto b=sequence_ext();check(b.size()==10,"sequence extension literal10");partials(b);auto u=parsed(b,10);const auto& x=std::get<MovieSequenceExtension>(u.payload);check(x.profile_and_level==0x48&&x.progressive_sequence&&x.chroma_format==1&&x.horizontal_size_extension==2&&x.vertical_size_extension==1&&x.bit_rate_extension==123&&x.vbv_buffer_size_extension==9&&x.low_delay&&x.frame_rate_extension_n==2&&x.frame_rate_extension_d==17,"sequence extension raw fields");}
 for(bool color:{false,true}){auto b=display(color);const auto count=color?12u:9u;check(b.size()==count,"display extension literal endpoint");partials(b);auto u=parsed(b,count);const auto& x=std::get<MovieDisplayExtension>(u.payload);check(x.video_format==5&&x.horizontal_size==640&&x.vertical_size==448&&bool(x.color)==color,"display extension fields/absence");if(color)check(x.color->primaries==1&&x.color->transfer==2&&x.color->matrix==3,"color data only");}
 for(bool composite:{false,true}){auto b=coding(composite);const auto count=composite?11u:9u;check(b.size()==count,"coding extension bit endpoint literal");partials(b);auto u=parsed(b,count);const auto& x=std::get<MoviePictureCodingExtension>(u.payload);check(x.f_code==std::array<std::uint8_t,4>{1,2,3,4}&&x.intra_dc_precision==2&&x.picture_structure==3&&x.top_field_first&&!x.frame_pred_frame_dct&&x.concealment_motion_vectors&&!x.q_scale_type&&x.intra_vlc_format&&!x.alternate_scan&&x.repeat_first_field&&x.chroma_420_type&&x.progressive_frame,"coding extension flags source bit order");check(bool(x.composite_display)==composite&&(!composite||(x.composite_display->v_axis==bool(0xabcde>>19)&&x.composite_display->field_sequence==((0xabcde>>16)&7)&&x.composite_display->sub_carrier==bool((0xabcde>>15)&1)&&x.composite_display->burst_amplitude==((0xabcde>>8)&127)&&x.composite_display->sub_carrier_phase==(0xabcde&255))),"optional composite1/3/1/7/8 fields");}
 // Field structure is serialized data here, not paired-field decode support.
 for(unsigned structure:{1u,2u}){auto u=parsed(coding(false,structure),9);check(std::get<MoviePictureCodingExtension>(u.payload).picture_structure==structure,"retain field structure without claiming output policy");}
 {auto b=gop();check(b.size()==8,"GOP endpoint literal8");partials(b);auto u=parsed(b,8);const auto& x=std::get<MovieGopHeader>(u.payload);check(x.raw_time_code==((1u<<24)|(12u<<19)|(34u<<13)|(1u<<12)|(56u<<6)|17u)&&x.drop_frame&&x.hours==12&&x.minutes==34&&x.seconds==56&&x.pictures==17&&x.closed_gop&&!x.broken_link,"GOP raw data/fields");}
 for(unsigned type:{1u,2u,3u})for(unsigned extra:{0u,2u}){auto b=picture(type,extra);const auto count=(32+10+3+16+(type>1?4:0)+(type==3?4:0)+extra*9+1+7)/8;check(b.size()==count,"independent picture bit endpoint");partials(b);auto u=parsed(b,count);const auto& x=std::get<MoviePictureHeader>(u.payload);check(x.temporal_reference==777&&x.coding_type==type&&x.vbv_delay==0xabcd&&bool(x.forward)==(type>1)&&bool(x.backward)==(type==3)&&x.extra_information.size()==extra,"picture optional fields and extras");if(x.forward)check(x.forward->full_pel&&x.forward->f_code==3,"forward bits");if(x.backward)check(!x.backward->full_pel&&x.backward->f_code==5,"backward bits");if(extra)check(x.extra_information==Bytes({0x80,0x81}),"extra bytes preserve encoded values");}
 {Bytes b{0,0,1,0xb7};partials(b);b.insert(b.end(),{9,8,7,6});auto u=parsed(b,4);check(std::holds_alternative<MovieSequenceEndHeader>(u.payload),"B7 only4 and no EOF inference");}
 {Bytes b{0,0,1,0xb2,0xff,0,0,2,0x80,0,0,1,0xb3};for(std::size_t n=4;n<b.size();++n)failure(Bytes(b.begin(),b.begin()+n),MovieHeaderUnitStatus::need_more,MovieHeaderUnitError::unterminated_userdata);auto u=parsed(b,9);check(std::get<MovieUserDataHeader>(u.payload).payload==Bytes({0xff,0,0,2,0x80}),"opaque binary userdata excludes next complete prefix");auto snapshot=u.bytes;b.clear();check(u.bytes==snapshot,"borrowed source release cannot change owned raw bytes");}
 {Bytes b(1048581,0x55);b[0]=0;b[1]=0;b[2]=1;b[3]=0xb2;b.insert(b.end(),{0,0,1,0});auto u=parsed(b,1048581);check(std::get<MovieUserDataHeader>(u.payload).payload.size()==1048577,"userdata exceeds source0xFFFFF limit without a native cap");}
 failure({0,2},MovieHeaderUnitStatus::malformed,MovieHeaderUnitError::invalid_prefix);
 failure({0,0,1,0xb5,0x30},MovieHeaderUnitStatus::unsupported,MovieHeaderUnitError::unsupported_extension);
 failure({0,0,1,1},MovieHeaderUnitStatus::unsupported,MovieHeaderUnitError::unsupported_start_code);
 {auto b=sequence(false,false);b[10]&=~0x20u;failure(b,MovieHeaderUnitStatus::malformed,MovieHeaderUnitError::invalid_marker);}
 {auto b=sequence_ext();b[7]&=~1u;failure(b,MovieHeaderUnitStatus::malformed,MovieHeaderUnitError::invalid_marker);}
 {auto b=gop();b[5]&=~8u;failure(b,MovieHeaderUnitStatus::malformed,MovieHeaderUnitError::invalid_marker);}
 {auto b=display(true);b[9]&=~2u;failure(b,MovieHeaderUnitStatus::malformed,MovieHeaderUnitError::invalid_marker);}
 failure(picture(4),MovieHeaderUnitStatus::unsupported,MovieHeaderUnitError::invalid_value);
 failure(coding(false,0),MovieHeaderUnitStatus::malformed,MovieHeaderUnitError::invalid_value);
 {auto b=coding();b[4]&=0xf0u;failure(b,MovieHeaderUnitStatus::malformed,MovieHeaderUnitError::invalid_value);}
 {auto b=sequence_ext();const auto before=b;allocation_after=0;auto q=parse_movie_header_unit(b);allocation_after=-1;check(q.status==MovieHeaderUnitStatus::error&&q.failure.kind==MovieHeaderUnitError::allocation&&!q.unit&&b==before,"genuine raw-result allocation failure transactional");check(parse_movie_header_unit(b).unit.has_value(),"same borrowed bytes retry after allocation failure");}
 {const auto b=picture(1,2);allocation_after=0;auto q=parse_movie_header_unit(b);allocation_after=-1;check(q.failure.kind==MovieHeaderUnitError::allocation&&!q.unit,"genuine extra vector allocation failure");}
 {const Bytes b{0,0,1,0xb2,1,2,0,0,1,0};allocation_after=1;auto q=parse_movie_header_unit(b);allocation_after=-1;check(q.failure.kind==MovieHeaderUnitError::allocation&&!q.unit,"payload allocation then raw allocation cannot publish partial owned unit");}
 {std::optional<MovieHeaderUnit> held;{auto input=picture(3,2);held=parse_movie_header_unit(input).unit;}auto moved=std::move(held);check(moved&&std::get<MoviePictureHeader>(moved->payload).extra_information==Bytes({0x80,0x81}),"owned variable fields survive borrowed input and result move");}
 std::puts("PASS native header unit checked fields/endpoints/ownership; no batch or admission binding");return 0;
}catch(const std::exception& e){allocation_after=-1;std::fprintf(stderr,"[movie-header-unit] %s\n",e.what());return 1;}}
