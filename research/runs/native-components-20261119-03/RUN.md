# Native movie and GPU components — 2026-11-19

Source/component checkpoint only. Acceptance remains **OPEN**. No game was
launched; no owner launch is requested. Backup, cards/states and local model
settings are preserved. Two preexisting IOP edits remain excluded.

All38 consolidated standalone cases pass in `verification.json`, with exact
test executable/argument/asset/library hashes. `source-manifest.json` binds
staged source. Tests predate the checkpoint commit; no clean accepted game
executable or full retail route is claimed.

- NativeFile64/customAVIO -> native MPEG2/ADX -> owned YUV/PCM decodes the
  complete logo/title/opening files. All output SHA256/counts and raw/best
  timestamp summaries match a separately written documented-default decoder.
  The strict decoder's terminal sequence-end packet boundary is repaired by
  preserving/splitting the4-byte marker; strict/error guards remain enabled.
- Native original-table conversion checks SHA256 for the selected one of two
  actual LUTs. Swapped/arbitrary/truncated tables fail. Both tables match the
  independent native SIMD instruction reference for all16,777,216 YUV triples.
  Owned linear pixels retain timing/SAR/source color provenance. Actual caller
  selection and live movie fidelity remain open.
- Explicit GPU snapshots/GLsync lifetime, logical/sample rectangles, context
  revocation/thread errors pass. Captured16S/DTHE1 effects remain rejected by
  name; rgba8/COLCLAMP1/DTHE0 execution alone is supported by this adapter.
- Three unsupported transfer modes stop before legacy GPU/CPU-VRAM/readback
  branches. Missing formats remain blockers; they were not made into no-ops.
- Prior bank/voice/input/haptics, captured final-packet, sampling/blend/display,
  observer and lifetime regressions remain green.

The native movie decoder is currently a standalone target using pinned
FFmpeg7.1.4 public headers and the existing native DLLs measured by the test.
SDK/library source, license, version and hashes must accompany eventual build
integration/packaging. These components have not replaced the live movie,
audio, fixed-memory or device route. Native movie GPU upload/presentation,
audio output/sync, file/lifecycle binding, full AOT/math coverage, cards,
physical controls, all modes and full regression acceptance remain open.

Parent records: `research/runs/native-movie-{decode,ingress}-20261119/`,
`native-render-final-20261119/VIDEO_GPU_ADAPTER.*`, and
`native-transfer-guard-20261119/SOURCE.md`. Historical failure evidence is
retained; the first adapter runner mistake passed an extra state argument,
then the corrected invocation passed without changing production expectations.

Owner: no action. Coding agent: connect native movie frames directly to native
GPU/audio services and preserve the remaining exact lifecycle/source contracts.
