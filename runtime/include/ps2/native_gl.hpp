#pragma once

namespace ps2::gs {
struct DisplayRegs;
}

namespace ps2::gs::native_gl {

// True when the native OpenGL packet renderer was requested by the environment.
bool selected();

// Adopt the SDL-owned current WGL context for direct-window GS presentation.
// The caller must create and make the SDL OpenGL context current first.
bool attach_current_context(void *host_window);

// True only when the backend owns a visible host-window OpenGL context.
bool direct_window();

// Draw the selected GS display rectangle from the native scene target into the
// visible window back buffer. The host remains responsible for swapping it.
bool present_frame(const DisplayRegs &regs);
bool present_current();

// Releases backend-owned GL objects without destroying an SDL-owned context.
void shutdown();

} // namespace ps2::gs::native_gl
