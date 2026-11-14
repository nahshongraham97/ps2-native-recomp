@echo off
setlocal
set ROOT=C:\Users\nahsh\mk port\Armageddon-backup\armageddon-reimplementation\external\forks\ps2-native-recomp
set BUILD=%ROOT%\build_native_gl_full
set CMAKE=C:\Users\nahsh\mk port\Armageddon-backup\dependencies\cmake\cmake-4.4.5-windows-x86_64\bin\cmake.exe
set LOG=%ROOT%\build_native_gl_full.build.log
set ERR=%ROOT%\build_native_gl_full.build.err
set MARK=%ROOT%\build_native_gl_full.build.marker
if exist "%MARK%" del /q "%MARK%"
"%CMAKE%" --build "%BUILD%" --target ps2run -j 2 > "%LOG%" 2> "%ERR%"
if errorlevel 1 ( echo BUILD_FAIL>"%MARK%" & exit /b 1 )
echo BUILD_OK>"%MARK%"
exit /b 0
