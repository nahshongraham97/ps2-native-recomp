@echo off
setlocal
rem Step 1 gate: retail Armageddon boot with the native CDVD/content service.
rem No disc image, no CDVD device: the game folder's extracted content is the media.
set ROOT=C:\Users\nahsh\mk port\Armageddon-backup\armageddon-reimplementation\external\forks\ps2-native-recomp
set RUN=%ROOT%\build_native_gl_full\diagnostics\cdvd-native-boot-20261113
set CARD=%RUN%\armageddon.ps2
set SOURCE=%ROOT%\games\mkarmageddon\disc\armageddon.ps2

set PS2_NATIVE_GL=1
set PS2_VU_AOT_STRICT=1
set PS2_EE_LIVE=1
set PS2_EE_LIVE_BG=0
set PS2_EE_LIVE_JOBS=2
set MKA_INTERCEPT_COMBAT=1
set PS2_KEYS=C:\Users\nahsh\mk port\Armageddon-backup\armageddon-reimplementation\config\armageddon-keys.ini

if not exist "%RUN%" mkdir "%RUN%"
if not exist "%CARD%" copy /y "%SOURCE%" "%CARD%" >nul

start "armageddon-cdvd-native" "%ROOT%\build_native_gl_full\ps2run.exe" "%ROOT%\games\mkarmageddon\disc\SLUS_214.10" --window --memcard "%CARD%" --quick-dir "%RUN%"
exit /b 0
