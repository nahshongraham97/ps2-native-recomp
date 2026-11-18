# Retail graphics source guide

Use the canonical parent [rendering-path source audit](../../../../docs/NATIVE_GRAPHICS_ARCHITECTURE.md). It is the single architecture account; this fork does not maintain a competing production-backend snapshot.

The source-defined direct presentation route uses `host/window.cpp` and `runtime/src/gs/native_gl_backend.cpp` when direct OpenGL is selected. This does not establish complete GPU-owned resource handling, correct frames, or current acceptance. The former claims of a direct production D3D12 renderer, mandatory GL 4.6, all 490 VU programs covered, and universally zero stopped-VRAM transfers were unsupported and are withdrawn. The parent audit explains the alternative WGPU/D3D12 path and remaining GS/VRAM coupling.

Current evidence: [PORT_STATUS.md](../../../../PORT_STATUS.md). Work order: [ROADMAP.md](../../../../ROADMAP.md). Binding acceptance: [execution contract](../../../../NATIVE_PORT_EXECUTION_CONTRACT.md). Graphics and whole-game acceptance remain OPEN until their matching source/behavior evidence satisfies that contract.
