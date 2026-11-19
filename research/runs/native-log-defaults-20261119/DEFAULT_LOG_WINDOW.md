# Default separate Windows log viewer

Current source change: normal window startup always initializes its live log and, on Windows, starts the separate PowerShell viewer. The redundant `--log-window` enable switch and `PS2_NO_LOG_COPY` bypass were removed. Headless runs remain outside this block. Log initialization, executable-directory resolution, missing viewer script and viewer-process creation failures are named; failed required log/viewer setup returns 1. Successful CreateProcess only proves process creation, not persistent viewer health.

Only host/main.cpp changed. The before snapshot is `main.cpp.before-default-log-window`; exact diff is `default-log-window.patch`. Older audit/cleanup source hashes and object receipts are historical, not current source pins. Current hashes and line evidence are in the JSON companion.

The exact saved per-object toolchain command compiled host/main.cpp with exit 0 and empty stderr: `default-log-window-object.json`. No link, game, GUI/viewer window or driver action was performed; runtime visual behavior is not verified by this object compile.

## Native userdata blocker

`runtime/src/quick_keys.inc:76-85` selects explicit quick-dir, explicit memory-card parent, then content-program parent. Window log initialization happens before host/main.cpp's default memory-card discovery. With no overrides, log files therefore go under the resolved content executable directory. If that directory belongs to an immutable accepted package, logs would violate its exact inventory. A native userdata root outside the package still needs an explicit production binding; this change neither selects a new location nor moves/changes original saves/cards/settings. The viewer script must also exist beside the executable and remain part of the accepted inventory. No accepted release binding or package is asserted here.
