#pragma once

#include <cstddef>
#include <memory>

#include "gs.hpp"
#include "ps2/native_gl.hpp"
namespace mka::video {class VideoGpuBackend;class NativeMovieGpuBackend;}

namespace ps2::gs::native_gl {
// Acquires this current native context's video adapter session. The opaque
// destination is supplied by its host owner, not derived from a packet address.
// Null means unavailable/context mismatch; never creates another context.
std::shared_ptr<mka::video::VideoGpuBackend> video_backend(std::uint64_t destination);
// Native converted RGBA upload/presentation: no serialized GS resource keys,
// packets, scene-size limits or implicit movie lifecycle/scheduling.
std::shared_ptr<mka::video::NativeMovieGpuBackend> movie_backend(std::uint64_t destination);
// Effective CLD load only: caller retains the retail CLD skip/latch decision.
// Palette source must be a covered native raw CT32 resource; never CPU VRAM.
bool load_clut(std::uint64_t tex0, std::uint64_t texclut);

// Complete tightly packed host-to-local IMAGE data, before any CPU VRAM write.
// Supported: exact PSMCT32 identity, DBP in packet resource units, DBW in
// 64-pixel units, forward row order. True means uploaded GPU-owned pixels;
// later sampling still requires complete coverage of its exact TEX0 view.
// Existing exact CT32 framebuffer updates/views use GPU storage. Pending-to-new
// framebuffer promotion and indexed/format aliases are unsupported; no CPU fallback.
bool upload_image(std::uint32_t psm, std::uint32_t base, std::uint32_t bw,
                  std::uint32_t x, std::uint32_t y, std::uint32_t w, std::uint32_t h,
                  const std::uint8_t* packed, std::size_t bytes);

// Snapshot copy between exact owned CT32 resources. Direction is the decoded
// TRXPOS traversal order (DIRY bit0, DIRX bit1), NOT geometric mirroring.
// Same-resource overlap stages on the GPU. FBO-to-raw is unsupported because
// scaled framebuffer alpha cannot reconstruct the original IMAGE bytes.
bool copy_image(std::uint32_t source_psm, std::uint32_t source_base, std::uint32_t source_bw,
                std::uint32_t sx, std::uint32_t sy,
                std::uint32_t destination_psm, std::uint32_t destination_base, std::uint32_t destination_bw,
                std::uint32_t dx, std::uint32_t dy, std::uint32_t w, std::uint32_t h,
                std::uint32_t direction = 0);

// Native OpenGL selection is mandatory and independent of environment flags.
// This query does not create or validate a context.
bool selected();

// True only after the native OpenGL context/backend initialized successfully.
// Selection alone does not establish rendering or hardware-driver acceptance.
bool enabled();

// Used by a visible SDL OpenGL host after it has made its context current.
bool attach_current_context(void *host_window);
bool direct_window();
struct PresentationLayout {
    std::uint32_t scan_width=0,scan_height=0;
    // Logical canvas rectangle in this exact owned resource's coordinates.
    std::uint32_t x=0,y=0,width=0,height=0;
};
// Explicit native video/render-target configuration only, never inferred from
// a primitive, screen contents or another resource. Does not authorize an
// FBP/stride/format alias or establish the retail video adapter binding.
bool configure_presentation_layout(std::uint32_t fbp,std::uint32_t bw,
                                   std::uint32_t psm,const PresentationLayout& layout);
bool present_frame(const DisplayRegs &regs);
bool present_current();

// Attempts to draw the given primitive batch through the native OpenGL backend.
// Returns true when the batch was consumed by this backend (caller should not
// fall through to gs::gpu::draw). Returns false immediately when disabled.
//
// st carries the full GS raster state (frame pointer, texture base, alpha test,
// zbuf, scissor). verts is 3 vertices for prim 3/4/5 (triangle/strip/fan), 2 for
// line prims, 1 for points; count is the vertex count.
bool submit(const State &st, const Vertex *verts, int count);

// Copies a rectangle the guest wrote straight into VRAM (an image transfer or
// a local copy) onto the native scene surface, at the same coordinates the
// primitives use. This is how a buffer clear and a movie frame reach the
// picture the window shows: both arrive as transfers, not as primitives.
// Returns false when the backend is off or the pixel format is unsupported.
bool draw_guest_rect(std::uint32_t psm, std::uint32_t base, std::uint32_t bw,
                     std::uint32_t x, std::uint32_t y,
                     std::uint32_t width, std::uint32_t height);

// Flushes pending readback of the GL framebuffer into GS memory. Called from
// the backend itself on an internal cadence; exposed for host code that wants
// an explicit sync point.
void flush();

// Reads the picture the GS scans out, straight from the native surface: the
// same rectangle the window blits, taken from the same framebuffer. The
// native path never writes its pixels into the CPU VRAM array, so a host
// picture capture that reads that array shows the game's texture data instead
// of the game. Returns false when the native backend is off or no surface
// holds the picture. Runs on the thread that owns the GL context.
bool read_picture(std::vector<std::uint32_t> &rgba, std::uint32_t &width, std::uint32_t &height);
// Explicit descriptor form also captures an uploaded-only native DISPLAY source
// directly from its GPU storage. RGB matches that selected picture; raw source
// alpha is retained (window scanout alpha is not an encoded-data contract).
bool read_picture(const DisplayRegs&, std::vector<std::uint32_t>& rgba,
                  std::uint32_t& width, std::uint32_t& height);

// Clears all native scene surfaces to black. Called at scene boundaries (e.g.
// SetGsCrt or video mode reconfiguration) to eliminate stale frame ghosts.
void clear_surfaces();

// Synchronizes a GPU scene surface back to CPU VRAM on demand if it was dirtied
// by GPU primitive rendering. Returns true if a sync occurred, false if clean or absent.
bool sync_surface_to_cpu(std::uint32_t fbp);

// Synchronizes all dirty GPU scene surfaces back to CPU VRAM.
void sync_all_surfaces_to_cpu();

// A guest write into the game's VRAM. When the write belongs to one of the
// native textures (the game's own textures), the written rectangle is pushed
// into that texture: the data reaches a native GL resource instead of
// stopping in the emulated VRAM. Returns true when a native texture took it.
bool guest_wrote_vram(std::uint32_t psm, std::uint32_t base, std::uint32_t bw,
                      std::uint32_t x, std::uint32_t y, std::uint32_t w, std::uint32_t h);

// The active drawing buffer's FBP (frame buffer pointer).
std::uint32_t active_frame_fbp();

// Process-lifetime cumulative milliseconds from completed GL frame queries.
// Preserved across context reinitialization; getter only reads published state
// and never calls GL. Missing/pending timing does not manufacture a sample.
// Snapshot differences are measured command time, not full-device utilization
// or evidence that retained GS/CPU dependencies have been removed.
double gpu_busy_ms();

// Tears down the GL context. Safe to call multiple times.
void shutdown();

} // namespace ps2::gs::native_gl
