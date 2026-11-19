# Independent native user-path review — 2026-11-19

Two concrete issues reported to root; source review only, no tests/builds or profile writes. Exact reviewed hashes are in JSON.

1. test_native_user_paths.cpp17 recursively removes its computed temporary root without first resolving/checking absolute containment within intended temporary parent. Exclusive create_directory is useful ownership evidence but does not satisfy the required resolved-target safety check.
2. host/main.cpp780 opens census.txt with narrow std::fopen(UTF8), while LogTee uses Unicode-aware ps2::os::open. On Windows non-ANSI profile paths, census may silently fail rather than share the actual KnownFolder destination. Existing Unicode helper uses _wfopen.

KnownFolder PWSTR lifetime is correctly owned/freed on success/failure. NativeUserPaths resolution creates no directory, validates native existing absolute root, and has no CWD/environment/content fallback. The real-profile fixture performs lookup only; writes are under its new temporary root. Log/census share log_directory in the window branch; headless stays outside that branch. Required shell32/ole32/uuid links are present in runtime/media/fixture targets. These properties do not certify all preexisting logger behavior or retail acceptance.

Findings remain pending a source recheck of root corrections; no production or test files edited by reviewer.

Existing named gap, separately reported: LogTee::start assigns file_=ps2::os::open atmain142 without checking null before its success return180/log-written claim179. This predates NativeUserPaths; directory creation does not guarantee per-file open succeeds. No source fix or logger-wide certification is implied.

## Correction recheck

Both reported findings are closed by source recheck at distinct correction_recheck JSON hashes; initial hashes above remain BEFORE evidence. Fixture now canonicalizes/validates absolute exclusive temporary child and revalidates unchanged containment before recursive removal. Census now uses Unicode-aware ps2::os::open and reports open/write/close errors. Isolated Unicode helper test checks exact census bytes; no real-profile writes added. Other agent reports standalone/hostobject PASS; this reviewer did not rerun or execute the actual exit callback. The separately reported inherited LogTee null-file success gap remains open unless root corrects it independently.

## Log startup INITIAL-OPEN issue closed

Current host/main SHA f1d0969197d273e576706cd496233861a9245e1a37beb9da74468de4407d668b rejects directory/enumeration/filename errors and null Unicode file open before pipe/thread creation, with named path/host/errno diagnostics. This closes the separately recorded INITIAL-OPEN success-with-no-file startup issue; earlier open status above is historical. Root retained exact-class harness receipts log-start-before-20261119-02/verification.json and log-start-after-20261119-01/verification.json are hash-pinned in JSON: original1226028 accepted blocked file-root (expected rejection probe exit1); corrected class rejected it (exit0) and retained exact emitted bytes through normal pump/stop (exit0). Reviewer inspected source/receipts only, no rerun. No game/viewer/AppData writes are credited. Ongoing write/rotation integrity, filename collision policy and partial pipe-start failure cleanup remain open; no logger-wide acceptance follows.

## Final pure-native helper source recheck

Verified final helper/header/test/main at separate JSON-pinned hashes: direct Windows wide-path FILE open with typed binary mode, empty/NUL/invalid-enum rejection and named error; LogTee initial/rotation open and census share it. Standalone fixture no longer includes ps2 utilities and retains Unicode/exact-byte/error/cleanup cases. No concrete final helper defect found. Earlier ps2::os helper references describe historical intermediate corrections. Root reports13-case/12-symbol-audit checkpoint PASS; reviewer did not rerun. Ongoing write/rotation integrity and partial pipe-start cleanup remain open.
