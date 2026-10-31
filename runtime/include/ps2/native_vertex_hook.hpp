#pragma once

#include <cstdint>

namespace ps2::gs {
struct State;
struct Vertex;
}

namespace ps2::vertex {

// Native Vertex Buffer Hook (Phase 2 Master Plan)
// Decouples 3D mesh rendering from per-triangle synchronous GPU compute dispatches.
// Batches identical-state primitives into consolidated submission buffers to eliminate
// the CPU-side rendering queue bottleneck on dual-core hardware.

bool is_enabled();
void set_enabled(bool enabled);

// Hook called by gs::kick(draw)
// Returns true if the primitive was batched into the native vertex buffer.
// Returns false if the buffer was flushed and standard dispatch should handle it.
bool submit_primitive(const gs::State &st, const gs::Vertex *v, int count);

// Flush all queued primitives to the GPU
void flush();

// Telemetry & profiling counters
std::uint64_t batched_vertices();
std::uint64_t draw_calls_saved();

} // namespace ps2::vertex
