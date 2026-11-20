#include "mka/movie_source_syntax_index.hpp"
#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
namespace mka::video {
struct MovieSourceSyntaxState {
    std::string key;
    std::shared_ptr<const MovieSourceObservationIdentity> identity;
    std::vector<std::shared_ptr<const MovieSourceSyntaxPicture>> pictures;
};
namespace {
std::size_t prefix(std::span<const std::uint8_t> bytes,std::size_t start) noexcept {
    for(auto i=start;i<bytes.size()&&bytes.size()-i>=4;++i)
        if(bytes[i]==0&&bytes[i+1]==0&&bytes[i+2]==1)return i;
    return bytes.size();
}
}
std::optional<NativeMovieSourceSyntaxIndex> NativeMovieSourceSyntaxIndex::build(
    const NativeMoviePesIndex& index,std::uint8_t stream,std::string key,
    std::shared_ptr<const MovieSourceObservationIdentity> identity,MovieSourceSyntaxFailure& failure) noexcept {
    failure={};
    auto fail=[&](MovieSourceSyntaxError kind,std::uint64_t offset){failure.kind=kind;failure.es_byte=offset;};
    if(key.empty()||!identity){fail(MovieSourceSyntaxError::invalid_source,0);return std::nullopt;}
    if(stream<0xe0||stream>0xef){fail(MovieSourceSyntaxError::invalid_stream,0);return std::nullopt;}
    try {
        auto checked_reader=index.reader(stream,failure.pes);
        if(!checked_reader){fail(failure.pes.kind==MoviePesError::moved?MovieSourceSyntaxError::invalid_owner:MovieSourceSyntaxError::invalid_stream,0);return std::nullopt;}
        auto streams=index.streams();const auto found=std::find_if(streams.begin(),streams.end(),[&](const auto& s){return s.stream_id==stream;});
        if(found==streams.end()){fail(MovieSourceSyntaxError::invalid_stream,0);return std::nullopt;}
        std::vector<std::uint8_t> bytes;
        if(found->bytes>bytes.max_size()){fail(MovieSourceSyntaxError::host_capacity,0);return std::nullopt;}
        bytes.resize(static_cast<std::size_t>(found->bytes));
        if(!index.read_range(stream,0,bytes,failure.pes)){fail(failure.pes.kind==MoviePesError::allocation?MovieSourceSyntaxError::allocation:MovieSourceSyntaxError::source_io,0);return std::nullopt;}
        auto state=std::make_shared<MovieSourceSyntaxState>();state->key=std::move(key);state->identity=std::move(identity);
        auto quant=std::make_shared<std::vector<MovieSourceSyntaxUnit>>();std::size_t quant_begin=0;
        std::optional<MovieSequenceHeader> sequence;std::optional<MovieSequenceExtension> extension;
        std::optional<MovieDisplayExtension> display;std::optional<MovieGopHeader> gop;std::optional<std::uint64_t> gop_byte;
        std::uint64_t sequence_byte=0,extension_byte=0;
        std::optional<MovieSourceSyntaxPicture> current;bool has_coding=false,has_slice=false;unsigned primary=0;
        auto finish=[&](){
            if(!current)return true;
            if(!has_coding){fail(MovieSourceSyntaxError::missing_coding_extension,current->picture_es_byte);return false;}
            if(!has_slice){fail(MovieSourceSyntaxError::missing_slice,current->picture_es_byte);return false;}
            current->quant_history_end=quant->size();
            state->pictures.push_back(std::make_shared<const MovieSourceSyntaxPicture>(std::move(*current)));
            current.reset();return true;
        };
        std::size_t pos=0;
        while(pos<bytes.size()) {
            const auto begin=prefix(bytes,pos);
            if(begin==bytes.size()) {
                // A prefix truncated at EOF is unavailable syntax, not a new unit.
                const auto tail=std::span(bytes).subspan(pos);
                if((tail.size()>=3&&tail[tail.size()-3]==0&&tail[tail.size()-2]==0&&tail.back()==1)){
                    fail(MovieSourceSyntaxError::need_more,bytes.size()-3);return std::nullopt;
                }
                if(std::any_of(tail.begin(),tail.end(),[](auto b){return b!=0;})){
                    fail(MovieSourceSyntaxError::malformed,pos);return std::nullopt;
                }
                break;
            }
            if(std::any_of(bytes.begin()+pos,bytes.begin()+begin,[](auto b){return b!=0;})){
                fail(MovieSourceSyntaxError::malformed,pos);return std::nullopt;
            }
            const auto code=bytes[begin+3];
            if(code>=1&&code<=0xaf) {
                if(!current||!has_coding){fail(MovieSourceSyntaxError::invalid_order,begin);return std::nullopt;}
                const auto next=prefix(bytes,begin+4);
                if(next==begin+4||bytes.size()==begin+4){fail(MovieSourceSyntaxError::missing_slice,begin);return std::nullopt;}
                has_slice=true;pos=next;continue;
            }
            if(code==0||code==0xb3||code==0xb8||code==0xb7)if(!finish())return std::nullopt;
            auto parsed=parse_movie_header_unit(std::span(bytes).subspan(begin));
            if(parsed.status!=MovieHeaderUnitStatus::parsed||!parsed.unit){
                failure.unit=parsed.failure;
                fail(parsed.status==MovieHeaderUnitStatus::need_more?MovieSourceSyntaxError::need_more:
                    parsed.status==MovieHeaderUnitStatus::unsupported?MovieSourceSyntaxError::unsupported:
                    parsed.failure.kind==MovieHeaderUnitError::allocation?MovieSourceSyntaxError::allocation:
                    parsed.failure.kind==MovieHeaderUnitError::host_capacity?MovieSourceSyntaxError::host_capacity:MovieSourceSyntaxError::malformed,begin);
                return std::nullopt;
            }
            auto unit=std::move(*parsed.unit);
            if(unit.consumed_bytes>bytes.size()-begin||unit.consumed_bytes<4){fail(MovieSourceSyntaxError::malformed,begin);return std::nullopt;}
            pos=begin+static_cast<std::size_t>(unit.consumed_bytes);
            if(auto* p=std::get_if<MovieSequenceHeader>(&unit.payload)) {
                sequence=std::move(*p);sequence_byte=begin;extension.reset();display.reset();gop.reset();gop_byte.reset();quant_begin=quant->size();primary=1;
            } else if(auto* p=std::get_if<MovieSequenceExtension>(&unit.payload)) {
                if(!sequence){fail(MovieSourceSyntaxError::missing_sequence,begin);return std::nullopt;}
                if(current||extension){fail(MovieSourceSyntaxError::invalid_order,begin);return std::nullopt;}
                extension=*p;extension_byte=begin;
            } else if(auto* p=std::get_if<MovieDisplayExtension>(&unit.payload)) {
                if(!extension){fail(MovieSourceSyntaxError::missing_sequence_extension,begin);return std::nullopt;}
                if(current){fail(MovieSourceSyntaxError::invalid_order,begin);return std::nullopt;}display=*p;
            } else if(std::holds_alternative<MovieQuantMatrixExtension>(unit.payload)) {
                if(!extension){fail(MovieSourceSyntaxError::missing_sequence_extension,begin);return std::nullopt;}
                quant->push_back({begin,std::move(unit)});
            } else if(auto* p=std::get_if<MovieGopHeader>(&unit.payload)) {
                if(!extension){fail(MovieSourceSyntaxError::missing_sequence_extension,begin);return std::nullopt;}
                gop=*p;gop_byte=begin;primary=2;
            } else if(auto* p=std::get_if<MoviePictureHeader>(&unit.payload)) {
                if(!sequence){fail(MovieSourceSyntaxError::missing_sequence,begin);return std::nullopt;}
                if(!extension){fail(MovieSourceSyntaxError::missing_sequence_extension,begin);return std::nullopt;}
                MovieSourceSyntaxPicture record;record.source_key=state->key;record.identity=state->identity;
                record.picture_es_byte=begin;record.sequence_es_byte=sequence_byte;record.sequence_extension_es_byte=extension_byte;
                auto owner=index.lookup(stream,begin,failure.pes);
                if(!owner){fail(MovieSourceSyntaxError::source_io,begin);return std::nullopt;}
                record.pes=*owner;record.quarter_pts=owner->quarter_pts();record.sequence=*sequence;record.sequence_extension=*extension;
                record.display_extension=display;record.gop=gop;record.gop_es_byte=gop_byte;record.picture=std::move(*p);
                record.quant_matrix_units=quant;record.quant_history_begin=quant_begin;current=std::move(record);has_coding=false;has_slice=false;primary=3;
            } else if(auto* p=std::get_if<MoviePictureCodingExtension>(&unit.payload)) {
                if(!current||has_coding||has_slice){fail(MovieSourceSyntaxError::invalid_order,begin);return std::nullopt;}
                current->coding=*p;current->coding_extension_es_byte=begin;has_coding=true;
            } else if(std::holds_alternative<MovieUserDataHeader>(unit.payload)) {
                if(primary==3&&current)current->userdata.push_back({begin,std::move(unit)});
            } else if(std::holds_alternative<MovieSequenceEndHeader>(unit.payload)) {
                sequence.reset();extension.reset();display.reset();gop.reset();gop_byte.reset();quant_begin=quant->size();primary=1;
            }
        }
        if(!finish())return std::nullopt;
        if(state->pictures.empty()){fail(MovieSourceSyntaxError::missing_picture,0);return std::nullopt;}
        return NativeMovieSourceSyntaxIndex(std::move(state));
    }catch(const std::bad_alloc&){fail(MovieSourceSyntaxError::allocation,0);return std::nullopt;}
     catch(const std::length_error&){fail(MovieSourceSyntaxError::host_capacity,0);return std::nullopt;}
}
std::shared_ptr<const MovieSourceSyntaxPicture> NativeMovieSourceSyntaxIndex::lookup(std::string_view key,
    const std::shared_ptr<const MovieSourceObservationIdentity>& identity,std::uint64_t offset,MovieSourceSyntaxFailure& f)const noexcept {
    f={};f.es_byte=offset;
    if(!state_){f.kind=MovieSourceSyntaxError::invalid_owner;return {};}
    if(key!=state_->key||!identity||identity!=state_->identity){f.kind=MovieSourceSyntaxError::foreign_source;return {};}
    const auto it=std::lower_bound(state_->pictures.begin(),state_->pictures.end(),offset,[](const auto& p,auto n){return p->picture_es_byte<n;});
    if(it==state_->pictures.end()||(*it)->picture_es_byte!=offset){f.kind=MovieSourceSyntaxError::range;return {};}
    return *it;
}
std::span<const std::shared_ptr<const MovieSourceSyntaxPicture>> NativeMovieSourceSyntaxIndex::pictures()const noexcept {
    if(!state_)return {};return state_->pictures;
}
} // namespace mka::video
