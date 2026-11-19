# Native component source checkpoint — 2026-11-19

Acceptance remains OPEN. No game executable was launched or offered to the
owner. The preserved playable backup, cards, states, launchers and model
settings were not changed. This is a source/component checkpoint, not a
claim that movies, physical input, sound playback or all game modes work.

## Implemented

- Host-owned MS4 source groups, exact bank/program/resource ownership and full
  token protocol decoding; no Bank 0/name/low-bit fallback.
- Native normal count2 conversion and independent ordered-plane decoding.
  Owned buffers exceed former chip capacities; unknown history/variants fail.
- Source-selected eager/deferred bank preparation with transactional failure,
  resource lifetime, unload/revocation and allocation-error handling.
  Preparation does not fabricate playback or completion callbacks.
- CT32 IMAGE uploads and supported local copies terminate in GPU resources.
  Raw bytes/alpha remain intact. Indexed GPU views support PSMT8/4/8H/4HL/4HH
  and the tested CPSM32 palette/CSA subsets. Undefined pixels and unsupported
  aliases/formats reject explicitly instead of sampling CPU VRAM.
- GPU DISPLAY/capture uses the supplied descriptor, clears failed output,
  avoids stale captures/CPU fallback and does not apply legacy frame weaving.
- Host input is submitted atomically; native PADMAN input reads host-owned
  complete samples rather than SIO2 controller state. Full pressure survives
  the frame latch. Version 2 recordings retain all sixteen pressures; valid
  version 1 data remains readable. Replay holds cannot be erased by live host
  polling on frames without a recorded event. Malformed rows/versions and
  out-of-range values fail before narrowing.

## Verification

LLVM MinGW Clang toolchain `llvm-mingw-20261006-ucrt-x86_64`, CMake 4.4.5,
existing `build_native_gl_full`, two parallel compiler jobs. Standalone GL
fixtures use AMD Radeon RX 6800 XT OpenGL 4.6. No game boot is involved.

- `verification.json`: all 21 component cases pass. This includes bank
  ownership/protocol/preparation, input/concurrency, PADMAN normal plus two
  named strict rejections, ten recording cases, GPU upload/copy/palette and
  descriptor checks, the 18-case sampling matrix, alpha and lazy readback.
- Preparation checks all 344 actual banks: 5,500 eager programs, 4,156
  source-deferred programs, 179 banks skipping precreation under source policy.
- `normal-count2-verification.json`: 40 actual resources/80 encoded planes,
  138 chunks, exact retained source permutation hashes and independent
  software PCM/history/control-byte parity. This does not prove channel,
  loop, envelope, mixer or audible parity.
- `stream-groups-verification.json`: 1,189 actual MS4 source groups and their
  exact metadata/byte hashes match the independent extractor.
- `source-manifest.json` binds staged source blobs and their content SHA-256.
  Working-file hashes are concurrent observations only: the GPU delegate had
  begun the next CPSM16 batch after staging, so that working backend hash is
  not the tested checkpoint's source identity. The staged blob is authoritative.
  JSON records bind individual test executable hashes and arguments. Tests
  ran before this checkpoint commit; no clean accepted game build is claimed.

Failures were retained in the parent research records before correction.
The indexed pixel failure exposed scissor clipping during GPU blits; the
corrected helper disables/restores scissor, with a zero-area-scissor test.
The preparation fixture incorrectly expected announcr selector 0 to be eager;
original flags prove it deferred. Only that expectation was corrected.
Independent review caught pressure truncation, replay-hold and parser gaps;
the final integration and error cases cover their corrections.

## Remaining blockers

Native audio completion/control/mixing/output and live transport replacement
remain incomplete. Some texture/palette/alias/copy formats remain unsupported;
native GPU state persistence and whole-game rendering/movie parity are open.
The fixed guest memory ABI, remaining device-backed services, full AOT
coverage/math parity, cards/profiles, physical input and all-mode comparison
still block acceptance. Two preexisting dirty IOP files (`mcserv.cpp` and
`msl_native.cpp`) were preserved and excluded from this commit. Integration
fixtures linking ps2runtime do not establish removal of those legacy systems.

Owner: no action. Coding agent: continue the native bank completion bridge,
remaining palette formats and the other open native platform boundaries.
