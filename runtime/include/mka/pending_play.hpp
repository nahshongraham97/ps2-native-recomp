#pragma once
#include "mka/playback_owner.hpp"
namespace mka::sound {
enum class NativePlayWorkState {queued,building,runtime_built,callback_complete,retired};
struct NativePlayWorkObservation {
    NativePlayWorkState state=NativePlayWorkState::queued;
    RuntimeProgramWorkBranch branch=RuntimeProgramWorkBranch::unselected;
    bool runtime_ready=false,ordinary_callback_invoked=false,activated=false;
    bool cancellation_requested=false,cancelled=false,pending=true;
    std::optional<PlaybackFailure> failure;
    bool direct_attempted=false,direct_retry_queued=false;
    std::optional<PlaybackFailure> direct_constructor_failure;
};
// One actual native association, runtime construction and ordinary consumer.
// Runtime readiness is distinct from private-copy/activation and voice start.
// A running request clears the pending indication but keeps actual ownership
// until callback/suppression and retirement. Cached work bypasses fresh cancel.
class NativePlayWork final {
public:
    ~NativePlayWork();
    NativePlayWork(const NativePlayWork&)=delete;
    NativePlayWork& operator=(const NativePlayWork&)=delete;
    NativePlayHandle handle() const;
    bool cached_runtime_available() const;
    bool begin_build(PlaybackFailure&);
    std::optional<bool> try_begin_direct(PlaybackFailure&);
    // EE8 queues only a failed fresh constructor, never a failed consumer.
    bool retry_direct_construction_on_worker(PlaybackFailure&);
    bool build(PlaybackFailure&);
    // On transient context exclusion the same owned plan is retried. Lower
    // construction leases retire before entering the actual native consumer.
    bool complete(bool native_output_admitted,PlaybackFailure&);
    bool retire(PlaybackFailure&);
    ProgramWorkCancellation cancel();
    NativePlayWorkObservation observe() const;
private:
    struct Impl;
    explicit NativePlayWork(std::shared_ptr<Impl> s):state_(std::move(s)){}
    std::shared_ptr<Impl> state_;
    friend class NativePlaybackOwner;
};
}
