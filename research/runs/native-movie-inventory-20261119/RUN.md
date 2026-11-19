# All physical MOVIEPS2 decode inventory — 2026-11-19

The isolated native service fixture completed all38 SFDfiles with no decode
failure:1,516,830,720sourcebytes,65,181video frames and99,511,552audio sample
frames. Source/YUV/PCM SHA256, timestamps, metadata and exact asset keys are in
inventory.jsonl; invocation/binary hash/exit are in execution.json. The three
exact-name independent goldens match;35others are decode-support observations.
Timestamp goldens compare first/last/missing/timebase aggregates; complete
per-frame timestamp sequence fidelity is not asserted.

Observed video shapes are320x240 SAR1:1 and640x448 SAR14:15, with source color
metadata retained. Observed audio is48000Hz/stereo. Nominal video frame rate is
not exposed by the API and is not inferred from observed timestamp deltas.

This does not establish subtitle metadata handling, all retail movie call
reachability, role/aspect selection, audible playback or A/V synchronization.
Actual TITLEN carries CRITAGS/SUBTITLE metadata in private_stream_2 packets;
the codec-only decoder omits those tags. Native extraction/selection and
retail overlay/font binding remain release blockers. No game was launched.
