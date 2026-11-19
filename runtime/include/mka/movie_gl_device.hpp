#pragma once
#include "mka/movie_gpu.hpp"
namespace mka::video {
// Borrows an already-current host WGL context. Never creates, steals, detaches
// or deletes the host context/window. No GS or packet API is required.
class NativeMovieGlDevice:public NativeMovieGpuBackend {
public:
    static std::shared_ptr<NativeMovieGlDevice> attach_current_context(void* host_window,
        std::uint64_t destination,MovieGpuError&);
    // Must succeed on the creating thread with its original context current
    // before releasing the device. Failure revokes submissions but retains IDs
    // for an owner-thread retry. Does not swap or infer movie completion.
    virtual MovieGpuError shutdown()noexcept=0;
};
} // namespace mka::video
