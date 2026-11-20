#pragma once
#include "mka/movie_source_observations.hpp"
#include <limits>
#include <string_view>

namespace mka::video {
enum class MovieInputReceiptError {none,null_receipt,foreign_source,after_program_end,
    invalid_stream,input_order,byte_order,unobserved_range};
// Source-owned native acceptance facts. This check neither executes codec work
// nor admits headers, advances frame positions, selects a clock or ends playback.
inline MovieInputReceiptError check_movie_input_receipt(
        const std::shared_ptr<const MovieCodecInputReceipt>& receipt,std::string_view key,
        const std::shared_ptr<const MovieSourceObservationIdentity>& identity,
        const std::shared_ptr<const MovieCodecInputReceipt>& previous,
        std::uint64_t observed_end,bool program_end,std::uint64_t initial_es_begin=0) noexcept {
    if(!receipt)return MovieInputReceiptError::null_receipt;
    if(!identity||key.empty()||receipt->source_key()!=key||receipt->source_identity()!=identity||
       (previous&&(previous->source_key()!=key||previous->source_identity()!=identity)))
        return MovieInputReceiptError::foreign_source;
    if(program_end)return MovieInputReceiptError::after_program_end;
    if(receipt->stream_id()<0xe0||receipt->stream_id()>0xef||
       (previous&&receipt->stream_id()!=previous->stream_id()))return MovieInputReceiptError::invalid_stream;
    const auto last=previous?previous->input_id():0;
    if(last==std::numeric_limits<std::uint64_t>::max()||receipt->input_id()!=last+1)
        return MovieInputReceiptError::input_order;
    if(receipt->es_begin()!=(previous?previous->es_end():initial_es_begin))return MovieInputReceiptError::byte_order;
    if(receipt->es_end()<=receipt->es_begin()||receipt->es_end()>observed_end)
        return MovieInputReceiptError::unobserved_range;
    return MovieInputReceiptError::none;
}
} // namespace mka::video
