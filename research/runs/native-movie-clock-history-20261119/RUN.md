# Native movie clock history — component verification, 2026-11-19

Root isolated build and fixture PASS. The authoritative
[complete-trace-oracle.json](validation-01/complete-trace-oracle.json) records
365 exact Python-integer transitions, required windows 1/10/61/256, three atomic
overflow rejects, one moved-owner reject and six altered/shortened-trace negative
checks. [verification.json](validation-01/verification.json) retains the initial
replay and source/executable pins; the complete-checker receipt supersedes its
coverage-policy strength while preserving that earlier evidence. Build,
execution and scoped zero-forbidden-symbol audit records remain in
validation-01. Executable SHA256:
`9348d13c0d5440fe3072f767ff5f355d517f6d459966a72f1b9aac8e9e4f1f34`.

`symbol-target-recheck.json` separately serializes the executed target's exact
hash before and after the read-only symbol inspection, alongside the tool's
own hashes. Both target hashes match the executed binary. The original
symbols.json recorded tool hashes only; it remains historical evidence.

This is only owned residual-window history math: zero-filled full-window mean,
ring insertion, recenter and clear. Exact wide sums and dynamic native storage
remove the old signed32 sum/wrap and fixed physical-array limitations. Recenter
failure leaves window/cursor unchanged. Original outer cached means, anchors,
flags and statistics are outside this kernel and are not reset by clear.

Full clock/filter, actual endpoint consumption/output timing, movie player and
game acceptance remain OPEN. These receipts establish no speaker timing or
legacy-overflow parity. No game/device/driver/settings operation occurred in
this component verification. The implementation agent read saved receipts for
this update and did not rerun builds or tests.
