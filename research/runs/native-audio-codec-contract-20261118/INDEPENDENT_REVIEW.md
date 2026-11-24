# Independent mono codec review — 2026-11-18

Read-only source/artifact review; no implementation, test/build/game execution, CMake/settings/backup changes. Exact current hashes and per-file review evidence are in INDEPENDENT_REVIEW.json.

No concrete arithmetic, bounds or transactional defect found in the explicit predictor<=4/shift<=12 subset. Low-first signed nibbles, portable floor rounding, predictor bias, clamp-before-history and sample ordering match read-only backup spu2.cpp164..178. Int16 history bounds keep predictor intermediates below6million, so floor_div never negates INT32_MIN. Bulk output/flags/history are staged until complete success; valid-prefix failure leaves caller history unchanged. Allocation products are checked against host vector capacity and failures named. No hardware loop/address/envelope/resampling/stereo behavior is imported; unsupported filter/shift is rejected rather than normalized.

Historical verification gap at the initial review (superseded by the root recheck below): test_adpcm_codec.cpp41 checks actual payload output shape only, not golden PCM/hash or final history. Existing69440-byte PCM artifact matches recorded9f80ed99501ec7213408eb951b5036d6b3644bf310c0ed4d446277799cbaeee6, but the reported independentPython backup transcription is not retained as a reproducible procedure/comparison artifact. Root notified to strengthen the durable exact-output assertion or qualify the authored one-off parity report. Reviewer did not decode or run the test.

Historical documentation findings at the initial review: SOURCE.md3/33 retains unqualified no-build/build-pending wording despite later successful result. CMakepin in codec-implementation is historical before theGPUtarget; current three codec source pins, backupSHA and PCMhash match. Original measurements/reference bytes were preserved. Hardware/audible playback parity, flags/loops, envelopes, timing, count2/stereo and whole-game acceptance remain OPEN.

## Root follow-up after durable verification correction

This annotation is by root, not a replacement of the independent reviewer's
original observation. The rebuilt test now asserts fixed payload and PCM
checksums and final history. The retained `tools/research/verify_adpcm_codec.py`
compares all decoded bytes with a word-oriented backup software reference;
`verified-harness.json` pins the rebuilt executable, script, source inputs,
69,440 PCM bytes and final history. The original shape-only test and unretained
inline proof remain historical. The verification gap is corrected at this
software boundary; hardware, audible output and live integration remain open.
