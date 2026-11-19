#include "mka/movie_metadata.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
namespace mka::video {
struct MovieMetadataStorage {
    std::vector<std::uint8_t> ordinary;
    std::vector<MovieMetadataOrigin> origins;
    std::vector<MovieSofdecHeader> headers;
    bool latch=false;
    MovieMetadataSelection selected=MovieMetadataSelection::empty;
    std::size_t root_offset=0,root_size=0;
};
const char* movie_metadata_error_name(MovieMetadataError e) noexcept {
    switch(e){
    case MovieMetadataError::none:return "none";
    case MovieMetadataError::invalid_packet_extent:return "invalid_packet_extent";
    case MovieMetadataError::packet_order:return "packet_order";
    case MovieMetadataError::missing_header_prefix:return "missing_header_prefix";
    case MovieMetadataError::invalid_header_prefix:return "invalid_header_prefix";
    case MovieMetadataError::malformed_version:return "malformed_version";
    case MovieMetadataError::version_overflow:return "version_overflow";
    case MovieMetadataError::truncated_tag_header:return "truncated_tag_header";
    case MovieMetadataError::invalid_tag_length:return "invalid_tag_length";
    case MovieMetadataError::tag_body_out_of_bounds:return "tag_body_out_of_bounds";
    case MovieMetadataError::allocation_failed:return "allocation_failed";
    case MovieMetadataError::host_capacity:return "host_capacity";
    }return "unknown_movie_metadata_error";
}
namespace {
constexpr char marker[]="SofdecStream            ";
bool prefix_valid(const MoviePrivate2Payload& p) {
    const auto& h=*p.adjacent_mpeg1_prefix;const auto& b=h.bytes;
    if(h.file_offset>UINT64_MAX-18||h.file_offset+18!=p.payload_offset||
       b[0]||b[1]||b[2]!=1||b[3]!=0xba||b[12]||b[13]||b[14]!=1||b[15]!=0xbf||
       ((unsigned(b[16])<<8)|b[17])!=p.bytes.size())return false;
    unsigned bit=32;
    const auto get=[&](unsigned n){unsigned v=0;while(n--){v=(v<<1)|((b[bit/8]>>(7-bit%8))&1);++bit;}return v;};
    if(get(4)!=2)return false;get(3);if(get(1)!=1)return false;get(15);if(get(1)!=1)return false;
    get(15);if(get(1)!=1||get(1)!=1)return false;get(22);return get(1)==1;
}
// Bounded conservative native policy for the source's strstr+decimal-prefix
// helper: require digits, literal '.', digits and a space/NUL field terminator.
MovieMetadataError version(std::span<const std::uint8_t> field,std::uint64_t& major,std::uint64_t& minor,std::uint64_t& code){
    std::size_t end=0;while(end<field.size()&&field[end])++end;
    std::size_t at=0;for(;at+4<=end;++at)if(std::memcmp(field.data()+at,"Ver.",4)==0)break;
    if(at+4>end)return MovieMetadataError::malformed_version;at+=4;
    const auto number=[&](std::uint64_t& value){value=0;const auto start=at;
        while(at<end&&field[at]>='0'&&field[at]<='9'){
            const auto digit=std::uint64_t(field[at]-'0');
            if(value>(UINT64_MAX-digit)/10)return MovieMetadataError::version_overflow;
            value=value*10+digit;++at;
        }return at==start?MovieMetadataError::malformed_version:MovieMetadataError::none;};
    auto e=number(major);if(e!=MovieMetadataError::none)return e;
    if(at>=end||field[at++]!='.')return MovieMetadataError::malformed_version;
    e=number(minor);if(e!=MovieMetadataError::none)return e;
    if(at<end&&field[at]!=' ')return MovieMetadataError::malformed_version;
    if(major>(UINT64_MAX-minor)/100)return MovieMetadataError::version_overflow;
    code=major*100+minor;return MovieMetadataError::none;
}
}
std::optional<NativeMovieMetadata> NativeMovieMetadata::assemble(std::vector<MoviePrivate2Payload> packets,bool initial,MovieMetadataFailure& failure){
    failure={};std::size_t current=0;
    const auto fail=[&](MovieMetadataError e,std::uint64_t offset=0)->std::optional<NativeMovieMetadata>{failure={e,current,offset};return {};};
    try {
        auto out=std::make_shared<MovieMetadataStorage>();out->latch=initial;
        std::uint64_t previous_end=0;bool first=true;
        for(const auto& p:packets){
            if(p.packet_offset>UINT64_MAX-6||p.packet_offset+6!=p.payload_offset||p.bytes.size()>UINT16_MAX||
               p.bytes.size()>UINT64_MAX-p.payload_offset)return fail(MovieMetadataError::invalid_packet_extent);
            if(!first&&p.packet_offset<previous_end)return fail(MovieMetadataError::packet_order);
            const bool header=p.bytes.size()>=2048-18&&std::memcmp(p.bytes.data()+14,marker,24)==0;
            if(header){
                if(!p.adjacent_mpeg1_prefix)return fail(MovieMetadataError::missing_header_prefix);
                if(!prefix_valid(p))return fail(MovieMetadataError::invalid_header_prefix);
                if(!first&&p.adjacent_mpeg1_prefix->file_offset<previous_end)
                    return fail(MovieMetadataError::packet_order);
                MovieSofdecHeader h{};h.packet_index=current;h.file_offset=p.adjacent_mpeg1_prefix->file_offset;
                const auto e=version(std::span(p.bytes).subspan(78,32),h.version_major,h.version_minor,h.version_code);
                if(e!=MovieMetadataError::none)return fail(e);
                h.full_bytes.insert(h.full_bytes.end(),p.adjacent_mpeg1_prefix->bytes.begin(),p.adjacent_mpeg1_prefix->bytes.end());
                h.full_bytes.insert(h.full_bytes.end(),p.bytes.begin(),p.bytes.end());
                h.callback_intended=true;h.first_header_forward_private_index0=!out->latch;
                h.source_first_cache_bytes=out->latch?0:std::min<std::uint64_t>(h.full_bytes.size(),2048);
                out->latch=true;out->headers.push_back(std::move(h));
            }else{
                if(p.bytes.size()>out->ordinary.max_size()-out->ordinary.size())return fail(MovieMetadataError::host_capacity);
                out->origins.push_back({current,p.payload_offset,out->ordinary.size(),p.bytes.size()});
                out->ordinary.insert(out->ordinary.end(),p.bytes.begin(),p.bytes.end());
            }
            previous_end=p.payload_offset+p.bytes.size();first=false;++current;
        }
        const auto& bytes=out->ordinary;
        const auto stream_fail=[&](MovieMetadataError e,std::size_t offset){
            for(const auto& origin:out->origins)
                if(offset>=origin.stream_offset && offset-origin.stream_offset<origin.bytes){current=origin.packet_index;break;}
            return fail(e,offset);
        };
        if(!bytes.empty())out->selected=MovieMetadataSelection::no_root;
        // Source first-target/endtag walk: bytes after target/end remain owned
        // opaque data; no fabricated claim that their technical body was parsed.
        for(std::size_t at=0;at<bytes.size();){
            if(bytes.size()-at<16)return stream_fail(MovieMetadataError::truncated_tag_header,at);
            std::uint64_t length=0;
            for(unsigned i=0;i<7;++i){const auto c=bytes[at+8+i];unsigned d;
                if(c>='0'&&c<='9')d=c-'0';else if(c>='A'&&c<='F')d=c-'A'+10;else if(c>='a'&&c<='f')d=c-'a'+10;
                else return stream_fail(MovieMetadataError::invalid_tag_length,at+8+i);length=length*16+d;}
            if(length>bytes.size()-at-16)return stream_fail(MovieMetadataError::tag_body_out_of_bounds,at);
            if(std::memcmp(bytes.data()+at,"CRITAGS",7)==0){out->selected=MovieMetadataSelection::root_found;out->root_offset=at;out->root_size=16+static_cast<std::size_t>(length);break;}
            if(std::memcmp(bytes.data()+at,"CRITAGE",7)==0){out->selected=MovieMetadataSelection::end_before_root;break;}
            at+=16+static_cast<std::size_t>(length);
        }
        return NativeMovieMetadata(std::move(out));
    }catch(const std::bad_alloc&){return fail(MovieMetadataError::allocation_failed);}
     catch(const std::length_error&){return fail(MovieMetadataError::host_capacity);}
}
bool NativeMovieMetadata::next_first_header_latched() const noexcept{return storage_&&storage_->latch;}
MovieMetadataSelection NativeMovieMetadata::selection() const noexcept{return storage_?storage_->selected:MovieMetadataSelection::invalid_owner;}
std::span<const std::uint8_t> NativeMovieMetadata::ordinary_stream() const noexcept {return storage_?std::span<const std::uint8_t>(storage_->ordinary):std::span<const std::uint8_t>{};}
std::span<const MovieMetadataOrigin> NativeMovieMetadata::origins() const noexcept {return storage_?std::span<const MovieMetadataOrigin>(storage_->origins):std::span<const MovieMetadataOrigin>{};}
std::span<const MovieSofdecHeader> NativeMovieMetadata::sofdec_headers() const noexcept {return storage_?std::span<const MovieSofdecHeader>(storage_->headers):std::span<const MovieSofdecHeader>{};}
std::span<const std::uint8_t> NativeMovieMetadata::selected_root_bytes() const noexcept {return storage_?std::span<const std::uint8_t>(storage_->ordinary).subspan(storage_->root_offset,storage_->root_size):std::span<const std::uint8_t>{};}
} // namespace mka::video
