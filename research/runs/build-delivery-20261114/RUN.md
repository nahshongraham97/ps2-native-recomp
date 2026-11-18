# Build delivery — 2026-11-14

> Dated-run audit boundary (2026-11-18): All implementation/current/verified language here refers to the named historical source, build and scenario. Original counter values, outputs, observations and artifact IDs are preserved as reports, not independently revalidated facts. “Native” component names, successful builds/tests and old census verdicts do not establish host resource ownership, card/profile behavior, correct sound, physical input, movie A/V sync or whole-game acceptance. Read the canonical parent PORT_STATUS.md and execution contract for current blockers; unresolved historical causal/source claims remain unresolved.

The runner at `build_native_gl_full\ps2run.exe` (the binary `launch_game.bat`
executes) was rebuilt in place from the source at the named revision, so the launcher now
serves every commit on `codex/native-vfs-sif-overlay-20261110` instead of the
checkpoint-era binary that predated them.

- Binary: `external/forks/ps2-native-recomp/build_native_gl_full/ps2run.exe`
- Built: 2026-11-14 22:44, 74,858,496 bytes
- SHA-256: `9e15d6b5aa6b656e379e316e00eb35a2f1eba19f581622dad554faa2accfa43a`
- Commit range included: `fd8bbd7` (surface cache / depth sharing / transfer
  timing) .. `e55be4f` (combiner parity gate) .. `fe7111a` (SPU2 mixer
  acceptance) .. `bdc73f9` (playback-device selection) .. `dd0876d` (Phase C
  notes). Working tree clean at HEAD `dd0876d`.
- Verified markers inside the binary: `PS2_AUDIO_DEVICE` (the playback-device
  fix) is present; it was absent from the previous 16:52 checkpoint binary.

Test gate run against this build (AMD Radeon RX 6800 XT, GL 4.6):

| suite | result |
| --- | --- |
| `test_spu2_mix` | PASS (SdInit silence gate documented; chain peak 28667) |
| `test_gl_vs_raster` | ALL 18 MATRIX TESTS PASSED (incl. real font atlas) |
| `test_native_gl_alpha` | ALPHA=0x44 source-over matches the reference |
| `test_gl_lazy_readback` | PASS (10 tests) |

Not yet verified in this build: the user-visible blockers (intro movies,
character select, combat rendering) and audible sound — those need a run via
`launch_game.bat`, and the audio bisect via `launch_game_audio_diag.bat`
(chip.wav / device.wav / heard.wav + voice log in
`build_native_gl_full/diagnostics/audio-bisect-20261114/`).
The checkpoint-era binary remains archived in
`native-checkpoint-text-20261114/`; the separate `build_dev/` tree was used
only as a scratch build and can be deleted.
