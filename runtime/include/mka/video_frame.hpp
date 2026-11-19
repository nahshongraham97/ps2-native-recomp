#pragma once
#include <cstdint>

namespace mka::video {
// Native resource identity/dimensions only. No packet/register/address ABI.
class VideoFrameResource {
public:
    virtual ~VideoFrameResource()=default;
    virtual std::uint64_t identity()const noexcept=0;
    virtual std::uint32_t width()const noexcept=0;
    virtual std::uint32_t height()const noexcept=0;
};
} // namespace mka::video
