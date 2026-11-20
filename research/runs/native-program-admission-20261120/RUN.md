# Actual Service-owned program admission — results, 2026-11-20

PASS_NATIVE_SERVICE_OWNED_PROGRAM_ADMISSION. Component verification only;
full native retail acceptance remains OPEN. Build base9415cb997e82a4731041a22cdbf82928df01df39,
Release existing native-only build with PS2_GENERATED empty. Actual original MS2/MS4
identities, compiler, source and exact executable receipts are pinned in the fork.

Implemented actual Service -> Protocol -> Session -> PreparedProgramWork admission.
Each BankProgramWork owns its immutable exact command/full token, actual Work and
Service State, plus real global/per-bank operation reservations. No lower Work
ownership escapes independently. Admission takes no program-category resource
lease. Build/publication wait outside outer locks. Queued/running cancellation,
private readiness, publication and explicit retirement remain distinct. Failed
retirement retains ownership; actual Work cleanup precedes reservation release.
Same-bank unload and close reject retained work. Unload closes admission before
Protocol mutation; known refusal restores it, unexpected before/after mutation
quarantines the exact bank. Other banks remain usable. Existing admitted sync
preparation still has unload revocation semantics; it was not silently changed
into the new owned operation. No sound context is inferred from bank/opaque word.

Source review corrected a phase/reservation observation-order race before build.
Exact initial draft is preserved. All9 source/fixture review pins remained current.

Actual tests:10 affected executables built and passed;379 final pinned inputs,
354 unchanged prior inputs. New actual admission test covers7 lifecycle groups;
new unload-gate test covers5 injected before/after/refusal/exception boundaries.
Unwrapped Service exercises actual source PCM, publication waits, foreign-thread
rejection, unrelated-bank use, moved/dropped owner lifetime, abandonment, invalid
and eager selector rollback, and retained immutable results. Existing bank,
Protocol, Service, resource-domain, injected preparation exception and playback
reservation/gap clients pass. Only the designated gate/exception/gap executables
have1/1/5 test-only linker wraps; production library is unwrapped. No device/game
runtime linkage appears in the scoped symbol/build rules.

Run01 passed build+first3 executions, then the reporting wrapper incorrectly
required a Service symbol in a bank-only executable. Failed script/receipt remain.
Continuation02 proves its actual Session symbol, the exact saved executable and
NM identities, reuses the3 passing executions and runs only7 remaining clients.
No native source/assertion was changed, no build or passing test was repeated.
The dated plan contains the recorded pre-execution/pending checkpoints; this
result is their outcome. Certificate SHA256: bba7dbb44acf55c11b9b4cd79f2db3f63d557bc2877959f95b0bf90ca8d81843.

No decoder, movie corpus, unchanged whole-bank policy corpus, game/ps2run,
physical GPU/audio/input endpoint, local model/server, driver, card/save/key or
setting operation occurred. Frozen movie decoder certificate is unchanged.

OPEN: actual source worker queue/job/runtime descriptor; callback argument and
bank-count ordering; broader vs targeted queued removal and running suppression;
public sound identity revocation/current-context command12/forced output cleanup;
retail cached-program branch; renderer-wide movie surround and actual host/game
binding. The new program_work_active response is a named native overlap policy,
not proof of source unload's wait/callback semantics. A retired ticket is not
proof that a separately queued physical file close has completed. Native game
memory/static coverage, physical fidelity and all-mode acceptance remain OPEN.

Owner: no action. Coding agent: build the source-owned program job coordinator
on these actual retained Service tickets, then bind current-sound and renderer
operations around real movie playback. Do not reopen decoder research.
