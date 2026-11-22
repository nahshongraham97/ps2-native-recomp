# Decisive gain arithmetic constraints

Base4e73d4d09d8d9aee7cc4a9ee8d0eb6cd2ba97643; private sound-arithmetic-20261122. No production code changed. This is a concrete mathematical/source result, not completed12C030/12C440 or playable sound.

## What is exact and useful to a real implementation

Original12C030 forwards resolved gain into315C70 as a float alongside actual bank, selector, priority/output and flags9.12C440 first clamps requested gain, then computes bounded integer percent/100, request*percent at12C7D4, product*category at12C7F4, live_gain*product at12C7F8, and clamps again. Every source operation boundary matters. Existing metadata already provides the actual table operands and retained Session preference alias; no second owner is needed.

For an ALREADY source-clamped ordinary binary32 gain0..1,315D34 multiplication by16384 is an exact power-of-two scaling, and315D5C CVT.W.S truncates to an integer0..16384. The uint32 high-half branch at315D54 is unreachable in this bounded contract. This final integer result can be implemented by native arithmetic without a guest instruction engine. It does not establish the preceding gain bits. Existing audio_controls.cpp consumes already-integer Q14 controls; it cannot repair an incorrect semantic producer's rounding, and status conversion is the reverse direction.

There is currently no real caller that supplies the complete source-resolved clamped-gain contract to a new helper. Adding a standalone optional converter would not close the connected semantic play producer; none was added. The implementation path is: prove requested/live operand domains and numerical source result, build the semantic producer using current metadata and actual catalog/runtime handle, perform final Q14 conversion at that real boundary, retain the existing still-required envelope/caller ownership gates. Do not call it done by moving the undefined boundary into another adapter.

## Why host float, host RTZ and Xbox cannot fill the missing step

Pinned EE Core User Manual6.0 p156 defines exponent255 as finite, exponent0 as signed zero, with no IEEE NaN/Inf/subnormal semantics. P164 explicitly says the FPU may differ from IEEE rounding-toward-zero in its least significant bit because guard/round/sticky are not used. It says usually, not a universal error bound. P165 repeats this incompatibility. P164 was rendered and visually inspected. Instruction manual p356 establishes integer truncation; it does not supply exact multiplication/division low-bit algorithms. FE_TOWARDZERO, volatile float and a normal x64 float chain cannot establish equivalence. A bit-level EE multiplier/divider model would violate the requested implementation direction and is not introduced.

Original Xbox23C03C..23C05B executes three x87 FMULs with no intervening binary32 stores, then one FSTP. PS2 separately rounds the three MUL.S results. Donor precision/control-state, different scalar rows, and IEEE exceptional operands also differ. Its logical operation order corroborates intent only, not source numerical parity. No x87 control state is inferred and no donor DSP/device implementation adopted.

## Independent decisive experiment

analyze.py uses exact Python Fraction arithmetic to compare TWO IEEE mathematical alternatives, not an EE simulator. Real sound0 packed06fc8000 has percent100/category3 scalar3f266666 and default live gain3f400000. With positive normal requested-volume bits39034835, per-stage IEEE nearest yields category-product38aaaaab then gain38800000 =1/16384, Q14=1. Per-stage IEEE truncation yields38aaaaaa then387fffff, Q14=0. Thus low-bit arithmetic choice can survive final integer quantization. RATIONAL_RESULTS.json preserves all exact expectations. Neither branch is asserted to be the EE result.

The discovered direct caller12C130 supplies volume1. The adversarial volume is valid for12C030's ABI/clamp but is NOT proved reached by that caller. default_corpus.py independently checks all9656 real rows at volume1/defaultgain3/4: the two IEEE alternatives produce identical Q14 values. Therefore default-only tests can pass while leaving the unresolved behavior undetected; this agreement is not EE equivalence, a bound on EE error, or exhaustive changed-setting/caller coverage. Sixth preference raw word14 must remain raw until source-domain proof; host NaN classification cannot silently select a source result.

## Outcome and next required evidence

Exact complete gain computation is genuinely unproved by the available pinned manual/listings/donor. Do not implement/claim an IEEE-equivalent production policy. Required decisive evidence is either independently authenticated original-result coverage of the actual bounded game operand domain (including live settings/restore and all reached callers), or a source-proved game-level algebraic/domain reduction that makes low bits irrelevant at final Q14 across that entire domain. An oracle using an emulated FPU is not hardware truth. Any still-uncovered operands remain named unsupported; do not invent zero-success, approximate timing, or envelope coefficients.

PINS.json records source/manual identities; three complete EE spans rejoin directly to original ELF SHAa2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f. Xbox listings reuse the pinned original source record. I executed Python rational/data checks and PDF extraction/rendering only. No compiler/client/game/window/physical audio endpoint ran, no source helper was injected, and no acceptance is claimed.
