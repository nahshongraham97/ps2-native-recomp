# Agent instructions — retail Armageddon native port

Read these in order before work: [PORT_STATUS.md](PORT_STATUS.md) for the
audited state, [NATIVE_PORT_EXECUTION_CONTRACT.md](NATIVE_PORT_EXECUTION_CONTRACT.md)
for binding requirements, [ROADMAP.md](ROADMAP.md) for exact implementation
steps, then the task's cited source/run record. The former AGENTS.md is
preserved at `research/history/native-port-docs-2026-11-18/AGENTS.md` and
does not supply current status or task order.

## Target and non-regression rule

- Work on the **retail game** in `external/forks/ps2-native-recomp/`, not the
  authored app or `milestone_build_kombat_practice/` package.
- Correct native behavior is the default. The accepted game needs zero
  command-line arguments, native-enabling environment variables or injected
  sampling fixes. Optional diagnostics are supplemental; missing native
  implementations fail by name. Preserve dedicated historical launchers.
- No guest instruction/microcode interpreter, emulated PS2 device, fixed
  guest address space, or fallback may remain on the accepted route. The
  compiled retail logic may remain; platform operations terminate in native
  game/Windows services. The contract defines each term exactly.
- No part of the game may regress. Correct card/profile behavior, exact sound
  identity, real keyboard/controller input, movies, rendering, combat, saves,
  and every mode are required on one clean build. If comparison evidence is
  missing, the status is **open**, never a pass.
- Never tell the owner to launch while any acceptance blocker remains. A
  zero census is diagnostic, not full-game acceptance. Do not claim a feature
  fixed from source presence, a unit test, scripted `--press`, or an old log.

## ELF authority, implementation and verification order

The hash-pinned original retail ELF and referenced original data are the
authority. Installing `ghidra-emotionengine-reloaded` does not make decompiled
C byte-exact or establish its ABI/types. For **every ported function**, follow
these five steps in order and record the address/range, input identity,
supporting evidence and unresolved behavior in the current work record. Reuse
valid existing evidence; keep one compact receipt instead of redundant reviews.

1. **Extract original words and data.** Map the function VMA through its actual
   ELF `PT_LOAD` segment: file offset = `p_offset + (VMA - p_vaddr)`, within
   that segment's file-backed range. Record exact little-endian instruction
   words, constants, strings and referenced table bytes from the pinned image.
   Distinguish file-backed bytes from loader zero-initialized storage. For an
   overlay or other image, establish that image's own mapping and identity;
   never reuse the main ELF's offsets by assumption.
2. **Decode and verify EE instructions.** Confirm the intended program and
   EE processor/language, record the processor-extension version when
   available, and join the listing's words to step 1. Compare listing coverage
   with the full original function span. Decode every omitted word and prove
   its control-flow role; omitted words may be real delay-slot instructions,
   not padding. A joined partial listing does not prove full-body coverage.
   Establish semantics
   from the applicable EE/R5900 instruction reference, including operand and
   result widths, sign/zero extension, signed/unsigned comparisons, wrapping
   versus trapping, branch targets, delay slots and branch-likely annulment.
   Verify argument rewrites in call delay slots and the values actually used
   by the callee. Prove both a branch's comparison width and the width/extension
   produced by preceding instructions; do not infer either from a decompiler
   variable. Check immediate extension separately from loaded-value extension
   (`LH` versus `LHU`), and never interpret the `U` in `ADDIU` as proof of an
   unsigned immediate. Resolve MMI operations by their actual lane widths,
   ordering and semantics, not opaque decompiler helper names. SLEIGH supplies
   both disassembly and p-code semantics; deterministic decoding alone does
   not prove that the processor specification is correct. Resolve discrepancies
   against the original words and instruction reference before coding. See
   the [Ghidra SLEIGH reference](https://ghidra.re/ghidra_docs/languages/html/sleigh.html).
3. **Use decompiled C only for orientation.** Follow broad data/control flow,
   then establish types, ABI, return values, aliases, shared state, effect
   order, errors and lifetime from the verified instructions AND callers and
   consumers. Names/comments, including earlier agent-authored wrapper names,
   are hypotheses, not recovered behavior. The former graphics-stage label
   for `0x002E06E0` must not override the verified file-descriptor behavior.
   A decompiler expression or suggested signature cannot settle signedness,
   a field meaning, an absent service, a callback or a successful empty case.
   For a virtual call, prove the actual constructor, concrete vtable and slot
   target. Objects sharing a registry need not share cleanup semantics; a
   helper named after another class is not proof that class was constructed.
4. **Implement the verified behavior in native C++.** Use typed host-owned
   objects/pointers or verified native handles and native platform services.
   Preserve required arithmetic widths, signedness, defined wrapping, dataflow,
   ordering, errors and lifetime explicitly; avoid C++ signed-overflow
   assumptions. Preserve retail game behavior without retaining guest address,
   register, MMIO or fixed PS2 resource constraints. Native code is not expected
   to produce byte-identical machine instructions. Unknown behavior remains
   a named implementation/link blocker, never a guessed mapping or success stub.
5. **Verify independently derived behavior.** Derive expected inputs, outputs,
   state transitions and edge cases from the verified instructions/data, not
   by copying the new implementation into its test. Exercise the affected
   normal, error and lifetime paths; include relevant negative/zero/positive,
   overflow, sign-extension, branch/delay-slot and SIMD-lane cases. Compile
   the connected callers and compare the same scenario with the preserved
   playable oracle when that route is available. A word fixture proves byte
   identity; a component test proves only its stated behavioral scope. Neither
   proves complete equivalence or full-game acceptance. Record remaining gaps
   explicitly and do not label them fixed.

### Required per-function evidence and claim gate

Keep the following in the function's existing source/implementation record;
link valid prior evidence rather than producing another competing report:

- **Original input:** ELF/overlay identity and SHA-256, function and referenced
  data ranges, actual segment/file mapping, and saved original words/data.
- **Actual decoding used:** program identity, EE language/processor and
  extension identity when available, the listing inspected, and its comparison
  with the original words. An installed plugin is not evidence that its EE
  listing was used. Do not claim that inspection without its saved evidence.
- **Behavior established:** instructions and callers/consumers proving the ABI,
  arithmetic and comparison widths, extension, delay-slot effects, MMI lanes,
  aliases, effect order, return/error behavior and lifetime. Mark every
  unresolved point explicitly; agreement between agents is not source proof.
- **Native implementation:** exact source paths and the host owners/services
  replacing the verified platform operations. Do not carry guest addresses,
  registers or hardware constraints into the native runtime to avoid proving
  a mapping. Historical addresses may identify source evidence.
- **Independent expected behavior:** cite the instructions/data or matching
  oracle scenario behind each relevant expected result, including edge cases.
  Record observed differences and their correction; preserve failed results.
- **Verification scope:** identify source-derived, implemented/unbuilt,
  compiled, tested-component, connected-run or accepted-gameplay status
  separately. For executed checks, record exact source/build/binary identity,
  commands, result and evidence paths. Missing checks remain OPEN. Neither
  matching words nor passing component checks closes the whole-game gate.

Do not promote a function beyond the evidence recorded for that stage. A
disputed instruction, type, field, callback or service contract must be resolved
from the authority before claiming the affected behavior implemented. Unknown
required production behavior remains a named blocker, never a guessed success.

Implementation and verification order remains binding:

- Follow the connected implementation order: retail entry -> native
  initialization -> game loop -> real input -> rendering and sound. Implement
  actual production effects and callers. Do not replace progress with report
  generation, isolated wrappers, optional cleanup, unrelated features or
  repeated verification of unchanged components. Defer work that can wait for
  the connected full build. A concrete failure/dependency can justify a
  directly needed source check or test; document which one it resolves.
- Compile the current connected changes and run the directly affected
  normal/error/lifetime checks. Broaden or repeat only for a source change,
  failure or unresolved material concern. Retain valid prior evidence for
  unchanged components. Preserve failures and prior measured bytes; never
  weaken a check or silently repin a changed implementation to manufacture a
  pass. Same-scenario oracle comparison and full-build acceptance remain
  separate required gates. Do not ask the owner to launch an incomplete build.
- Every progress claim must identify its evidence level: source-derived,
  implemented/unbuilt, compiled, tested component, connected run, or accepted
  gameplay. Missing evidence remains OPEN. Counts of hashes, reports or passed
  synthetic clients are not substitutes for getting the retail game working.

## Cross-version implementation donors

Keep the current native Windows architecture and verified implementation.
Other retail Armageddon versions may supply individual algorithms, asset
representations or platform-independent implementations when they resolve a
current connected-path blocker with less work. This is subsystem-level reuse,
not a restart or a new engine. The PS2 playable oracle remains the behavioral
reference unless the owner explicitly approves a different behavior.

Before adoption, pin the donor binary/data and platform, prove its own image
mapping, identify the equivalent PS2 caller/consumer and compare inputs,
outputs, numeric behavior, callbacks, side-effect order, lifetime, errors and
asset identity. Record differences and the transformation into our existing
typed native interfaces. Never mix addresses, structure layouts, pointer or
handle values between versions. A donor SDK/device model, guest memory or
interpreter cannot enter the accepted route. An easier-looking decompilation
or similar function name is insufficient evidence. Unmatched behavior remains
open; do not discard verified work or change gameplay to fit a donor.

## Checkout and evidence procedure

1. Verify absolute checkout root, branch, HEAD, and dirty paths for both
   parent and fork before editing. Preserve all existing changes. The fork
   had uncommitted `mcserv.cpp` and `msl_native.cpp` edits at the 2026-11-18
   audit; do not overwrite or silently include them in another change.
2. Preserve `external/forks/ps2-native-recomp-backup-20261110/`, user cards,
   saves, states, key files, and local model/settings. Use disposable copies
   for comparisons. Do not stage unrelated fighter/stage assets.
   Never install, remove, update, roll back, or reconfigure GPU drivers or
   their update settings. Use standard graphics APIs and queried host
   capabilities; no vendor-specific driver modification is part of this port.
3. For each boundary, cite the actual retail caller, bytes/fields, normal
   and error behavior, and reference run. Implement the native destination;
   unknown mappings fail by name. Do not invent bank IDs, return success
   stubs, skip packets, or weaken a checker to pass.
   For R5900 evidence, calculate signed16-bit immediates from the original
   LE32 word and verify the resulting mapped address/data bytes. Check branch
   subopcodes, delay slots and register rewrites across calls before naming
   a condition or returned status. Check floating argument registers such as
   f12/f14 and argument rewrites in the call delay slot; a decompiler's mixed
   float/integer signature is not ABI proof. Task continuation/yield and
   native main reentry are different control transfers. Raw and caller-normalized status are
   distinct values. Do not repair a disproved review hypothesis with a
   workaround: verify the actual declaration/ownership and withdraw it.
4. Build and run only with a provenance record. Compare the exact same
   scenario against the behavioral oracle; record clean commit, binary hash,
   flags, assets, log, frames, audio, input, exit, and source-path audit in
   `research/runs/<candidate-id>/`. The owner is not the test rig.
5. Update [PORT_STATUS.md](PORT_STATUS.md) for live state, [ROADMAP.md](ROADMAP.md)
   for task order, and a dated run record for new evidence. Keep historical
   measurements intact. Commit and push authored changes to `origin/main`
   in the appropriate repository; do not commit preexisting dirty source.
   Run staging validation, commit and push in one failure-propagating process.
   A failed check must prevent the commit/push. Compare staged paths with the
   approved changed paths; an approved but unchanged file need not be staged.
   Before consuming a review/checker report, inspect its actual return schema
   and a saved representative result. Declare exact status labels, pin-map
   keys, categorized negative plans and receipt/target identity fields per
   producer; never infer their types or names from a task description. A schema
   preflight must expose actual field types; do not flatten lists into counts
   and then compare them as integers. Compare exact target-name lists and
   their lengths separately. Use
   a read-only preflight for existing producer records before an expensive
   build/corpus run. A reporting-wrapper failure must remain separate from
   passed component/checker results; preserve both outcomes and prior bytes.

A legitimate native game control transfer is not a native failure. Propagate
NativeMainReentry through real task/body/cleanup callouts and verify that the
next source frame can still look up or publish tasks. Do not poison the owner
with an error-only latch for this transfer. Record the actual source effect
order, retain required native ownership, and avoid replaying cleanup effects;
ordinary after-effect failure remains a named blocker. Do not clear source
current-task state on unwinding unless the original control path does so.

Text rewrites must use explicit normalized LF bytes or `newline="\n"`.
Do not decode raw CRLF bytes and then use Windows default text-newline output;
that creates doubled CR and changes the frozen source identity. Check scoped
`git diff --check` and new-file line endings before freezing a handoff or
building, as well as before commit. Preserve exact prior bytes when repairing
measured source, and rerun the affected build/checks with new provenance.
Use distinctive JSON-helper names such as `json_string`; a helper named
`quoted` with string arguments can select `std::quoted` through ADL. Verify
actual included declarations before accepting an emitter or ownership claim.

## Documentation must change with the work

After every code/configuration change, test result, verified source finding,
implementation decision, blocker discovery, or rollback, update the affected
active documents **before starting the next implementation step**. Record
what changed, exact source/build identity, evidence, verification scope, and
what is still open. Include documentation in the same authored change batch.

`PORT_STATUS.md` owns live state, `ROADMAP.md` owns ordered implementation,
`NATIVE_PORT_EXECUTION_CONTRACT.md` owns acceptance, and dated run records own
observations. Other guides must link to these owners instead of maintaining
independent competing snapshots. Check all affected references for conflicts.
Correct contradictory live text in place; preserve old measurements as
historical evidence with a superseding label. Do not fix a conflict by
adding a new claim above an unchanged contradictory current section.

Affected-document maintenance applies in every subfolder, including generated
reports and nested guides. The preserved broad inventory covers the entire
`Armageddon-backup/` workspace, including sibling `.qwen/`, `.tools/`,
`dependencies/` and `forks/` trees, not only this Git repository.
`research/doc-audit-20261118/inventory.json` tracks that dated file-hash and
review-disposition snapshot. Donor repositories, original
input/reference manuals (including `private-inputs/**/.agents/`), preserved
backup documents, and before-correction snapshots are evidence, not current
project instructions. Preserve their original bytes; record any contradiction
or unverified technical claim in the audit instead of adopting it as fact.
Read `external/README.md` and the audit reference-scope record before using
a donor's build instructions or its definition of native.

The owner ended broad documentation cleanup to prioritize implementation.
Maintain the affected current documents and dated evidence after changes;
do not resume workspace-wide scanning/classification or rewrite the broad
inventory unless the owner asks. The coverage tool's `--check-coverage` also
rewrites its inventory, so it is not a read-only per-change check. Preserve
the committed broad inventory and label existing coverage measurements dated
and OPEN. Missing/pending or stale hash-bound reviews remain unresolved. An
explicit unresolved reference disposition is not factual verification and
does not pass the separate game acceptance contract.
Never report disposition coverage as a completed accuracy review of every
document. `research/doc-audit-20261118/ACCURACY_FOLLOWUP.md` records that
distinction and concrete follow-up findings. Before adopting a claim, record
its exact bytes/context, supporting or contrary source/run evidence, verdict,
and remaining limits. Unverified technical reference bodies remain open even
when their authority and file hashes have been catalogued.

## Authorized verification surfaces

The owner authorized headless game verification on2026-11-21. Confirm the
chosen route creates no visible game window and sends no sound to speakers
before running it. Record this as supplemental diagnostic evidence; it does
not replace zero-flag release or physical input/audio acceptance. Preserve
user device/driver/model settings. Do not access the local model while the
owner has reserved it.

## Local research model and tools

Use only the documented 9B local models for source review; default to
`huihui-qwythos-9b-claude-mythos-5-1m-abliterated-i1`. Keep at most two hosted
Sol subagents active (available model `gpt-6.1-sol`). Stream local output;
cancel confirmed loops, verify idle, correct the exact discrepancy, retry.
Do not impose generation/reasoning caps or change saved model settings.

For hosted delegates, check their current status. Use `followup_task` to start
a new task on an idle/completed agent; `send_message` alone does not restart
a completed agent. Do not mistake a queued message for active review.

Assign each delegate exact, unique output paths before writes, including raw
word exports, review documents and snapshots. Shared files have one writer;
other reviewers send proposed changes to that owner. Create new evidence
exclusively and preserve existing bytes before an authorized replacement.
After parallel work, verify cited file hashes before accepting a report.
Never repair an ownership collision by silently replacing its expected hash.

Follow [MODEL_COORDINATION.md](MODEL_COORDINATION.md) when the local model is
loaded: one focused exact-file source question per request, the documented
streaming tool bridge, no generation/output/reasoning limits, then verify
all cited lines locally. Cancel confirmed loops at request level, verify
idle, and correct the specific error before retrying. Do not change LM Studio settings or load another
instance. If no model is loaded, continue source reading directly and state
that limitation. Use a private Ghidra project copy for each agent session.
Search exact paths or bounded source globs; exclude build/log/generated trees
unless the task concerns them. Do not flood a tool result with entire logs.

End progress and final responses with concrete next steps for the owner and
the coding agent. Say “no action” for the owner when appropriate. Do not
imply work continues after a turn unless an active run or schedule exists.
