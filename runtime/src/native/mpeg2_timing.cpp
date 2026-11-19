#include "mka/mpeg2_timing.hpp"

namespace mka::video {
namespace {
struct Bits {
    std::span<const std::uint8_t> data; std::size_t byte=0; unsigned bit=0;
    bool get(unsigned n,std::uint32_t& v) noexcept {
        v=0;
        for(unsigned i=0;i<n;++i){
            if(byte==data.size())return false;
            v=(v<<1)|((data[byte]>>(7-bit))&1u);
            if(++bit==8){bit=0;++byte;}
        }
        return true;
    }
    bool skip(unsigned n) noexcept {std::uint32_t v;while(n){unsigned k=n>32?32:n;if(!get(k,v))return false;n-=k;}return true;}
};
bool prefix(std::span<const std::uint8_t> b,std::size_t p) noexcept {
    return b.size()-p>=3&&b[p]==0&&b[p+1]==0&&b[p+2]==1;
}
}
const char* mpeg2_timing_error_name(Mpeg2TimingError e) noexcept {
#define E(x) case Mpeg2TimingError::x:return #x
    switch(e){E(none);E(invalid_start_code);E(truncated_header);E(invalid_marker);E(invalid_sequence);E(unsupported_rate);E(unsupported_extension);E(unsupported_start_code);E(missing_sequence);E(missing_sequence_extension);E(duplicate_extension);E(missing_picture);E(multiple_pictures);E(unsupported_field_picture);E(invalid_picture);E(missing_picture_extension);E(missing_slices);E(unsupported_composite_display);E(picture_in_extradata);}
#undef E
    return "unknown_mpeg2_timing_error";
}
bool NativeMpeg2Timing::prime(std::span<const std::uint8_t> bytes,Mpeg2TimingFailure& f) noexcept {
    NativeMpeg2Timing candidate; std::optional<Mpeg2PictureTiming> picture;
    if(!candidate.parse(bytes,picture,f))return false;
    if(picture){f={Mpeg2TimingError::picture_in_extradata,0};return false;}
    if(!candidate.sequence_header_){f={Mpeg2TimingError::missing_sequence,0};return false;}
    if(!candidate.sequence_extension_){f={Mpeg2TimingError::missing_sequence_extension,0};return false;}
    *this=candidate;return true;
}
bool NativeMpeg2Timing::parse(std::span<const std::uint8_t> bytes,
                            std::optional<Mpeg2PictureTiming>& out,Mpeg2TimingFailure& f) noexcept {
    auto next=*this;std::optional<Mpeg2PictureTiming> picture;
    bool picture_extension=false,slices=false,terminal=false; f={};out.reset();
    auto fail=[&](Mpeg2TimingError e,std::size_t p){f={e,p};return false;};
    std::size_t pos=0;
    while(pos<bytes.size()){
        // Leading zero_byte padding is legal; arbitrary unframed bytes are not.
        while(pos<bytes.size()&&!prefix(bytes,pos)&&bytes[pos]==0)++pos;
        if(pos==bytes.size())break;
        if(terminal)return fail(Mpeg2TimingError::invalid_sequence,pos);
        if(!prefix(bytes,pos))return fail(Mpeg2TimingError::invalid_start_code,pos);
        if(bytes.size()-pos<4)return fail(Mpeg2TimingError::truncated_header,pos);
        const auto code=bytes[pos+3];const auto start=pos;
        std::size_t end=pos+4;
        while(end<bytes.size()&&!prefix(bytes,end))++end;
        Bits bits{bytes.subspan(pos+4,end-pos-4)};std::uint32_t x=0,y=0;
        auto get=[&](unsigned n,std::uint32_t& v){return bits.get(n,v);};
        auto trunc=[&](){return fail(Mpeg2TimingError::truncated_header,start);};
        if(code==0xb3){
            if(picture)return fail(Mpeg2TimingError::invalid_sequence,start);
            Mpeg2SequenceTiming seq;
            if(!get(12,x))return trunc();seq.width=static_cast<std::uint16_t>(x);
            if(!get(12,x))return trunc();seq.height=static_cast<std::uint16_t>(x);
            if(!get(4,x)||!get(4,y))return trunc();
            if(!seq.width||!seq.height||!x||x>4)return fail(Mpeg2TimingError::invalid_sequence,start);
            if(y<1||y>8)return fail(Mpeg2TimingError::unsupported_rate,start);
            seq.frame_rate_code=static_cast<std::uint8_t>(y);
            if(!bits.skip(18)||!get(1,x))return trunc();
            if(x!=1)return fail(Mpeg2TimingError::invalid_marker,start);
            if(!bits.skip(11)||!get(1,x))return trunc();
            if(x&&!bits.skip(512))return trunc();
            if(!get(1,x)||(x&&!bits.skip(512)))return trunc();
            next.sequence_=seq;next.sequence_header_=true;next.sequence_extension_=false;
            // A new sequence may not inherit a GOP belonging to an old sequence.
            next.gop_.reset();
        }else if(code==0xb5){
            if(!get(4,x))return trunc();
            const auto id=x;
            if(id==1){
                if(!next.sequence_header_)return fail(Mpeg2TimingError::missing_sequence,start);
                if(picture)return fail(Mpeg2TimingError::invalid_sequence,start);
                if(next.sequence_extension_)return fail(Mpeg2TimingError::duplicate_extension,start);
                if(!bits.skip(8)||!get(1,x))return trunc();next.sequence_.progressive_sequence=x!=0;
                if(!get(2,x))return trunc();if(!x)return fail(Mpeg2TimingError::invalid_sequence,start);
                next.sequence_.chroma_format=static_cast<std::uint8_t>(x);
                if(!get(2,x)||!get(2,y))return trunc();
                next.sequence_.width|=static_cast<std::uint16_t>(x<<12);next.sequence_.height|=static_cast<std::uint16_t>(y<<12);
                if(!bits.skip(12)||!get(1,x))return trunc();if(x!=1)return fail(Mpeg2TimingError::invalid_marker,start);
                if(!bits.skip(9)||!get(2,x)||!get(5,y))return trunc();
                next.sequence_.extension_n=static_cast<std::uint8_t>(x);next.sequence_.extension_d=static_cast<std::uint8_t>(y);
                next.sequence_extension_=true;
            }else if(id==8){
                if(!picture)return fail(Mpeg2TimingError::missing_picture,start);
                if(picture_extension)return fail(Mpeg2TimingError::duplicate_extension,start);
                for(unsigned i=0;i<4;++i){if(!get(4,x))return trunc();if(!x)return fail(Mpeg2TimingError::invalid_picture,start);}
                if(!bits.skip(2)||!get(2,x))return trunc();
                if(x==0)return fail(Mpeg2TimingError::invalid_picture,start);
                if(x!=3)return fail(Mpeg2TimingError::unsupported_field_picture,start);
                picture->picture_structure=static_cast<std::uint8_t>(x);
                if(!get(1,x))return trunc();picture->top_field_first=x!=0;
                if(!bits.skip(5)||!get(1,x))return trunc();picture->repeat_first_field=x!=0;
                if(!bits.skip(1)||!get(1,x))return trunc();picture->progressive_frame=x!=0;
                if(!get(1,x))return trunc();
                if(x)return fail(Mpeg2TimingError::unsupported_composite_display,start);
                picture_extension=true;
            }else if(id==2){
                if(!next.sequence_extension_||picture)return fail(Mpeg2TimingError::invalid_sequence,start);
                if(!bits.skip(3)||!get(1,x))return trunc();
                if(x&&!bits.skip(24))return trunc();
                if(!bits.skip(14)||!get(1,x))return trunc();
                if(x!=1)return fail(Mpeg2TimingError::invalid_marker,start);
                if(!bits.skip(14))return trunc();
            }else if(id==3){
                if(!next.sequence_extension_)return fail(Mpeg2TimingError::missing_sequence_extension,start);
                for(unsigned i=0;i<4;++i){if(!get(1,x)||(x&&!bits.skip(512)))return trunc();}
            }else return fail(Mpeg2TimingError::unsupported_extension,start);
        }else if(code==0xb8){
            if(picture)return fail(Mpeg2TimingError::invalid_picture,start);
            if(!next.sequence_extension_)return fail(Mpeg2TimingError::missing_sequence_extension,start);
            Mpeg2GopTiming g;
            if(!get(25,x)||!get(1,y))return trunc();g.raw_time_code=x;g.closed_gop=y!=0;
            if(!get(1,y))return trunc();g.broken_link=y!=0;
            if(!(x&(1u<<12)))return fail(Mpeg2TimingError::invalid_marker,start);
            g.drop_frame=(x>>24)!=0;g.hours=static_cast<std::uint8_t>((x>>19)&31);
            g.minutes=static_cast<std::uint8_t>((x>>13)&63);g.seconds=static_cast<std::uint8_t>((x>>6)&63);g.pictures=static_cast<std::uint8_t>(x&63);
            next.gop_=g;
        }else if(code==0){
            if(picture)return fail(Mpeg2TimingError::multiple_pictures,start);
            if(!next.sequence_header_)return fail(Mpeg2TimingError::missing_sequence,start);
            if(!next.sequence_extension_)return fail(Mpeg2TimingError::missing_sequence_extension,start);
            Mpeg2PictureTiming pic;pic.sequence=next.sequence_;pic.gop=next.gop_;
            pic.packet_picture_offset=static_cast<std::uint64_t>(start);
            if(!get(10,x)||!get(3,y))return trunc();
            if(y<1||y>3)return fail(Mpeg2TimingError::invalid_picture,start);
            pic.temporal_reference=static_cast<std::uint16_t>(x);pic.coding_type=static_cast<std::uint8_t>(y);
            if(!bits.skip(16))return trunc();
            if(y==2||y==3){if(!bits.skip(1)||!get(3,x))return trunc();if(!x)return fail(Mpeg2TimingError::invalid_picture,start);}
            if(y==3){if(!bits.skip(1)||!get(3,x))return trunc();if(!x)return fail(Mpeg2TimingError::invalid_picture,start);}
            do{if(!get(1,x))return trunc();if(x&&!bits.skip(8))return trunc();}while(x);
            picture=pic;
        }else if(code>=1&&code<=0xaf){
            if(!picture)return fail(Mpeg2TimingError::missing_picture,start);
            if(!picture_extension)return fail(Mpeg2TimingError::missing_picture_extension,start);
            if(end==start+4)return trunc();slices=true; // entropy bytes belong to the codec
        }else if(code==0xb2){
            // User data is opaque codec payload; no text or timing inference.
        }else if(code==0xb7){
            // Sequence-end has no payload. The unit scanner may have retained
            // zero_byte padding before EOF/the next prefix, never other data.
            for(std::size_t tail=start+4;tail<end;++tail)
                if(bytes[tail])return fail(Mpeg2TimingError::invalid_sequence,tail);
            // End-of-sequence is a control unit, not a second picture. Clear
            // inherited metadata for a future sequence, retaining this output.
            next=NativeMpeg2Timing{};
            terminal=true;
        }else return fail(Mpeg2TimingError::unsupported_start_code,start);
        pos=end;
    }
    if(picture&&!picture_extension)return fail(Mpeg2TimingError::missing_picture_extension,bytes.size());
    if(picture&&!slices)return fail(Mpeg2TimingError::missing_slices,bytes.size());
    *this=next;out=picture;return true;
}
}
