# Native Windows log destination

NativeUserPaths resolves the actual Windows FOLDERID_LocalAppData, canonicalizes that existing root and derives `MortalKombatArmageddon/logs`. It never uses an environment override, content directory, CWD or original memory card as fallback. Resolution creates no directories. Normal window startup explicitly creates the log directory, initializes the log and required viewer; the census hook uses the same process-owned path. Headless startup does none of these operations.

The primary installed API declaration is `shlobj.h:766`, with KF_FLAG_DONT_VERIFY at 752 and FOLDERID_LocalAppData in `knownfolders.h:73`, under the recorded LLVM-MinGW dependency. SHGetKnownFolderPath returns its allocated wide path, released with CoTaskMemFree through RAII. No KF_FLAG_CREATE is requested. HRESULT/host filesystem errors are retained in typed failure data; non-Windows default resolution explicitly reports unsupported. Root resolution and log creation are separate operations.

CMake adds the service to ps2runtime and the standalone native media library, with shell32/ole32/uuid. The new standalone test compiles only the path service, without media codecs, ps2runtime or generated game. Generated exact commands are retained in `native-user-paths-commands.txt`.

Standalone test build/run passed, as did host/main.cpp object compilation. Receipts: `native-user-paths-build.json`, `native-user-paths-test.json`, `native-user-paths-host-object.json`. Tests only read the real known-folder location; they create/write/read/clean exclusively a new temporary Unicode root and check no-create resolution, stable project component, repeated creation, invalid/relative/traversing/missing/file roots, blocked child creation, stale-output clearing and unresolved rejection. The first host compile exposed an incompatible capturing lambda for the raw function-pointer census hook; its failure receipt is retained and the corrected process-owned static directory compiled successfully.

No full runner link, game, viewer window or driver operation occurred. No actual AppData writes were made by tests. Existing save/card/state/key paths were untouched and still require separate native userdata binding before an immutable accepted package can be claimed. This supersedes only DEFAULT_LOG_WINDOW's log-location blocker; its historical compile/source pins remain preserved. JSON companion contains current source hashes.

## Independent-review correction

The initial revision had two gaps: the temporary test cleanup lacked a verified recursive-deletion boundary, and the census callback still opened its UTF-8 path with ANSI fopen. Current cleanup verifies the exclusively created directory's resolved absolute identity and exact canonical temporary parent before recursion, refusing a changed boundary. Census now uses the existing header-only `ps2::os::open` Windows wide-file helper and names open, write and close failures; stored census line bytes are unchanged.

The isolated temporary Unicode fixture now uses that same helper to write and read exact census bytes. Updated standalone build/test and host object receipts all exit 0; their original receipts are preserved with `.initial` suffix. The actual production census callback was not executed, and no real profile writes/window/game occurred. Current source hashes supersede the initial independent-review pins only for these corrections.

## Initial logfile-open guard

A subsequent review found LogTee::start could continue after file open returned null. It now reports path/errno and returns false before pipe/thread setup; directory creation/enumeration and filename-status errors likewise fail by name. The stale card-adjacent log comment now names LocalAppData policy. `log-open-guard-object.json` records host-object compilation exit 0. Root owns the separate isolated actual-class failure harness; this receipt does not claim its outcome. Ongoing writes/rotation integrity and partial pipe-startup cleanup remain unverified/open outside this bounded fix. Before bytes are preserved in `main.cpp.before-log-open-guard`; previous main hash remains historical in JSON.

## Native helper and strict symbol-gate correction

The first root consolidated run (`diagnostics/native-defaults-checkpoint-20261119-01`) had all 13 executions exit 0 but **failed** the strict symbol gate: the test's old inline utility defined `ps2::os::open(...)` and `ps2::os::path(...)`. That run is not 13 PASS. The checker was not weakened.

Current service/test no longer includes that utility. `mka::platform::open_native_file(const std::filesystem::path&, NativeFileMode, std::error_code&)` accepts only typed binary read/write modes, rejects empty/embedded-NUL paths and invalid modes, and directly uses Windows `_wfopen` on the original native wide path. Failure sets a host error; the returned FILE is caller-owned and must be closed. There is no UTF-8-to-ANSI fallback. Non-Windows calls explicitly fail unsupported. Live LogTee and census now use this same helper; LogTee retains a native path separately from its UTF-8 display string, including rotation naming/opening.

Current standalone build/test and host object receipts exit 0. Temporary Unicode helper tests verify exact bytes plus invalid/missing inputs. `native-user-paths-symbols.json` records a narrow defined-symbol inspection with no ps2:: symbols; root owns the full consolidated strict rerun. Previous receipts are preserved with `.before-native-helper` suffix. Earlier references above to the shared ps2 utility describe the superseded correction only. No runtime census/viewer/game or real-profile writes were executed.
