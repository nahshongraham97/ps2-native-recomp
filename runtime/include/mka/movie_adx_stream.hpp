#pragma once
#include "mka/movie_adx_header.hpp"
#include "mka/movie_adx_blocks.hpp"
#include <memory>
#include <thread>
namespace mka::video {
enum class MovieAdxStreamError {
    none,invalid_owner,wrong_thread,reentrant,failed_owner,invalid_input,
    header,profile,decode,premature_marker,invalid_marker,duplicate_header,
    trailing_data,incomplete,overflow,host_capacity,allocation,resolver
};
struct MovieAdxStreamFailure {
    MovieAdxStreamError kind=MovieAdxStreamError::none;
    const char* operation="";
    MovieAdxHeaderFailure header;
    AdxBlockFailure block;
    MovieAdxStreamError original_kind=MovieAdxStreamError::none;
};
class MovieAdxProfileResolver {
public:
    virtual ~MovieAdxProfileResolver()=default;
    virtual std::optional<AdxBlockProfile> resolve(const MovieAdxHeader&,
        MovieAdxStreamFailure&)=0;
};
enum class MovieAdxStreamStatus {need_more,frame,ended,error};
struct MovieAdxStreamFrame {
    std::uint64_t first_source_frame=0,sample_rate=0,raw_decoded_frames=0;
    std::vector<std::vector<std::int16_t>> planes;
    // Source indices only. No PTS, speaker labels or master-clock origin.
};
struct MovieAdxStreamResult {
    MovieAdxStreamStatus status=MovieAdxStreamStatus::need_more;
    std::optional<MovieAdxStreamFrame> frame;
    std::optional<MovieAdxHeader> new_header;
    bool header_available=false,encoded_ended=false;
    std::vector<std::uint8_t> completed_marker;
};
struct MovieAdxStreamSnapshot {
    std::optional<MovieAdxHeader> header;
    std::vector<std::pair<std::int16_t,std::int16_t>> histories;
    std::vector<std::uint8_t> carry,marker;
    std::uint64_t raw_decoded_frames=0,published_frames=0;
    bool encoded_ended=false,failed=false;
};
// Creating-thread, serialized, non-reentrant owned normal/no-loop stream.
// Resolver and actual same-source packet association are explicit caller data.
// Returned results/snapshots own their bytes independently of this owner.
class NativeMovieAdxStream {
public:
    static std::unique_ptr<NativeMovieAdxStream> create(std::vector<std::uint8_t> exact_extradata,
        std::unique_ptr<MovieAdxProfileResolver>,MovieAdxStreamFailure&) noexcept;
    ~NativeMovieAdxStream();
    NativeMovieAdxStream(const NativeMovieAdxStream&)=delete;
    NativeMovieAdxStream& operator=(const NativeMovieAdxStream&)=delete;
    MovieAdxStreamResult feed(std::span<const std::uint8_t>,MovieAdxStreamFailure&) noexcept;
    MovieAdxStreamResult finish(MovieAdxStreamFailure&) noexcept;
    std::optional<MovieAdxStreamSnapshot> snapshot(MovieAdxStreamFailure&) noexcept;
private:
    struct State {
        std::optional<MovieAdxHeader> header;
        std::optional<AdxBlockProfile> profile;
        std::vector<std::uint8_t> carry,marker;
        std::uint64_t decoded=0,published=0;
        bool ended=false;
    };
    struct Guard;
    explicit NativeMovieAdxStream(std::unique_ptr<MovieAdxProfileResolver>);
    bool initialize(std::span<const std::uint8_t>,MovieAdxStreamFailure&);
    bool admit_profile(State&,MovieAdxHeader,MovieAdxStreamFailure&);
    bool process(State&,MovieAdxStreamResult&,MovieAdxStreamFailure&);
    MovieAdxStreamResult terminal(MovieAdxStreamFailure&) noexcept;
    State state_;
    std::unique_ptr<MovieAdxProfileResolver> resolver_;
    const std::thread::id owner_=std::this_thread::get_id();
    bool busy_=false;
    MovieAdxStreamFailure terminal_;
};
} // namespace mka::video
