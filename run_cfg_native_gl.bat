@echo off
setlocal
set ROOT=C:\Users\nahsh\mk port\Armageddon-backup\armageddon-reimplementation\external\forks\ps2-native-recomp
set BUILD=%ROOT%\build_native_gl
set CMAKE=C:\Users\nahsh\mk port\Armageddon-backup\dependencies\cmake\cmake-4.4.5-windows-x86_64\bin\cmake.exe
set NINJA=C:\Users\nahsh\mk port\Armageddon-backup\dependencies\ninja\ninja.exe
set CLANG=C:\Users\nahsh\mk port\Armageddon-backup\dependencies\compiler\llvm-mingw-20261006-ucrt-x86_64\bin\clang++.exe
set CFGLOG=%ROOT%\build_native_gl.cfg.log
set CFGERR=%ROOT%\build_native_gl.cfg.err
set CFGMARK=%ROOT%\build_native_gl.cfg.marker
if exist "%CFGMARK%" del /q "%CFGMARK%"
if exist "%BUILD%" rmdir /s /q "%BUILD%"
"%CMAKE%" -S "%ROOT%" -B "%BUILD%" -G Ninja -DCMAKE_MAKE_PROGRAM="%NINJA%" -DCMAKE_CXX_COMPILER="%CLANG%" -DCMAKE_BUILD_TYPE=Release > "%CFGLOG%" 2> "%CFGERR%"
if errorlevel 1 ( echo CONFIGURE_FAIL>"%CFGMARK%" & exit /b 1 )
echo CONFIGURE_OK>"%CFGMARK%"
exit /b 0
