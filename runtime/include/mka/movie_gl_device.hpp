#pragma once
#include "mka/movie_gpu_completion.hpp"
#include "mka/native_gl_texture.hpp"
namespace mka::video {
// Borrows an already-current host WGL context. Never creates, steals, detaches
// or deletes it. The explicit host retirement owner must outlive devices and
// drain retained sessions before host/context teardown.
class NativeMovieGlDevice:public NativeMovieGpuBackend,public graphics::NativeGlTextureUpload {
public:
    static std::shared_ptr<NativeMovieGlDevice> attach_current_context(void* host_window,
        std::uint64_t destination, const std::shared_ptr<NativeMovieGpuRetirementOwner>&,
        MovieGpuError&);
    // Seals THIS device's image/movie upload, sampler and present admission
    // and inserts a real native
    // context-tail fence. It does not close ordinary game rendering admission.
    virtual MovieGpuDrain begin_context_drain() noexcept = 0;
    virtual MovieFenceResult poll_context_drain(const std::shared_ptr<MovieGpuDrainReceipt>&) noexcept = 0;
    virtual MovieGpuDrain retry_context_drain(const std::shared_ptr<MovieGpuDrainReceipt>&) noexcept = 0;
    virtual MovieGpuError resume_after_drain(const std::shared_ptr<MovieGpuDrainReceipt>&) noexcept = 0;
    // Revokes submissions; pending/wrong-thread/error retains actual IDs and
    // resources. Call/pump on the original current context until none. Backend
    // destruction marks its pre-registered host retirement task; never completion.
    virtual MovieGpuError shutdown() noexcept = 0;
};
} // namespace mka::video
