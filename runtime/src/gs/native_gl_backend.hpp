#pragma once

#include "gs.hpp"
#include "ps2/native_gl.hpp"

namespace ps2::gs::native_gl {

// True when native OpenGL is requested by the environment. This does not
// create the context, so it is safe to use while selecting the GS backend.
bool selected();

// True when the native OpenGL context was requested and initialized.
bool enabled();

// Used by a visible SDL OpenGL host after it has made its context current.
bool attach_current_context(void *host_window);
bool direct_window();
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

// Clears all native scene surfaces to black. Called at scene boundaries (e.g.
// SetGsCrt or video mode reconfiguration) to eliminate stale frame ghosts.
void clear_surfaces();

// Synchronizes a GPU scene surface back to CPU VRAM on demand if it was dirtied
// by GPU primitive rendering. Returns true if a sync occurred, false if clean or absent.
bool sync_surface_to_cpu(std::uint32_t fbp);

// Synchronizes all dirty GPU scene surfaces back to CPU VRAM.
void sync_all_surfaces_to_cpu();

// The active drawing buffer's FBP (frame buffer pointer).
std::uint32_t active_frame_fbp();

// The GPU's own busy time in milliseconds, measured with a GL timer query
// around the frame's commands. Zero when the driver has no timer query. This
// is what the window title's "GPU %" reads in the native GL path: the WebGPU
// counter it used before is always zero there, which is what made the title
// say "GPU 0%" while the game was drawing on the Radeon.
double gpu_busy_ms();

// Tears down the GL context. Safe to call multiple times.
void shutdown();

} // namespace ps2::gs::native_gl
