# Native retail movie source composition — 2026-11-19

validation-01/verification.json reports PASS_SCOPED_NATIVE_RETAIL_MOVIE_SOURCE.
All 38 native sources pass: three full software-decode goldens and 35
first-video-only observations, totaling 8,928 observed video frames. Four missing
requests fail named. Raw PS/PES prefix/packet/PTS/quarter-PTS owner joins pass.
One executable symbol audit finds zero defined forbidden namespace symbols.
Source, executable, three codec DLL and asset pre/post identity pins are retained.

validation-01 contains build.json, launch.json, source.json, source.stdout,
source.stderr, symbols.json, verification.json and the explicit fixture assets.tsv.
The source factory resolves an owned semantic request, opens its exact explicit
binding and consumes that opened asset in the existing native decoder; it adds
no file reopen/search or guest ABI. Request metadata, move/lifetime errors and
synthetic invalid-binding/error-order checks are part of the fixture. Parent
SOURCE_FACTORY_IMPLEMENTATION.md retains the source scope and historical
unbuilt handoff; SOURCE_FACTORY_VALIDATION.md records root's separate verifier
contract. `raw-oracle-edges.json` separately records 11 positive single-video
MPEG1 cases (including all prefix splits, half-open boundary and absent/zero/
high timestamps) and three rejected malformed/out-of-range cases. Exact bytes
are retained; production code and the main verifier remained unchanged.

Only three full YUV/PCM/count/timestamp goldens are claimed. The other 35 raw
owner joins do not independently prove earliest picture or YUV correctness.
Decoder/source ownership is not player readiness, completion or audio drain.
Original game caller/memory, player clock, mixer, render/subtitle integration
and full game acceptance remain OPEN. No game/device/audio/video output,
driver/settings or backup operation occurred.
