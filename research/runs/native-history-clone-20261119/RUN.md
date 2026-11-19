# Owned history clone — component verification, 2026-11-19

The native deep clone preserves the complete physical residual window and
cursor in independent storage. Source/clone mutation, clear, moves, self-move,
moved-owner rejection and source destruction pass all 380 declared trace rows
for N1/10/61/256. The existing history fixture was rebuilt from this source;
all 365 planned rows, overflow atomicity and lifetime checks still pass.
Both scoped executable symbol scans are empty and pin target/tool hashes
before/after inspection. No game source-route audit is implied.

The authoritative clone checker result is
[clone-complete-trace-oracle-02.json](validation-01/clone-complete-trace-oracle-02.json):
380 rows and 18 altered trace/metadata rejections. Review found the first
checker used an assertion for execution exit validation; Python optimization
could remove that gate. The correction uses explicit errors and was checked
with Python `-O` against the saved native trace. No C++ rerun was needed.
The initial eight-negative receipt remains historical; the correction execution
and current checker SHA are recorded separately. The native run itself exited0.

The existing history checker also now requires explicit valid SHA256 strings,
with twelve negative trace/identity checks. Its prior six-negative result
remains a dated measurement under the original run, not the current checker.

Executed SHA256 values:

* History: `cad25f77d4cfd3b3c3dc16d3c9054afb83d8f5766172263aa2637a17177deaec`
* Clone: `b6cb9bca75355f14da3a8e11b8af87c22b64dc8d4e188cd9824d0599e3d39703`

`validation-01` binds source/CMake/compiler/cache and executable identities.
The two preexisting IOP sources remain untouched. Size/allocation failure
branches are source-reviewed, not fault-injected. These are history ownership
checks, not complete filter, timing, movie playback or game acceptance.
No game, device, driver, local model or saved setting was operated on.
