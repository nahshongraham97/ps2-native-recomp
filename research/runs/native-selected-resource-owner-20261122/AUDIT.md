# Requested binding audit — published main 3f01556

**Outcome: the brief is stale; the requested font production binding remains
blocked. No new production implementation, build or game pass is claimed.**

## Baseline and scope

Parent `02ff360c499db53ba12e53fb94253fc225eec891`, fork/origin main
`3f0155622a148ac490565337d65cb4aaf0cbce45`. This work uses the separate
`selected-resource-owner-20261122` branch/worktree. The existing dirty integration
batch, other worktrees, audio, original inputs, saves and local model are untouched.
The cited `Master_Implementation_Roadmap.docx` was not located in this checkout;
this is a bounded source/contract audit, not certification against that document.

| Requested effect | Actual published source |
| --- | --- |
| `initialize_retail_saved_profile_records` | Defined in `runtime/src/native/retail_saved_profile_bindings.cpp:31`, compiled into the startup target by `cmake/native_game.cmake:248` |
| `reset_saved_profile_children_246070` | Defined in the same production file at line49, not only test fixtures |
| `reset_selected_resource_334700` | Defined in `runtime/src/native/retail_arcade_ladder.cpp:16`; resets arcade-ladder state, not font data |
| `configure_selected_resource_zero_16fc70` | Still required/undefined; the existing decoded CPU section is deliberately partial |
| `clear_nonowning_references_18da30` | Still required/undefined; clears object/cookie and task/cookie references, not a font cache destructor |

The existing clean profile result is
`../native-retail-saved-profile-records-20261122/clean-validation-01/VERIFICATION.json`.
Its actual schema has `status` string, `clients` list and
`remaining_undefined_symbols` list; its recorded clean source is `a4147aa`.
It records seven component clients and64 remaining full-entry names, including
the font/section effects but not the two implemented profile bindings.
Those names are a dated diagnostic, not a full work inventory or37 audio-only
obligations. This audit did not rerun that unchanged checkpoint.

## Original authority actually inspected

`SOURCE_RECEIPT.json` joins every listed byte in the nine exported contiguous
spans to the actual ELF `PT_LOAD` mapping, not a guessed VMA subtraction.
It pins original LE32 words, file offsets, span/listing hashes, the row-zero
descriptor at `005A1AF0`, its two original strings and all ten original
atlas/MET payload identities across the five locale sections.

The program is the SHA-256-pinned `SLUS_214.10` image
`a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f`;
the actual private listing uses `r5900:LE:32:default`, language1.4.
`source-01/PROGRAM.txt`, the saved invocation and isolated session manifest
establish the actual decoding used. The PS2 and Xbox private projects have
independent storage/JVM directories; no shared MCP program was switched.
No Xbox implementation is adopted.

Relevant full spans: `16FC70..16FD7C`, `300B60..300D4C`,
`300440..3005CC`, `334700..334750`, `3DE190..3DE204`.
`18DA30..18DA60` is only the tail of original `18D970`; its Ghidra
split-function decompilation does not prove an independent function or ABI.
Valid prior whole-section/caller evidence is reused from
`../parallel-native-font-zero-20261122/source-full/` rather than reclassified
as a newly implemented service.

## Behavior established; do not replace it with a font-file loader

- Row zero supplies `EDDMONDGRAY14`, `EDDMONDGRAY14_MET`, logical section10
  and two initially null file-backed cache words. These are not loader BSS.
- `16FC70` skips both lookups on a cache hit. On a miss, it calls `300B60`
  first. A null texture skips the MET lookup and all cache writes.
- When the texture exists, `300440` is called with a stack length output
  initialized to zero in the earlier call delay slot. Its result may be null.
  `16FD48` publishes the texture and `16FD4C` publishes that MET result.
  Requiring a nonnull MET before caching would change this source path.
- The returned pointer identifies the native equivalent of a cache slot,
  not evidence of a ready font. No caller may infer readiness from that
  nonnull slot address.
- `300B60` searches section subrecords forward; `300440` searches backward.
  Both perform the actual `19C140` lookup/type admission and name scan.
  `300440` also writes the matched record length even when its payload pointer
  is null before continuing backward. Preserve that effect order.
- `16FD20..16FD44` uses sign-extended `ADDIU -256`, bitwise masks and ORs,
  ending with texture sampler metadata low16=`3301` while retaining high16.
  This establishes bit behavior, not a nearest/linear or wrap/clamp mapping.
  The runtime host sampler mapping remains unproved.
- The existing complete `3011D0` listing iterates every type2/3 record,
  calls `3DE190`, then sets converted at `3014BC`. Selecting one decoded
  atlas/MET pair cannot stand in for that entire operation.
- `3DE190` calls `2DFE50` for a pending job, clears its handle afterward,
  and recycles the job only when its second pending field is zero.
  It does **not** execute `SCR_MANAGED`; a prior partial-CPU label must not
  be promoted into an invented script-execution requirement.

The existing native CPU/font decoders remain reusable. The pending ordinary
GL image capability is a real storage/cleanup dependency, not the source
loaded-section producer or actual EngineOpen/current-context acquisition.
The native section workspace currently refuses unported populated retirement.
Its raw storage must not be relabeled as a converted live section.

## Implementation gate and next connected work

No new successful binding or substitute resource owner was written: that would
either duplicate existing CPU ownership or invent the missing loaded/GPU
publication contract. No source-derived cache state is published from a
partial CPU decode. The production native section owner, cache-resource
lifetime/invalidation, host sampler translation and connected font tests
remain **OPEN**.

1. Primary integration: finish the existing reviewed file-job/GL corrections
   and clean verification without absorbing other dirty work.
2. Implement the actual native loaded-section owner through
   `19A630/19A5E0/19C140 -> 3011D0 -> 3DE190`, using those real services.
   Preserve partial conversion, cancellation, effect order and retirement.
3. Implement `16FC70(0)` against that owner and persistent Session cache.
   Test real original atlas/MET consumption through existing layout/geometry,
   cache hit/miss, null texture, null MET, matched-null length output,
   sampler high16 preservation, owner/thread/reentry and cleanup errors.
4. Compile the actual reset caller and compare a matched oracle scenario.
   Component checks and full-game acceptance remain separate.

Executed here: private headless EE export and read-only original-byte/source
authentication, both exit0. No new C++ build, connected run, visible window,
physical audio endpoint, presentation or gameplay acceptance occurred.
No Main/CMake changes exist to integrate. `parent-documents.patch` contains
only the affected documentation corrections for the primary writer; it is
not silently applied to the dirty shared documents.

Owner: no action. Coding agent: close the loaded-section prerequisite before
claiming or landing the font binding. This branch has no active background run.
