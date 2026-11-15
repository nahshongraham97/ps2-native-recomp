# Native renderer follow-up, 2026-11-14

The user confirmed the text rendering checkpoint `efa189a` visually. The
changes described here are subsequent source changes; the checkpoint runner
was not replaced and the game was not launched for this follow-up.

## Changes and isolated verification

The native GL path accepts PSMCT32's full alpha-byte framebuffer mask,
source-identity ALPHA=0xA8 with FIX=0x80, and FB_ONLY alpha-test behavior
when depth writes are masked or depth testing is disabled. Unsupported
partial masks and other blend configurations remain rejected. A rejected
primitive no longer changes the active framebuffer or counts as a completed
draw for presentation selection.

Rebuilt and ran `test_native_gl_alpha.exe` and `test_gl_vs_raster.exe` from
`build_native_gl_full`; both passed, including 18 sampling matrix cases. These
tests use an AMD Radeon RX 6800 XT OpenGL context. They do not establish retail
movie playback, character selection, audio, or complete native-path compliance.

## Open investigation

In the user-run `games/mkarmageddon/disc/logs/ps2run_2026-11-14_16-57-24.log`,
the movie sequence opens LOGON.SFD, OPENINGN.SFD and TITLEN.SFD and presentation
switches among framebuffer IDs 0, 140 and 91. Selection compares only the
active and display surfaces. The log also shows frequent recycling of four scene
targets, including IDs 0, 91, 440, 448, 456, 464 and 496. Recycling clears a
surface; the interaction with pending GPU writes and presentation needs a
targeted test before changing eviction policy. These observations do not prove
the cause of black movie frames or missing character-select imagery.

The accepted no-emulation objective remains unmet: the recorded run used live
EE compilation and transitional PS2-oriented services. Audio remains unverified
and the user reported silence. Qwen's LM Studio server was unreachable on
127.0.0.1:1234 during this source review; model settings were not changed.

## Subsequent surface-cache review

`ensure_scene_target` cleared a recycled OpenGL target but retained the old
owner's primitive counts, last-write tick, synchronization flag and readback
state. Those fields are now reset when assigning a target to a new FBP, so an
empty target cannot inherit the former owner's presentation age. Rebuilt and
passed the alpha, sampling and lazy-readback tests; none directly proves the
recycled-target selection behavior.

A subsequent cache-eviction test drew a dirty surface at FBP 91 and filled the
four-target cache with other surfaces. Before the fix its sampled CPU pixel was
zero after eviction; after synchronizing dirty surfaces before reuse, the same
test passed with the expected RGB value. The alpha and sampling tests above do
not exercise this eviction case. Restoring an evicted surface back into a new
GPU target, overlapping framebuffer layouts, depth, and retail behavior still
need separate verification; the runner has not been rebuilt for this change.

A temporary follow-up test revisited FBP 91 after eviction, drew an unrelated
rectangle, and synchronized it: the original RGB pixel was overwritten with
black (`ff000000`). Thus preserving the CPU copy on eviction alone is not
enough. The current `read_rect_rgba` helper forces alpha to `0xff`, so it is
not a correct general-purpose framebuffer restoration path. The failing
experiment was removed from the normal test suite; restoration remains open.

An experimental PSMCT32 color restore now reads stored RGBA and uploads it to
the recreated surface before a draw. The opt-in `MKA_TEST_EVICT_RESTORE=1`
regression now passes, as do the alpha and 18-case sampling tests. This does not
restore depth, cover non-32-bit formats, establish overlapping-surface
coherency, or validate presentation/movie playback. Recreated non-32-bit
surfaces are rejected at this seam. No retail run or runner rebuild was made.

The opt-in test now checks non-opaque alpha (`0x80`) and confirms that a later
GEQUAL depth-tested draw is rejected after color restoration because depth was
not preserved. This guard avoids silently using cleared depth, but can cause
missing geometry and is not a replacement for implementing depth restoration.

Surface allocation, recycling and scene-boundary clears now temporarily disable
scissor and enable color/depth writes, then restore GL state. Previously a
preceding scissor or mask could leave parts of the supposedly cleared surface
unchanged. The lazy-readback, alpha, and sampling tests pass after this change.
A focused test now draws outside a small scissor, leaves that scissor active,
clears surfaces, and verifies that the outside pixel is black. The test does
not independently exercise color or depth write masks. Depth is allocated per cached
color surface while the depth key is global, so depth ownership across targets
must be resolved before treating the cache as a faithful depth implementation.

A scene reset previously retained the global depth-buffer key even though it
cleared every cached depth surface. An isolated test reproduced rejection of a
new depth key after reset; resetting the key with the surfaces makes it pass.
This does not implement sharing or restoration of depth between surfaces.

An opt-in regression (`MKA_TEST_SHARED_DEPTH=1`) draws to two different color
targets with the same depth key. It currently fails: the second target accepts
a pixel that should fail against the first target's depth, because each target
has its own GL depth renderbuffer. This test remains opt-in while the shared
depth implementation is outstanding. The experiment does not establish retail
gameplay impact.

The experimental renderer now shares one GL depth renderbuffer across its
color targets and does not clear that shared buffer when recycling a color
target. Both `MKA_TEST_SHARED_DEPTH=1` and `MKA_TEST_EVICT_RESTORE=1` pass in
the isolated test, as do the alpha and sampling tests. Only one depth key is
supported at a time; multiple depth buffers, GS memory aliasing, precision,
and real game behavior remain unverified. No game runner was rebuilt.

The first-use depth clear now disables any inherited scissor and restores the
previous depth/scissor state afterward. The shared-depth, color restoration,
alpha and sampling tests pass after that change. A dedicated scissored first-use
depth test is still needed; these tests do not establish overlapping-memory
coherency or depth values written through a different path.

An opt-in `MKA_TEST_ALIAS_OWNERSHIP=1` test confirms two differently based
32-bit coordinates address the same memory. The initial experiment incorrectly
expected the exact-base `surface_owner` API to infer an entire surface range
from a base address alone. Presentation does use that exact-base lookup, so
overlap remains an unresolved limitation, but a correct range-aware API needs
format, stride, extent, and write coverage before that behavior can be tested.
The dedicated scissored first-use depth test remains outstanding.

Guest-write history now records destination buffer width for both local copies
and host-to-local transfers when those events are recorded. The opt-in test
checks that this metadata survives recording. Presentation still uses exact-base
ownership; width alone does not implement overlap detection. Host-to-local
recording is conditional on `cpu_raster()`, which needs separate route analysis.

Packet-path source trace: `native_renderer::image` calls `gs::write_image`,
which forwards data to `feed_image`; when `cpu_raster()` is true, completed
transfers call `apply_display_write`. The write-history event is currently
recorded on TRXDIR setup, before image data is complete, and local-copy history
is recorded before the copy loop. Therefore the record indicates intent, not
confirmed completion; it must not alone establish that a full frame exists.
When WebGPU is primary, `feed_image` returns before the CPU transfer path. The
recorded run selected native GL, but this source trace is not a fresh run.

The CPU transfer path now records host-to-local writes after the last pixel and
local copies after the copy loop. `MKA_TEST_TRANSFER_COMPLETION=1` verifies that
a 4-pixel 32-bit transfer is absent from ownership history at setup and present
after its payload arrives. Partial, interrupted, overlapping and other-format
transfers need further coverage; this test does not verify movie presentation.

The transfer test additionally sends an eight-pixel 32-bit transfer in two
qwords: no completed-write owner is reported after the first qword, and the
owner and last pixel are present after the second. Interrupted transfers and
other formats remain unverified.

An additional 32-bit test interrupts a half-received transfer with a new one:
the interrupted destination does not gain a completed-write record, and the
replacement transfer starts at its first pixel and is recorded on completion.
The first transfer's already-written pixels are still present in memory; how
partial writes should affect presentation and overlapping ownership is not
covered by this completion-only test.

The completion test also passes for a six-pixel 24-bit transfer split across
two qwords, including a pixel whose bytes cross the qword boundary. Ownership
is recorded only after the final bytes in this case. This does not test how
the display should show partial transfers.

`MKA_TEST_TRANSFER_COHERENCY=1` reproduced an older GPU readback replacing a
newer CPU image-transfer pixel at the same framebuffer base. The host-to-local
setup now synchronizes a dirty exact-base surface first; that test passes. This
does not handle differently based aliases, local copies, partial GPU refresh,
or prove that the renderer displays the transferred pixel in a retail run.

`MKA_TEST_COPY_COHERENCY=1` checks GPU-dirty source and destination buffers for
local copies. Exact, aligned buffer bases are synchronized before the copy and
the non-overlapping test passes. An earlier test setup used overlapping bases
91 and 140: full-frame destination readback overwrote the source's memory.
That is a confirmed unresolved aliasing hazard, not fixed by exact-base sync.
The reproducible opt-in combination `MKA_TEST_COPY_COHERENCY=1` and
`MKA_TEST_COPY_OVERLAP=1` currently fails with `ff000000` in place of the
newer copied pixel. The same copy test using non-overlapping bases passes.

Source review confirms `do_readback_target` reads `FBW * 64` by 512 pixels and
writes every pixel back, while `SceneTarget` tracks only a dirty boolean and
the last draw's format state. It does not retain per-pixel coverage, masks, or
the sequence of writes. A single scissor or bounding rectangle would not be
sufficient to protect untouched pixels or overlapping later writes. Correcting
this requires finer-grained ownership/dirty coverage and ordering, including
format and framebuffer-mask changes. The current experimental full-surface
readback can corrupt aliased data and must not be treated as retail-validated.

The current renderer uses immediate-mode fixed-function drawing and a shared
`GL_DEPTH_COMPONENT24` renderbuffer; it does not attach a stencil buffer or
write a separate coverage attachment. Consequently, a correct GPU-produced
write mask is not already available in this implementation. Adding one must
preserve depth-test outcomes, alpha/mask behavior, per-surface ownership and
write ordering, and cannot be inferred from the submitted vertex bounds alone.

The shared depth attachment is reused across color targets, so a shared stencil
mask would also mix coverage from different targets. A correct coverage design
must retain independent ownership per color target while honoring the shared
depth result; replacing depth with a packed depth/stencil attachment alone is
insufficient. It also needs per-channel coverage for masked framebuffer writes
and invalidation when CPU transfers update overlapping addresses. No coverage
implementation has been added or validated by this source analysis.

One candidate design is a per-color-target stencil attachment, kept separate
from the shared depth attachment, that marks fragments which pass the draw's
tests. It must be checked for framebuffer completeness on the actual driver,
preserve per-channel write masks, and be cleared only after corresponding
writes are synchronized. CPU writes to aliases still need ordering/invalidation;
stencil coverage alone cannot solve that. This is a candidate, not an
implemented or verified feature.

A temporary isolated attachment probe on the AMD Radeon RX 6800 XT with GL
4.6 compatibility context attached a separate `STENCIL_INDEX8` renderbuffer
beside the existing `DEPTH_COMPONENT24` renderbuffer. Framebuffer status was
`0x8CDD` (`GL_FRAMEBUFFER_UNSUPPORTED`), so that candidate is not usable in
this tested configuration. The probe was removed after the test; packed
depth/stencil, coverage textures, or other approaches require separate tests.

A temporary probe attached a second RGBA texture as COLOR_ATTACHMENT1 beside
the existing shared depth renderbuffer. On the same AMD GL context the FBO
status was `0x8CD5` (`GL_FRAMEBUFFER_COMPLETE`) and the existing lazy-readback
test passed. The probe was removed afterward. This only demonstrates attachment
compatibility; it does not show simultaneous shader outputs, coverage accuracy,
per-channel masking, or ordering with overlapping CPU writes.

The existing draw uses fixed-function texture combiners (including scaled
modulation and multi-texture modes), not a fragment shader. A simultaneous
coverage output therefore requires reproducing the existing color and alpha
combiners in a shader or a separately validated alternative; framebuffer
completeness alone does not prove that such a change preserves image output.

The visible `test_gl_vs_raster` font cases exercise TFX=0 (MODULATE), while
`prepare_texture` also implements TFX=1 (DECAL) and TFX=2/3 (highlight modes)
with distinct alpha and second-texture-unit behavior. Before replacing this
path with a shader, parity coverage must include those cases, both TCC choices,
and the currently supported blend and framebuffer-mask combinations. Existing
font tests alone are insufficient to verify that migration.

`MKA_TEST_COVERAGE_HOLES=1` now reproduces the need for actual coverage: a
sentinel pixel lies inside a triangle's bounding box but outside its rasterized
area; full-surface readback changes it to black. The opt-in test currently
fails and must pass before a coverage-based readback can be claimed correct.

The movie log shows a host-to-local transfer with DBP 8960 while the observed
display FBP is 140 and the active FBP is 0. `apply_display_write` accepts only
exact base matches to the display or active surface. Whether this transfer
carries movie pixels and how it maps to display memory remain open; the log
entry alone cannot establish that it causes the black screen.

For the 16x448 transfer at DBP 8960, the difference from display FBP 140
(block base 4480) is 4480 blocks, or 1,146,880 bytes: numerically the size of a
640x448 32-bit frame. This is consistent with a following buffer, but does not
prove ownership, pixel contents, or that the transfer is a decoded movie frame.
The log reports other large uploads to non-display bases throughout frontend
operation, so size alone must not be used to classify every upload as video.

## Phase A split verification — 2026-11-14

Rebuilt from the working tree (`build_native_gl_full`, AMD Radeon RX 6800 XT
GL 4.6 context) and ran every suite before committing the safe subset.

Passing (committed as the safe subset):
- Default `test_gl_lazy_readback` (tests 1-10), `test_native_gl_alpha`, and
  `test_gl_vs_raster` (18/18 sampling cases).
- Opt-in `MKA_TEST_TRANSFER_COMPLETION`, `MKA_TEST_EVICT_RESTORE`,
  `MKA_TEST_SHARED_DEPTH`, `MKA_TEST_TRANSFER_COHERENCY`,
  `MKA_TEST_COPY_COHERENCY` (non-overlapping bases), and
  `MKA_TEST_ALIAS_OWNERSHIP` all print `[PASS]`.

Parked (opt-in, known-failing, not part of any default gate):
- `MKA_TEST_COPY_OVERLAP=1`: `[FAIL] local copy destination overwritten by GPU
  readback: ff000000`.
- `MKA_TEST_COVERAGE_HOLES=1`: `[FAIL] readback overwrote a pixel outside
  triangle coverage: ff000000`.
- `MKA_TEST_COMBINER_PARITY=1` (in `test_native_gl_alpha.cpp`): fails at
  `tfx=2 tcc=0 ref=405480B8 GL=8094C0F8`. The loop reaches tfx=2 only after
  tfx=0 (MODULATE) and tfx=1 (DECAL) pass all TCC combinations, so the DECAL
  `GL_RGB_SCALE=1` fix is validated and the HIGHLIGHT path (tfx=2/3) is a
  separate pre-existing gap. This test is the precursor to combiner-parity
  coverage; the tfx=2/3 mismatch is recorded here, not fixed in this commit.

These three failures are pre-existing renderer limitations, not regressions
introduced by the safe subset. They remain opt-in so the default branch stays
green. The full-surface readback behind the first two and the highlight
combiner behind the third are the open items.

## Phase B combiner parity matrix — 2026-11-14

Added an opt-in combiner parity matrix to `test_gl_vs_raster`
(`MKA_TEST_COMBINER_MATRIX=1`): TFX {0,1,2,3} x TCC {0,1} x blend
{none, ALPHA=0x44, ALPHA=0xA8/FIX=0x80}, each cell drawing a single
controlled texel (R=0xA0 G=0x80 B=0x50 A=0x40) under vertex colour
(r=96 g=64 b=32 a=64) through both renderers. Result: 10 pass, 8 fail,
6 rejected. The default 18-case sampling matrix stays green; the matrix is
opt-in so the known gaps do not turn the branch red.

Per-cell outcomes:
- TFX=0 (MODULATE): all 6 cells pass (both TCC, all three blends). This is
  the retail font/UI packet shape and is fully gated.
- TFX=1 (DECAL): no-blend and ALPHA=0xA8 pass (4 cells); ALPHA=0x44 is
  rejected by the `(!textured || tfx == 0)` guard on source-over, a
  fail-closed limitation.
- TFX=2/3 (HIGHLIGHT): all 8 drawable cells fail. Every RGB channel is
  off by exactly +64 (ref 184/128/84 vs GL 248/192/148). Root cause:
  the GL HIGHLIGHT path adds `GL_PRIMARY_COLOR`'s alpha to RGB, but
  `glColor` carries the x2-scaled vertex alpha (min(255, 64*2)=128), so it
  adds 128 where the software reference adds the raw 64. The output alpha is
  correct under the x2 storage convention (GL 128 vs ref 64); only the
  RGB addend is doubled.
- The 6 rejected cells are textured TFX!=0 with ALPHA=0x44, which the
  source-over guard rejects by design.

The TFX=2/3 fix is a runtime change (it needs the RGB addend to use the raw
vertex alpha while the output alpha/blend factor keep the x2 scaling, which
fixed-function combiners cannot express for an interpolated alpha without
either a shader or a per-draw constant). It is recorded here as the gate's
first finding; the gate is not clear for shader migration until TFX=2/3
match. No game was launched; both suites were run from `build_native_gl_full`
on the AMD Radeon RX 6800 XT GL 4.6 context.
