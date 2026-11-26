@echo off
setlocal
set ROOT=C:\Users\nahsh\mk port\Armageddon-backup\armageddon-reimplementation\external\forks\ps2-native-recomp
set BUILD=%ROOT%\build_native_gl_full
set CMAKE=C:\Users\nahsh\mk port\Armageddon-backup\dependencies\cmake\cmake-4.4.5-windows-x86_64\bin\cmake.exe
set NINJA=C:\Users\nahsh\mk port\Armageddon-backup\dependencies\ninja\ninja.exe
set CLANG=C:\Users\nahsh\mk port\Armageddon-backup\dependencies\compiler\llvm-mingw-20261006-ucrt-x86_64\bin\clang++.exe
set GEN=%ROOT%\generated\mkarmageddon
set WGPU=%ROOT%\third_party\wgpu-native
set CAPTURES=%BUILD%\diagnostics\iop-capture-20261112\captures
set IMPORTS=%BUILD%\diagnostics\iop-capture-20261112\generated
set MODULES=%BUILD%\diagnostics\iop-capture-20261112\generated_modules
set DONOR=%ROOT%\..\..\..\..\forks\God-Of-War-Native-Port\tools\iop\iopn_gen.py
set CFGLOG=%ROOT%\build_native_gl_full.cfg.log
set CFGERR=%ROOT%\build_native_gl_full.cfg.err
set CFGMARK=%ROOT%\build_native_gl_full.cfg.marker
if exist "%CFGMARK%" del /q "%CFGMARK%"
if not exist "%CAPTURES%\modules.txt" ( echo CAPTURE_INPUT_MISSING>"%CFGMARK%" & exit /b 1 )
if not exist "%DONOR%" ( echo DONOR_GENERATOR_MISSING>"%CFGMARK%" & exit /b 1 )
python "%ROOT%\scripts\iop_reference_codegen.py" "%CAPTURES%" "%DONOR%" "%MODULES%" > "%ROOT%\build_native_gl_full.iop_codegen.log" 2> "%ROOT%\build_native_gl_full.iop_codegen.err"
if errorlevel 1 ( echo IOP_CODEGEN_FAIL>"%CFGMARK%" & exit /b 1 )
"%CMAKE%" -S "%ROOT%" -B "%BUILD%" -G Ninja -DCMAKE_MAKE_PROGRAM="%NINJA%" -DCMAKE_CXX_COMPILER="%CLANG%" -DCMAKE_BUILD_TYPE=Release -DPS2_GENERATED="%GEN%" -DPS2_WGPU_DIR="%WGPU%" -DPS2_IOP_AOT_IMPORTS="%IMPORTS%" -DPS2_IOP_AOT_MODULES="%MODULES%" > "%CFGLOG%" 2> "%CFGERR%"
if errorlevel 1 ( echo CONFIGURE_FAIL>"%CFGMARK%" & exit /b 1 )
echo CONFIGURE_OK>"%CFGMARK%"
exit /b 0
