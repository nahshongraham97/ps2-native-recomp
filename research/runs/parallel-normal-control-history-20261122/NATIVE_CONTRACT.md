# Source to native control-history contract

Source authority is original MSL SHA f62dfbcedd39bc92cb8b9c02bb685b956a10357f9181d7f8c2801d8eaff07af7, not generated IOP code or decompiler names. SOURCE_JOIN.json rechecks every actual listing word. The decompiled orientation is preserved separately; reference_history.cpp is an independent four-branch game-math transcription checked against these words, never a production device/DSP/instruction engine.

|Actual source|Native effect|
|---|---|
|A49C SH atA4E4/A4E8; SHzero atA4EC (inspect full listing)|Fresh left/right historyF000 and remaining0. Region/delta/divisor are not claimed zero-born; typed optional provenance persists until actual writes.|
|959C..960C|Truncated curve index; mode0 emits same positive coefficient twice and leaves ALL history untouched.|
|9610..965C|Mode1 folds rear pan into front domain before sign/region/history.|
|9660..987C|Positive rear region4; choose baseline sign from cachedright<=0,excludingF000. Controlled transition channel isleft.|
|9880..9A84|Positive/front region2 (includingzero); choose baseline sign from cachedright<0,excludingF000. Controlled channel isleft.|
|9A88..9CA8|Negative rear region3; choose sign from cachedleft<=0 while cachedright!=F000. Controlled channel isright.|
|9CAC..9EB8|Negative/front region1; same cached sign predicate; controlled channel isright.|
|9534..9598;934C..9378|Transition comparison is abs(left_difference)>=10001 OR abs(right_difference)>=10001. Differences fit signed32 over two signed16 fields.|
|Each region transition block|Only when region differs and9534 succeeds: store signed16 delta of the controlled cached channel minus target; set divisor=abs(delta/2500),then remaining=divisor. Zero divisor with remaining0 is valid and does not divide.|
|Each countdown block|If remaining>0,output cached-controlled-minus(delta/divisor) narrowed to signed16 and decrement once. A retained countdown can carry across subsequent calls/region changes exactly as source. No sample/frame timer is invented.|
|9EBC..9F00|Publish signed16 caches before clearing output bit15 on negative values. Interpret the resulting signed15 constant operand into nativeQ15; preserve overflow/narrowing behavior,not just logicalgain*2. Manualpage42 corroborates int1:0:14. No register/sweep model is retained.|
|9F88..A0C4|959C invoked separately for each ordered PCM plane,then channel routing suppressed. A two-plane update can therefore advance countdown twice.|
|3FCC factor calculation|Existing bounded native factors/overflow/repeat/spatial/deferred-start/zero-pitch gates remain; this work does not declare every original caller/control supported.|

Runtime ownership: actual HostVoice::Data holds the one authoritative history for a bound normal output. start seeds source fresh state and computes it; rendering copies/commits that state with the existing transactional voice state but never advances the invocation counter. update_controls checks exact program/clip/command identity before computing new controls; it preserves source cursor/fraction/envelope/profile. NativeProgramCopy reconstructs its actual retained descriptor operands and reaches that same output method; its instance observation exposes a value snapshot. Clone histories are independent even when immutable PCM/runtime backing is shared. No raw borrowed history pointer, duplicate Main scalar, callback, lock, clock read or per-frame cadence is added. Caller serialization is the existing NativeProgramCopy/HostVoice contract,not a new thread-safe concurrent mutation promise.

Only bound-output history/control use is delivered. Missing/retired/unbound outputs remain named refusals; this is not a complete unbound-descriptor producer or pause coordinator. Invalid externally supplied history that requests a positive countdown without a positive established divisor fails by name before state publication; the sealed native output history cannot create that state. Other validation failures preserve current controls/history/cursor. The source formatter's otherwise-unread initial fields stay absent until their writers; no unknown default is fabricated.

Independent expected results: reference_history.cpp retains the source's four explicit branch forms while production uses combined region logic. It consumes the previously frozen600 curve fixture values,independently reauthenticated in CURVE_AUTHENTICATION.json against original static arrays and original signed-DIV/SH/SB constructor words. No candidate function or output generates the expected corpus. reference-03 produces26576 rows before the candidate build,with before/after provenance masks. Tests include explicit mathematical cache/threshold states,marked as fixtures rather than actual gameplay producers. The reference is not a retail execution oracle and does not establish heard audio.

1299B0/315620/3158D0 source tracing is retained as next dependency evidence. Complete context pause/resume still needs both source lists/construction waits,actual secondary/context controls,per-program pause timestamp/deadline and retained-ramp shifts,queued/held/pending ownership,correct output cursor freezing without release,and one real Session/game latch owner. None is replaced by the worker shutdown pause or by immediate mute/cached restoration. Full pause/caller/envelope/output/device/game acceptance remains OPEN.
