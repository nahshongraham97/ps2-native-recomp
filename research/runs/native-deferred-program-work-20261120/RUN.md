# Native deferred program work/cache ownership — 2026-11-20

Status: PASS_NATIVE_DEFERRED_PROGRAM_WORK_AND_CORRECTED_SAMPLE_CACHE.
Build base3c09588d3777d465285d3e78dd118206d4515660 plus exact361-file map in
validation-01/verification.json; Release native component build, PS2_GENERATED
empty. This is not an accepted retail executable or full-game run.

Corrected original flag40 polarity: PRESENT suppresses cache-reference decrement;
ABSENT decrements. Cache association now has its own ordinary native Record and
per-occurrence owner, separate from immutable PCM lifetime. Source preserve pin
and zero-instance precreation transfer follow the corrected policy. No PS2 count
capacity or address/register model was introduced. Prior false review claims are
preserved as DISPROVED in parent's NATIVE_DEFERRED_PROGRAM_CURRENT_POLICY_20261120.

Actual PreparedProgramWork admits exact bank+backing, builds on worker thread,
retains real resource work through decision and explicit retirement, and supports
queued removal/active request/ready/private result/publication/retirement states.
Cancellation closes atomically after actual construction. Publication checks and
commits against unload under one lock cut. Private result cleanup precedes work
release; ticket refs drop before retired observation. Existing synchronous fresh
clients use this real primitive. No source callback or async Service success stub.

All12 affected executables rebuilt/executed PASS,361 stable current pins,
346 untouched prior. Existing bank policy check passed344 banks/5500eager/
4156deferred/179skipprecreation after actual core ownership changes. New Work
fixture has5 actual ANNOUNCR case groups; policy fixtures test ordered repeated
instances, cache-vs-PCM, failure/abandonment, and inherited sticky policy.
Existing service/program/playback/publication/exception and gap clients pass.
Only designated gap/exception tests have linker wraps. Decoder stays frozen;
no movie decoder/unchanged numeric corpus rerun, game, endpoint, model/server,
driver or user-data action occurred. Saved independent reviews reran nothing.

Limits: fresh native DATA work is not complete runtime descriptor/queue/callback
integration. Service async admission lease, source bank counter/callback order,
global public handles/command12, cached-program branch and hardware/full-game
fidelity remain OPEN. Sticky fixture includes a bank-owned eager program; ticket
retirement is not proof of physical final file-close completion.

Owner: no action. Coding agent: connect actual async Service admission and source
job/callback retirement using this corrected work and resource policy.

Publication wrapper initially rejected its path filter before staging/commit:
approved source files are outside the run folder. Exact script/failure record
are preserved; the filter now uses the fork root. Native source, binaries and
all12 passing execution records are unchanged; no test rerun is justified.

A second publication check rejected the raw Windows CRLF bank-policy TSV.
Its exact bytes are preserved; only that generated measurement receives a
scoped raw-evidence attribute. The failed attempt committed nothing and its
owned staged paths were restored before retry. Native source, binaries and
12 passing tests are unchanged; no test or decoder corpus rerun occurred.
