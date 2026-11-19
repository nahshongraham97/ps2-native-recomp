#include "mka/mpeg2_timing.hpp"
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <optional>
#include <vector>
#include <utility>
using namespace mka::video;
namespace {
void check(bool v,const char* name){if(!v){std::fprintf(stderr,"[mpeg2-timing-fail] %s\n",name);std::exit(1);}}
struct Writer {
    std::vector<std::uint8_t> v;unsigned bit=0;
    void put(unsigned n,std::uint32_t x){for(unsigned i=n;i;--i){if(!bit)v.push_back(0);v.back()|=((x>>(i-1))&1u)<<(7-bit);bit=(bit+1)&7;}}
};
using Bytes=std::vector<std::uint8_t>;
void unit(Bytes& b,std::uint8_t code,const Writer& w){b.insert(b.end(),{0,0,1,code});b.insert(b.end(),w.v.begin(),w.v.end());}
Bytes sequence(unsigned rate=4,unsigned n=2,unsigned d=7,bool progressive=true){
    Bytes b;Writer s;s.put(12,720);s.put(12,480);s.put(4,3);s.put(4,rate);s.put(18,4000);s.put(1,1);s.put(10,112);s.put(1,0);s.put(1,0);s.put(1,0);unit(b,0xb3,s);
    Writer e;e.put(4,1);e.put(8,0x48);e.put(1,progressive);e.put(2,1);e.put(2,1);e.put(2,0);e.put(12,0);e.put(1,1);e.put(8,0);e.put(1,0);e.put(2,n);e.put(5,d);unit(b,0xb5,e);return b;
}
Bytes gop(unsigned hour,unsigned minute,unsigned second,unsigned frame,bool drop){
    Writer w;w.put(1,drop);w.put(5,hour);w.put(6,minute);w.put(1,1);w.put(6,second);w.put(6,frame);w.put(1,1);w.put(1,0);Bytes b;unit(b,0xb8,w);return b;
}
Bytes picture(unsigned ref=513,unsigned type=3,unsigned structure=3,bool top=true,bool repeat=true,bool progressive=true){
    Writer w;w.put(10,ref);w.put(3,type);w.put(16,0xffff);if(type==2||type==3){w.put(1,0);w.put(3,7);}if(type==3){w.put(1,0);w.put(3,6);}w.put(1,0);Bytes b;unit(b,0,w);
    Writer e;e.put(4,8);e.put(16,0xffff);e.put(2,2);e.put(2,structure);e.put(1,top);e.put(5,0x10);e.put(1,repeat);e.put(1,1);e.put(1,progressive);e.put(1,0);unit(b,0xb5,e);
    b.insert(b.end(),{0,0,1,1,0x55,0xaa});return b;
}
void append(Bytes& a,const Bytes& b){a.insert(a.end(),b.begin(),b.end());}
}
int main(){
    NativeMpeg2Timing parser;Mpeg2TimingFailure f;std::optional<Mpeg2PictureTiming> result;
    auto p=picture();check(!parser.parse(p,result,f)&&f.kind==Mpeg2TimingError::missing_sequence&&!result,"no guessed sequence");
    auto seq=sequence();check(parser.prime(seq,f),"explicit extradata prime");
    auto packet=gop(12,34,56,17,true);append(packet,p);check(parser.parse(packet,result,f)&&result.has_value(),"one frame picture");
    check(result->packet_picture_offset==8,"GOP prefix places picture FIRST byte at8");
    auto owned=*result;check(owned.sequence.frame_rate_code==4&&owned.sequence.extension_n==2&&owned.sequence.extension_d==7&&owned.sequence.width==4816&&owned.sequence.height==480,"sequence fields and extended width");
    check(owned.gop&&owned.gop->raw_time_code==((1u<<24)|(12u<<19)|(34u<<13)|(1u<<12)|(56u<<6)|17u)&&owned.gop->hours==12&&owned.gop->minutes==34&&owned.gop->seconds==56&&owned.gop->pictures==17&&owned.gop->drop_frame&&owned.gop->closed_gop&&!owned.gop->broken_link,"raw GOP bit positions");
    check(owned.temporal_reference==513&&owned.coding_type==3&&owned.picture_structure==3&&owned.top_field_first&&owned.repeat_first_field&&owned.progressive_frame,"picture fields");
    packet.clear();seq.clear();check(owned.gop->minutes==34,"owned metadata input lifetime");
    auto other=picture(7,1,3,false,false,false);check(parser.parse(other,result,f)&&result->gop&&result->gop->raw_time_code==owned.gop->raw_time_code&&!result->top_field_first&&!result->repeat_first_field&&!result->progressive_frame,"GOP inheritance different picture flags");
    // Literal unit sizes: sequence12 + extension10 + GOP8 + user-data7.
    NativeMpeg2Timing offsets;
    auto prefixed=sequence();append(prefixed,gop(1,2,3,4,false));
    append(prefixed,Bytes{0,0,1,0xb2,'a','b','c'});append(prefixed,p);
    check(offsets.parse(prefixed,result,f)&&result&&result->packet_picture_offset==37,
          "sequence/GOP/user-data exact prefix-firstbyte offset");
    auto padded=p; padded.insert(padded.begin(),0);
    check(offsets.parse(padded,result,f)&&result&&result->packet_picture_offset==1,
          "extra zero is padding before actual picture prefix");
    check(offsets.parse(p,result,f)&&result&&result->packet_picture_offset==0,
          "picture-only offset never inherits previous packet offset");
    auto malformed=sequence(8);append(malformed,Bytes{0,0,1,0,0});
    check(!offsets.parse(malformed,result,f)&&!result,
          "failed picture publishes no offset or mutated sequence");
    check(offsets.parse(p,result,f)&&result&&result->packet_picture_offset==0&&
          result->sequence.frame_rate_code==4&&result->gop&&result->gop->hours==1,
          "failure preserves previous sequence/GOP; offset stays input-local");
    check(offsets.parse(Bytes{0,0,1,0xb2,'x'},result,f)&&!result,
          "control-only input has no picture offset result");
    check(offsets.parse(Bytes{0,0,1,0xb7},result,f)&&!result,
          "sequence-end control has no picture offset result");
    auto many=picture();append(many,picture(1));check(!parser.parse(many,result,f)&&f.kind==Mpeg2TimingError::multiple_pictures&&!result,"multiple pictures fail");
    auto field=picture(0,1,1);check(!parser.parse(field,result,f)&&f.kind==Mpeg2TimingError::unsupported_field_picture,"field rejected");
    auto reserved=picture(0,1,0);check(!parser.parse(reserved,result,f)&&f.kind==Mpeg2TimingError::invalid_picture,"reserved structure");
    check(parser.parse(p,result,f)&&result->gop&&result->gop->minutes==34,"state retained after failures");
    auto bad=sequence(0);check(!parser.parse(bad,result,f)&&f.kind==Mpeg2TimingError::unsupported_rate,"reserved frame rate");
    bad=sequence(8,3,31,false);append(bad,Bytes{0,0,1,0xb5,0xf0});check(!parser.parse(bad,result,f)&&f.kind==Mpeg2TimingError::unsupported_extension,"unknown extension transaction");
    check(parser.parse(p,result,f)&&result->sequence.frame_rate_code==4&&result->gop,"failed sequence mutation rolled back");
    for(std::size_t cut=4;cut<12;++cut){auto short_seq=sequence();short_seq.resize(cut);NativeMpeg2Timing fresh;check(!fresh.prime(short_seq,f),"truncated sequence header");}
    check(!parser.parse(Bytes{0,0,1},result,f)&&f.kind==Mpeg2TimingError::truncated_header,"truncated start code");
    check(!parser.parse(Bytes{3,0,0,1,0xb7},result,f)&&f.kind==Mpeg2TimingError::invalid_start_code,"unframed bytes");
    check(!parser.parse(Bytes{0,0,1,0xb4},result,f)&&f.kind==Mpeg2TimingError::unsupported_start_code,"reserved start code");
    auto broken_gop=gop(1,2,3,4,false);broken_gop[5]&=0xf7;check(!parser.parse(broken_gop,result,f)&&f.kind==Mpeg2TimingError::invalid_marker,"GOP marker rejection");
    auto noext=picture();noext.resize(9);check(!parser.parse(noext,result,f),"truncated picture no extension");
    Bytes extras=sequence();append(extras,p);NativeMpeg2Timing fresh;check(!fresh.prime(extras,f)&&f.kind==Mpeg2TimingError::picture_in_extradata,"extradata picture rejection");
    check(!fresh.parse(p,result,f)&&f.kind==Mpeg2TimingError::missing_sequence,"failed priming transactional");
    auto s2=sequence(8,3,31,false);check(parser.prime(s2,f)&&parser.parse(p,result,f)&&result->sequence.frame_rate_code==8&&result->sequence.extension_n==3&&result->sequence.extension_d==31&&!result->sequence.progressive_sequence&&!result->gop,"new sequence clears old GOP");
    auto moved=std::move(parser);check(moved.parse(p,result,f)&&result->sequence.frame_rate_code==8,"parser move value state");
    check(moved.parse(Bytes{0,0,1,0xb7},result,f)&&!result,"sequence end control no picture");
    check(!moved.parse(p,result,f)&&f.kind==Mpeg2TimingError::missing_sequence,"end clears sequence inheritance");
    check(owned.temporal_reference==513&&owned.gop->minutes==34,"output survives parser reset and moves");
    NativeMpeg2Timing terminal;check(terminal.prime(sequence(),f),"terminal fixture prime");
    auto after_end=picture();append(after_end,Bytes{0,0,1,0xb7,0,0,1,1,0x55});
    check(!terminal.parse(after_end,result,f)&&f.kind==Mpeg2TimingError::invalid_sequence&&!result,"no later slice may attach after terminal sequence-end");
    auto terminal_ok=picture();append(terminal_ok,Bytes{0,0,1,0xb7,0,0});
    check(terminal.parse(terminal_ok,result,f)&&result,"terminal source end with zero padding retains only its prior picture");
    NativeMpeg2Timing reprime;check(reprime.prime(sequence(),f),"reprime fixture initialized");
    check(!reprime.prime({},f)&&f.kind==Mpeg2TimingError::missing_sequence,"empty extradata must not reuse a prior sequence");
    check(!reprime.prime(Bytes{0,0,1,0xb2,'u'},f)&&f.kind==Mpeg2TimingError::missing_sequence,"opaque-only extradata must not reuse a prior sequence");
    check(reprime.parse(p,result,f)&&result->sequence.frame_rate_code==4,"failed reprime preserves prior valid state transactionally");
    check(!reprime.parse(Bytes{0,0,1,0xb7,0x55},result,f)&&f.kind==Mpeg2TimingError::invalid_sequence&&!result,"terminal sequence-end has no nonzero payload bytes");
    // Dynamic source size: opaque user-data is bounded by the supplied native
    // span, without an SDK packet-buffer cap or entropy decode allocation.
    Bytes large{0,0,1,0xb2};large.resize(4+1024*1024,0x55);check(fresh.parse(large,result,f)&&!result,"large host control payload");
    std::puts("[mpeg2-timing-pass] synthetic header ownership/transaction/cardinality checks");return 0;
}
