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

// Flushes pending readback of the GL framebuffer into GS memory. Called from
// the backend itself on an internal cadence; exposed for host code that wants
// an explicit sync point.
void flush();

// Tears down the GL context. Safe to call multiple times.
void shutdown();

} // namespace ps2::gs::native_gl
