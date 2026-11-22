#pragma once
#include "mka/video_frame.hpp"
#include <memory>
#include <optional>
#include <span>
namespace mka::graphics {
enum class NativeTextureError {none,unavailable,invalid_image,invalid_sampler,wrong_thread,stale_resource,host_capacity,allocation,upload_failed,sampler_failed,completion_failed,admission_closed};
struct NativeTextureFailure {NativeTextureError kind{NativeTextureError::none};const char* operation{""};};
enum class NativeTextureFilter {unspecified,nearest,linear};
enum class NativeTextureAddress {unspecified,repeat,clamp_to_edge};
struct NativeTextureSampler {NativeTextureFilter filter{NativeTextureFilter::unspecified};NativeTextureAddress u{NativeTextureAddress::unspecified},v{NativeTextureAddress::unspecified};};
struct NativeTextureRgbaImage {std::uint32_t width{},height{};std::span<const std::uint8_t> bytes;};
// Actual GL-created image ownership. The returned native name is borrowed
// while this capability and its owning current context remain live; never a
// serialized/game numeric handle. No movie/alpha conversion metadata exists.
class NativeGlTexture:public video::VideoFrameResource {
public:
 virtual std::optional<std::uint32_t> gl_object(NativeTextureFailure&)const noexcept=0;
};
struct NativeTextureUpload {NativeTextureFailure failure;std::shared_ptr<const NativeGlTexture> texture;};
class NativeGlTextureUpload {
public:
 virtual ~NativeGlTextureUpload()=default;
 // Copies exact storedRGBA8 bytes, including raw alpha. No normalization,
 // display/presentation, source policy or implicit sampler is supplied.
 virtual NativeTextureUpload upload_rgba_image(const NativeTextureRgbaImage&,NativeTextureSampler)=0;
 // Invalidates retained capabilities and deletes actual native storage; no source refcount model.
 // An unknown deletion outcome seals the owning Session. No automatic name
 // deletion retry or Session resume establishes host-context recovery.
 virtual bool revoke_image(const std::shared_ptr<const NativeGlTexture>&,NativeTextureFailure&)=0;
 virtual bool set_image_sampler(const std::shared_ptr<const NativeGlTexture>&,NativeTextureSampler,NativeTextureFailure&)=0;
};
}
