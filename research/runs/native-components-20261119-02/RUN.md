# Native components, second checkpoint — 2026-11-19

Source/component checkpoint only. Full game acceptance remains **OPEN**.
No game was launched and no owner launch is requested. The playable backup,
cards/states and local model/settings are preserved. Preexisting dirty
`runtime/src/iop/mcserv.cpp` and `msl_native.cpp` are excluded from this change.

The current standalone targets were rebuilt with the existing pinned
LLVM-MinGW/CMake toolchain. All **31 cases pass** in `verification.json`,
which records executable hashes, exact arguments, asset hashes and output.
Main/window objects also compile; this is not a new verified game executable.
`source-manifest.json` binds the staged source content, not a later worktree.

- Native bank service completion gates real program preparation; full wire
  tokens remain explicit and unknown identities fail by name.
- The normal-program executor starts real owned PCM through source-backed
  gain/pan controls and an explicitly supplied envelope/output rate. A start
  succeeds only after a HostVoice exists. Dynamic voices, transactional
  mixing, matching owner/kind7 stop, release and refused active unload pass.
  Actual envelope binding, streams/loops/effects, live output and fidelity
  remain open. The fixture's explicit envelope is not an inferred game profile.
- Native input/haptic ownership and serialized retail haptic data pass;
  physical hardware input/rumble and compiled retail ingress remain open.
- GPU textures/palettes/resources have no CPU-VRAM texture fallback or fixed
  16-scene eviction pool. Exact DISPLAY and covered region-clamp views pass.
  The pinned original final-resample packet renders 372,736 pixels with no
  RGB components outside its declared tolerance. Raw packet bytes, inherited
  state and full source/destination identities are used. 16S quantization,
  dither and live movie/frame fidelity remain open.
- Native video queue/environment/fence ordering is a standalone data core,
  not an interrupt/device implementation. Its retail compiler binding and
  GPU adapter remain open. Full832-pixel resampled content must not be cropped
  as if it contained192 blank columns; logical framing is a separate input.
- Authored damage scaling, position changes and movie skips are removed.
  Read-only observers call each original compiled retail body once. Full
  CPU/memory snapshots pass for input, health and all33 movie IDs. Six overlay
  leafs match checked original effects; the invented NETR BSS hook is gone.
  Old EE ABI dependencies remain and are not native-memory acceptance.

Primary source and review evidence is maintained in the parent repository's
`research/runs/native-{audio-controls,playback-owner,overlay-leaf-review,retail-hooks,render-final}-20261119/`
records. Root independently verifies all500 control-table bytes and original
overlay words against the original assets. Local model requests are absent
from this batch because `lms ps --json` returned `[]`; settings are unchanged.

Owner: no action. Coding agent: bind these verified components to native retail
platform ingress, remove the remaining memory/device implementations, and
complete exact all-mode/media/input/save comparisons before acceptance.
