# Xbox-first mode/scene implementation

Owner explicitly directs Xbox-first for ALL remaining implementation, not
only CPU work. Use the easier matching portion from either retail version
inside the existing native architecture; PS2 remains the behavioral reference.
The existing Ghidra plugins/programs are already prepared, not a setup task.

Start with Xbox mode/scene initialization corresponding to the still-required
PS2 mode2460B0 and12A4F0(0) boundaries. Reuse current PS2/source/default evidence
instead of redoing it. Original XBE identity/mapping comes from the existing
xbox-donor-20261121 records and is checked against actual bytes.
Private project: mode-scene-xbox-1c3c47b6b8d84fc5874adda06c554d02.

Stage: bounded original-data/source discovery; no donor implementation claim.
Frame sourcea36ee13 is already published and passes four clean-source clients.
No original game/user/audio files, drivers or model settings are changed.
No game, visible window or audio endpoint is launched.
Owner: no action. Agent next: identify Xbox mode-table callers and adapt the
simpler matching source into existing owners; unknown behavior stays OPEN.

Original-data finding: all nine existing PS2 sound-mode descriptor names are
present in the Xbox image, with corresponding descriptor name fields at
56C958+24h*index. The selector column begins56C950; this is not proof of
the complete descriptor's base/layout. These addresses come from
the XBE's own checked sections, not reused ELF offsets. Next inspect its real
callers and selector implementation in the existing x86 Ghidra program.

Source-mode-01 inspected the actual Xbox x86 listing:23C8A0 is the matching
bounded nine-mode selector and calls23D060 with descriptor/policy as two32-bit
cdecl arguments;185 instructions across three saved functions join original
XBE sections. Descriptor policy0 calls23D4D0 then23D830. This is source-derived
correspondence, not a new native implementation or complete policy/lifetime
proof. The mode-service boundary reaches audio catalog ownership, whose files
remain untouched; trace the startup caller next to locate mode-object setup.

The saved Xbox startup caller19C680 contains the matching sequence: current
sound23AD80, mode objects1F98D0, preferences235A40, mode selector23C8A0(0),
first-scene2330C0, then call368850 and initial task2001/20. The decompiler
labels that call as a thunk to3728C0; its actual body/terminal service is not
yet exported, so it is not accepted as verified work-storage semantics.
Its x86 listing and original-byte join are saved in source-callers-02.
Next export actual1F98D0, avoiding further PS2-only alias archaeology.
This identifies the connected donor boundary; it does not by itself prove
the downstream object fields or permit copying Xbox layout/addresses.

Actual1F98D0 is36 x86 instructions, all joined to the XBE. It uses one child
reset helper1F9BF0, writes two minus-one metadata words plus status0, and
publishes child+8 / child+240 aliases. Material platform difference: Xbox
does this for FOUR active player records; PS2's verified2460B0 does it for
TWO and publishes a different second child offset. Do not import four-player
layout or guess offset correspondence. The x86 helper/alias consumers are
the next focused source, reusing the existing native child reset/owners.

The source-child-04 original-data preflight correctly refused to read736500
as file-backed input; it is loader-zero alias storage. No Ghidra process ran
and no runtime changed. EXPORT_FAILURE_04.json preserves the failure.
The query exporter now distinguishes section-proven loader-zero queries
from original file-backed instructions; retry uses new outputsource-child-05.

Source-child-05 closes the first real consumer hop:736500 is read by105510,
which places the alias in field+8 of each12-byte player record at82ECA0.
Its caller1052B0 installs that array in the task and loads
"common/kart/kart_main". This is a Motor Kombat consumer, not proof that
the alias refers to named presets or KAF. The child helper1F9BF0 clears35E0,
sets initialized byte/name/action masks, calls188A60 then2532B0(child+278),
then fills40/30 indices with-1. PS2 child size3C30 and offsets differ.
Next inspect the Motor Kombat record consumers to recover the actual value
contract before binding it to a native field. No audio file changed.

Source-kart-06 follows the real record consumers and source character chooser;
it confirms a Motor Kombat player-selection record, but those reached native
script handlers do not directly read the profile alias payload. Its exact
payload contract remains OPEN; do not invent a56-byte host record or call
the alias a named-preset pointer. The simpler Xbox reset algorithm is identified
but NOT yet adopted. Next inspect the actual kart script/data consumer or
an equivalent verified native field, then implement/test mode-object startup.
The saved partial proof is retained without blocking use of Xbox elsewhere.
