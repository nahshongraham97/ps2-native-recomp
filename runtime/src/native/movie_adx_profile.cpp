#include "mka/movie_adx_profile.hpp"
#include <limits>
#include <new>
#include <stdexcept>
namespace mka::video {
namespace {
bool fail(MovieAdxStreamFailure& f,MovieAdxStreamError kind,const char* operation) noexcept {
    f.kind=kind;f.operation=operation;return false;
}
bool same_descriptor(const MovieAdxHeader& a,const MovieAdxHeader& b) noexcept {
    return a.data_start==b.data_start&&a.encoding==b.encoding&&a.block_bytes==b.block_bytes&&
        a.sample_bits==b.sample_bits&&a.source_planes==b.source_planes&&
        a.sample_rate==b.sample_rate&&a.declared_samples==b.declared_samples&&
        a.cutoff==b.cutoff&&a.version==b.version&&a.flags==b.flags&&a.histories==b.histories;
}
struct CoefficientRow {std::uint16_t cutoff;std::uint32_t rate;std::int16_t first,second;};
// Source-helper sequence measured OFFLINE in native-adx-coefficient-reference-
// 20261119/validation-01. The helper itself is NOT a runtime dependency.
constexpr CoefficientRow rows[]={{500,48000,7400,-3342},{500,44100,7334,-3283}};
static_assert(rows[0].rate!=rows[1].rate&&rows[0].cutoff==500&&rows[1].cutoff==500);
}
std::optional<AdxBlockProfile> NativeMovieAdxProfileResolver::resolve(const MovieAdxHeader& header,
        MovieAdxStreamFailure& f) noexcept {
    f={};
    auto parsed=parse_movie_adx_header(header.raw_header,f.header);
    if(!parsed){
        const auto kind=f.header.kind==MovieAdxHeaderError::allocation?MovieAdxStreamError::allocation:
            f.header.kind==MovieAdxHeaderError::host_capacity?MovieAdxStreamError::host_capacity:MovieAdxStreamError::header;
        fail(f,kind,"ADX profile raw header validation failed");return {};
    }
    if(parsed->data_start!=header.raw_header.size()||!same_descriptor(*parsed,header)){
        fail(f,MovieAdxStreamError::profile,"ADX profile descriptor/raw-header identity mismatch");return {};
    }
    // These are the original signed coefficient-function operand domains, not
    // native resource/rate capacities. Broader inputs need a proved expansion.
    if(!header.cutoff||header.cutoff>std::numeric_limits<std::int16_t>::max()||
       !header.sample_rate||header.sample_rate>std::uint64_t(std::numeric_limits<std::int32_t>::max())){
        fail(f,MovieAdxStreamError::profile,"ADX source_signed_coefficient_input_domain unsupported");return {};
    }
    const CoefficientRow* selected=nullptr;
    for(const auto& row:rows)if(header.cutoff==row.cutoff&&header.sample_rate==row.rate){selected=&row;break;}
    if(!selected){fail(f,MovieAdxStreamError::profile,"ADX unsupported_coefficient_inputs");return {};}
    try {
        std::vector<std::pair<std::int16_t,std::int16_t>> coefficients(2,{selected->first,selected->second});
        std::vector<std::pair<std::int16_t,std::int16_t>> histories(header.histories.begin(),header.histories.end());
        return AdxBlockProfile(AdxBlockMode::version4_flags0,std::move(coefficients),std::move(histories));
    }catch(const std::bad_alloc&){fail(f,MovieAdxStreamError::allocation,"ADX owned resolved profile allocation");return {};}
    catch(const std::length_error&){fail(f,MovieAdxStreamError::host_capacity,"ADX resolved profile vector extent");return {};}
}
} // namespace mka::video
