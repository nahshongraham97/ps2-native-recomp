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
