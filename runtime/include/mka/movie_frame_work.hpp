#pragma once
#include "mka/movie_output_position.hpp"
#include <map>
#include <set>

namespace mka::video {
enum class MovieFrameWorkProfile {frame_no_repeat_no_skip};
enum class MovieFrameWorkError {none,invalid_owner,wrong_thread,reentry,foreign_source,
    invalid_order,unsupported_profile,invalid_receipt,missing_output,overflow,allocation,feed,failed};
struct MovieFrameWorkFailure {
    MovieFrameWorkError kind=MovieFrameWorkError::none;
    const char* operation="";
    MovieAdmissionFeedFailure feed;
};
enum class MovieFrameWorkProgress {need_more,processed_without_output,output,flushed};
struct MovieFrameWorkResult {
    MovieFrameWorkProgress progress=MovieFrameWorkProgress::need_more;
    std::optional<MovieOutputPositionWork> work;
    // Pre-work outer admission is work.current_header. This separate snapshot
    // describes the picture actually processed inside that same outer call.
    std::shared_ptr<const MovieAdmissionCurrent> processing_header;
    std::shared_ptr<const MovieCodecInputReceipt> input;
    std::optional<MoviePictureSource> selected;
    std::optional<std::uint64_t> timing_admission_id;
    std::uint64_t consumed_begin=0,consumed_end=0;
};
struct MovieFrameWorkSnapshot {
    std::uint64_t observation_sequence=0,processed=0,published=0,last_work_id=0;
    std::size_t retained_inputs=0,retained_outputs=0;
    bool outer_active=false,waiting_output=false,program_end=false,flushed=false,failed=false;
};
// Concrete native normal frame processing, not an IPU/SDK buffer model. The
// borrowed feed has one parser/consumer writer while an outer operation runs.
// The service owns both objects and delivers actual native source observations.
class NativeMovieFrameWorkCoordinator final {
public:
    static std::unique_ptr<NativeMovieFrameWorkCoordinator> create(
        NativeMovieAdmissionFeed&,std::string_view,
        std::shared_ptr<const MovieSourceObservationIdentity>,MovieFrameWorkProfile,
        MovieFrameWorkFailure&) noexcept;
    ~NativeMovieFrameWorkCoordinator();
    NativeMovieFrameWorkCoordinator(const NativeMovieFrameWorkCoordinator&)=delete;
    NativeMovieFrameWorkCoordinator& operator=(const NativeMovieFrameWorkCoordinator&)=delete;
    bool observe_source(const MovieSourceObservationBatch&,MovieFrameWorkFailure&) noexcept;
    bool begin_outer(std::uint64_t admission_id,std::shared_ptr<const MovieAdmissionCurrent>,
        std::optional<std::uint64_t> timing_admission_id,MovieFrameWorkFailure&) noexcept;
    std::optional<MovieFrameWorkResult> next(MovieFrameWorkFailure&) noexcept;
    // Service supplies its actual owned Stream decoder_eof observation. EOF
    // alone is insufficient: parsed end/empty input and selected ownership join.
    std::optional<MovieFrameWorkResult> flush_after_decoder_end(bool actual_decoder_eof,
        MovieFrameWorkFailure&) noexcept;
    std::optional<MovieFrameWorkSnapshot> snapshot(MovieFrameWorkFailure&) const noexcept;
private:
    NativeMovieFrameWorkCoordinator(NativeMovieAdmissionFeed&,std::string,
        std::shared_ptr<const MovieSourceObservationIdentity>);
    bool access(MovieFrameWorkFailure&) const noexcept;
    bool fail(MovieFrameWorkFailure&,MovieFrameWorkError,const char*) const noexcept;
    std::optional<MovieFrameWorkResult> release_pending(MovieFrameWorkFailure&) noexcept;
    bool retire_control_inputs(const MovieAdmissionFeedSnapshot&,MovieFrameWorkFailure&) noexcept;
    NativeMovieAdmissionFeed& feed_;
    std::string key_;
    std::shared_ptr<const MovieSourceObservationIdentity> identity_;
    std::map<std::uint64_t,std::shared_ptr<const MovieCodecInputReceipt>> inputs_;
    std::map<std::uint64_t,MovieDecodedPictureObserved> decoded_;
    std::set<std::uint64_t> processed_pictures_,published_pictures_;
    std::shared_ptr<const MovieCodecInputReceipt> last_input_;
    std::shared_ptr<const MovieAdmissionCurrent> outer_;
    std::shared_ptr<const MovieAdmissionCurrent> last_processing_;
    std::optional<std::uint64_t> timing_;
    std::uint64_t outer_admission_=0;
    std::optional<MoviePictureSource> reference_,expected_;
    std::optional<MovieFrameWorkResult> pending_;
    MovieFrameWorkSnapshot state_;
    const std::thread::id owner_=std::this_thread::get_id();
    mutable bool busy_=false;
    bool inner_scope_=false;
};
} // namespace mka::video
