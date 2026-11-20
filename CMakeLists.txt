cmake_minimum_required(VERSION 3.20)
project(ps2-native-recomp CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PS2_GENERATED "" CACHE PATH "Folder with the C++ generated for a game")
set(PS2_IOP_AOT_IMPORTS "${CMAKE_CURRENT_SOURCE_DIR}/runtime/src/iop/generated_modules" CACHE PATH "Folder containing generated native IOP import bridges and registry")
set(PS2_IOP_AOT_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/runtime/src/iop/generated_modules" CACHE PATH "Folder containing generated native IOP module bodies and registry")
set(PS2_RUNTIME_LIB "" CACHE FILEPATH "Unsupported legacy prebuilt runtime input; native candidates must build reviewed runtime sources")

if(WIN32)
    add_link_options(-static-libgcc -static-libstdc++)
endif()
include(cmake/native_media.cmake)
include(cmake/native_audio.cmake)
include(cmake/native_sound_resources.cmake)
include(cmake/native_game.cmake)

# Build identity. ps2run prints this at startup so every run log names the commit
# and working-tree state that produced the binary; two runs are then comparable
# without guessing which build they came from.
set(PS2_BUILD_STAMP_DIR "${CMAKE_BINARY_DIR}/build-stamp")
set(PS2_BUILD_STAMP_HEADER "${PS2_BUILD_STAMP_DIR}/build_stamp.hpp")
add_custom_target(ps2_build_stamp
    COMMAND "${CMAKE_COMMAND}"
        "-DPS2_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
        "-DPS2_OUTPUT=${PS2_BUILD_STAMP_HEADER}"
        "-DPS2_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DPS2_COMPILER=${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/build_stamp.cmake"
    BYPRODUCTS "${PS2_BUILD_STAMP_HEADER}"
    COMMENT "Writing build stamp"
    VERBATIM)

if(EMSCRIPTEN)
    add_compile_options(-msimd128 -mtail-call -fwasm-exceptions -sWASM_LEGACY_EXCEPTIONS=0 -pthread)
    add_link_options(-msimd128 -mtail-call -fwasm-exceptions -sWASM_LEGACY_EXCEPTIONS=0 -pthread)
endif()

if(NOT EMSCRIPTEN)
    add_executable(eerecomp
        recomp/main.cpp
        recomp/r5900.cpp
        recomp/elf.cpp
        recomp/analysis.cpp
        recomp/codegen.cpp
        recomp/native_closure_codegen.cpp
        recomp/native_startup_codegen.cpp
        recomp/eerecomp_gen.cpp
        recomp/snr2.cpp)
    target_include_directories(eerecomp PRIVATE runtime/include)
    if(MSVC)
        target_compile_options(eerecomp PRIVATE /W4)
    else()
        target_compile_options(eerecomp PRIVATE -Wall -Wextra)
    endif()

    add_executable(vujit_dump recomp/vujit_dump.cpp runtime/src/vu_jit.cpp)
    target_include_directories(vujit_dump PRIVATE runtime/include runtime/src)
    add_executable(vurecomp recomp/vurecomp.cpp recomp/vurecomp_gen.cpp)
    target_include_directories(vurecomp PRIVATE runtime/include)
    if(MSVC)
        target_compile_options(vurecomp PRIVATE /W4)
    else()
        target_compile_options(vurecomp PRIVATE -Wall -Wextra)
    endif()
endif()

if(PS2_RUNTIME_LIB AND NOT EMSCRIPTEN)
    message(FATAL_ERROR "Prebuilt PS2_RUNTIME_LIB cannot supply the native candidate: build the reviewed runtime sources; legacy fallback libraries are forbidden")
else()
    set(PS2_PREBUILT OFF)
    set(PS2_API PUBLIC)
    add_library(ps2runtime STATIC
        runtime/src/memory.cpp
        runtime/src/dispatch.cpp
        runtime/src/observer_hook.cpp
        runtime/src/native_hook.cpp
        runtime/src/native_vertex_hook.cpp
        runtime/src/native/adpcm_codec.cpp
        runtime/src/native/bank_asset.cpp
        runtime/src/native/native_file.cpp
        runtime/src/native/native_user_paths.cpp
        runtime/src/native/content_vfs.cpp
        runtime/src/native/sound_bank.cpp
        runtime/src/native/sound_archive.cpp runtime/src/native/audio_resource_domain.cpp
        runtime/src/native/stream_source.cpp
        runtime/src/native/audio_banks.cpp
        runtime/src/native/audio_bank_protocol.cpp
        runtime/src/native/normal_sample.cpp
        runtime/src/native/bank_preparation.cpp
        runtime/src/native/audio_bank_service.cpp
        runtime/src/native/program_executor.cpp
        runtime/src/native/audio_controls.cpp
        runtime/src/native/host_voice.cpp
        runtime/src/native/playback_owner.cpp
        runtime/src/native/video_producer.cpp
        runtime/src/native/video_gpu_adapter.cpp
        runtime/src/native/movie_color.cpp
        runtime/src/native/movie_gpu.cpp
        runtime/src/native/movie_assets.cpp
        runtime/src/native/haptics.cpp
        runtime/src/native/retail_haptics_bridge.cpp
        runtime/src/native/controller_input.cpp
        runtime/src/port_config.cpp
        runtime/src/compliance_census.cpp
        runtime/src/system.cpp
        runtime/src/kernel.cpp
        runtime/src/hw.cpp
        runtime/src/cop.cpp
        runtime/src/disc.cpp
        runtime/src/dmac.cpp
        runtime/src/vif.cpp
        runtime/src/vu_micro.cpp
        runtime/src/media/mpeg2.cpp
        runtime/src/media/mpeg2_decoder.cpp
        runtime/src/gs/gs.cpp
        runtime/src/gs/gif.cpp
        runtime/src/gs/queue.cpp
        runtime/src/gs/raster.cpp
        runtime/src/gs/native_gl_backend.cpp
        runtime/src/gs/native_renderer.cpp
        runtime/src/iop/cpu.cpp
        runtime/src/iop/kernel.cpp
        runtime/src/iop/native_modules.cpp
        runtime/src/iop/adx_native.cpp
        runtime/src/iop/dev9_offline.cpp
        runtime/src/iop/usbd_offline.cpp
        runtime/src/iop/libsd_native.cpp
        runtime/src/iop/msl_native.cpp
        runtime/src/iop/padman.cpp
        runtime/src/iop/mcserv.cpp
        runtime/src/iop/mwfilefr.cpp
        runtime/src/iop/libs.cpp
        runtime/src/iop/hw.cpp
        runtime/src/iop/audio_service.cpp
        runtime/src/iop/sio2.cpp
        runtime/src/iop/io.cpp
        runtime/src/iop/cdvd_native.cpp
        runtime/src/iop/native_module_loader.cpp
        runtime/src/iop/native_legacy_state.cpp
        runtime/src/iop/sif.cpp
        )
    if(MSVC)
        target_compile_options(ps2runtime PRIVATE /W4)
    else()
        target_compile_options(ps2runtime PRIVATE -Wall -Wextra)
    endif()
endif()
if(NOT WIN32 AND NOT EMSCRIPTEN)
    target_link_libraries(ps2runtime ${PS2_API} ${CMAKE_DL_LIBS})  # vu_live.cpp loads libraries with dlopen
endif()
target_include_directories(ps2runtime ${PS2_API} "${CMAKE_CURRENT_SOURCE_DIR}/runtime/include" "${CMAKE_CURRENT_SOURCE_DIR}/runtime/src")
if(PS2_IOP_AOT_IMPORTS)
    if(PS2_PREBUILT)
        message(FATAL_ERROR "PS2_IOP_AOT_IMPORTS requires building ps2runtime from source")
    endif()
    if(NOT EXISTS "${PS2_IOP_AOT_IMPORTS}/iop_import_registry.cpp")
        message(FATAL_ERROR "PS2_IOP_AOT_IMPORTS is missing iop_import_registry.cpp")
    endif()
    file(GLOB PS2_IOP_IMPORT_SOURCES CONFIGURE_DEPENDS "${PS2_IOP_AOT_IMPORTS}/iop_imports_*.cpp")
    target_sources(ps2runtime PRIVATE "${PS2_IOP_AOT_IMPORTS}/iop_import_registry.cpp" ${PS2_IOP_IMPORT_SOURCES})
    target_compile_definitions(ps2runtime PRIVATE PS2_IOP_AOT_IMPORTS_ENABLED=1)
endif()
if(PS2_IOP_AOT_MODULES)
    if(PS2_PREBUILT)
        message(FATAL_ERROR "PS2_IOP_AOT_MODULES requires building ps2runtime from source")
    endif()
    if(NOT EXISTS "${PS2_IOP_AOT_MODULES}/iopn_registry.cpp")
        message(FATAL_ERROR "PS2_IOP_AOT_MODULES is missing iopn_registry.cpp")
    endif()
    file(GLOB PS2_IOP_NATIVE_SOURCES CONFIGURE_DEPENDS "${PS2_IOP_AOT_MODULES}/iopn_*.cpp")
    target_sources(ps2runtime PRIVATE ${PS2_IOP_NATIVE_SOURCES})
    target_compile_definitions(ps2runtime PRIVATE PS2_IOP_AOT_MODULES_ENABLED=1)
endif()
if(WIN32)
    # The optional WGL backend is compiled independently of WebGPU.
    target_link_libraries(ps2runtime ${PS2_API} opengl32 bcrypt shell32 ole32 uuid)
endif()

set(PS2_WGPU_DIR "${CMAKE_SOURCE_DIR}/third_party/wgpu-native" CACHE PATH "wgpu-native release folder")
if(EMSCRIPTEN OR EXISTS "${PS2_WGPU_DIR}/lib/libwgpu_native.a")
    set(PS2_GS_WGSL "${CMAKE_BINARY_DIR}/gs_wgsl.cpp")
    add_custom_command(OUTPUT "${PS2_GS_WGSL}"
        COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR}/runtime/src/gs/gpu/gs.wgsl -DOUT=${PS2_GS_WGSL} -DNAME=kGsShader -P ${CMAKE_SOURCE_DIR}/cmake/embed.cmake
        DEPENDS runtime/src/gs/gpu/gs.wgsl cmake/embed.cmake)
    set(PS2_BLIT_WGSL "${CMAKE_BINARY_DIR}/blit_wgsl.cpp")
    add_custom_command(OUTPUT "${PS2_BLIT_WGSL}"
        COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR}/runtime/src/gs/gpu/blit.wgsl -DOUT=${PS2_BLIT_WGSL} -DNAME=kBlitShader -P ${CMAKE_SOURCE_DIR}/cmake/embed.cmake
        DEPENDS runtime/src/gs/gpu/blit.wgsl cmake/embed.cmake)
    if(NOT PS2_PREBUILT)
        target_sources(ps2runtime PRIVATE runtime/src/gs/gpu/gpu.cpp "${PS2_GS_WGSL}" "${PS2_BLIT_WGSL}")
    endif()
    target_compile_definitions(ps2runtime ${PS2_API} PS2_GPU)
    if(EMSCRIPTEN)
        target_compile_options(ps2runtime PUBLIC --use-port=emdawnwebgpu)
        target_link_options(ps2runtime PUBLIC --use-port=emdawnwebgpu)
    else()
        if(NOT PS2_PREBUILT)
            target_include_directories(ps2runtime PRIVATE "${PS2_WGPU_DIR}/include")
        endif()
        if(WIN32)
            target_link_libraries(ps2runtime ${PS2_API} "${PS2_WGPU_DIR}/lib/libwgpu_native.a" ws2_32 userenv ntdll synchronization pthread m)
        else()
            target_link_libraries(ps2runtime ${PS2_API} "${PS2_WGPU_DIR}/lib/libwgpu_native.a" dl pthread m)
        endif()
        if(APPLE)
            target_link_libraries(ps2runtime ${PS2_API} "-framework Metal" "-framework QuartzCore" "-framework Foundation" "-framework CoreFoundation" "-framework IOKit" "-framework IOSurface" objc)
        endif()
    endif()
endif()

if(PS2_GENERATED)
    file(GLOB PS2_UNITS CONFIGURE_DEPENDS "${PS2_GENERATED}/*.cpp")
    add_library(ps2game STATIC ${PS2_UNITS})
    target_include_directories(ps2game PRIVATE "${PS2_GENERATED}")
    target_link_libraries(ps2game PUBLIC ps2runtime)
    if(MSVC)
        target_compile_options(ps2game PRIVATE /wd4100 /wd4101)
    else()
        target_compile_options(ps2game PRIVATE -Wno-unused-label -Wno-unused-variable)
    endif()

    if(EMSCRIPTEN)
        set(PS2_WEB_ELF "" CACHE FILEPATH "Game executable preloaded as /game/core.elf")
        set(PS2_WEB_BOOT_FILE "" CACHE FILEPATH "Text file holding the argv[0] the game sees, preloaded as /game/boot.txt")
        set(PS2_WEB_PATCHES "" CACHE FILEPATH "Patch list the runtime writes into memory, preloaded as /game/patches.txt")
        add_executable(ps2web host/web.cpp)
        if(PS2_WEB_BOOT_FILE)
            target_link_options(ps2web PRIVATE "SHELL:--preload-file \"${PS2_WEB_BOOT_FILE}@/game/boot.txt\"")
        endif()
        if(PS2_WEB_PATCHES)
            target_link_options(ps2web PRIVATE "SHELL:--preload-file \"${PS2_WEB_PATCHES}@/game/patches.txt\"")
        endif()
        target_link_libraries(ps2web PRIVATE ps2game ps2runtime)
        target_link_options(ps2web PRIVATE --profiling-funcs -sJSPI -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=268435456 -sPTHREAD_POOL_SIZE=2 -sALLOW_TABLE_GROWTH=1 -lidbfs.js -sSTACK_SIZE=16777216 -sENVIRONMENT=web
            -sEXPORTED_FUNCTIONS=_main,_ps2web_pad,_ps2web_pad_connected,_ps2web_resize,_ps2web_gpu_ms,_ps2web_frames,_ps2web_primitives,_ps2web_audio_ring,_ps2web_audio_queued_ms,_ps2web_idle_ms,_ps2web_vu_interpreted,_ps2web_quick_state,_ps2web_video_options,_ps2web_stream
            -sEXPORTED_RUNTIME_METHODS=callMain,addRunDependency,removeRunDependency,HEAPU8,ENV
            --shell-file ${CMAKE_SOURCE_DIR}/web/shell.html --preload-file ${PS2_WEB_ELF}@/game/core.elf)
        set_target_properties(ps2web PROPERTIES SUFFIX ".html" LINK_DEPENDS ${CMAKE_SOURCE_DIR}/web/shell.html)
        return()
    endif()

    add_executable(ps2run host/main.cpp host/mka_unit_wrappers.cpp)
    target_link_libraries(ps2run PRIVATE ps2game ps2runtime)
    target_include_directories(ps2run PRIVATE "${PS2_BUILD_STAMP_DIR}")
    add_dependencies(ps2run ps2_build_stamp)
    target_link_options(ps2run PRIVATE
        "-Wl,--wrap=_ZN3gen9unit_0018ERN3ps23CpuEjj"
        "-Wl,--wrap=_ZN3gen9unit_0020ERN3ps23CpuEjj"
        "-Wl,--wrap=_ZN3gen9unit_0025ERN3ps23CpuEjj"
    )
    add_executable(test_disc_vfs EXCLUDE_FROM_ALL tests/test_disc_vfs.cpp)
    # This parser test must not acquire guest memory, hardware, or scheduler
    # dependencies through ps2runtime. It owns ordinary host byte buffers.
    add_executable(test_sound_bank EXCLUDE_FROM_ALL tests/test_sound_bank.cpp runtime/src/native/sound_bank.cpp)
    target_include_directories(test_sound_bank PRIVATE runtime/include)
    add_executable(test_adpcm_codec EXCLUDE_FROM_ALL tests/test_adpcm_codec.cpp runtime/src/native/adpcm_codec.cpp)
    target_include_directories(test_adpcm_codec PRIVATE runtime/include)
    add_executable(test_native_content_read EXCLUDE_FROM_ALL tests/test_native_content_read.cpp
        runtime/src/native/content_vfs.cpp runtime/src/native/native_file.cpp)
    target_include_directories(test_native_content_read PRIVATE runtime/include)
    add_executable(test_sample_payload EXCLUDE_FROM_ALL tests/test_sample_payload.cpp
        runtime/src/native/sample_payload.cpp runtime/src/native/native_file.cpp)
    target_include_directories(test_sample_payload PRIVATE runtime/include)
    add_executable(test_gpu_diagnostics EXCLUDE_FROM_ALL tests/test_gpu_diagnostics.cpp)
    target_include_directories(test_gpu_diagnostics PRIVATE runtime/include)
    add_executable(test_vu_aot_only EXCLUDE_FROM_ALL tests/test_vu_aot_only.cpp runtime/src/vu_micro.cpp)
    add_executable(test_native_normal_sample EXCLUDE_FROM_ALL
        tests/test_normal_sample.cpp
        runtime/src/native/normal_sample.cpp
        runtime/src/native/adpcm_codec.cpp
        runtime/src/native/native_file.cpp
        runtime/src/native/sample_payload.cpp
        runtime/src/native/sound_bank.cpp)
    target_include_directories(test_native_normal_sample PRIVATE runtime/include)
    target_include_directories(test_vu_aot_only PRIVATE runtime/include runtime/src)
    add_executable(test_native_fail_closed EXCLUDE_FROM_ALL tests/test_native_fail_closed.cpp)
    target_include_directories(test_native_fail_closed PRIVATE runtime/include runtime/src)
    target_link_libraries(test_native_fail_closed PRIVATE ps2runtime)
    target_link_libraries(test_disc_vfs PRIVATE ps2runtime)
    add_executable(test_content_volume EXCLUDE_FROM_ALL tests/test_content_volume.cpp)
    target_link_libraries(test_content_volume PRIVATE ps2runtime)
    add_executable(test_gs_display EXCLUDE_FROM_ALL tests/test_gs_display.cpp)
    target_include_directories(test_gs_display PRIVATE runtime/src)
    target_link_libraries(test_gs_display PRIVATE ps2runtime)
    add_executable(test_iop_io_overlay EXCLUDE_FROM_ALL tests/test_iop_io_overlay.cpp)
    target_include_directories(test_iop_io_overlay PRIVATE runtime/src)
    target_link_libraries(test_iop_io_overlay PRIVATE ps2runtime)
    add_executable(test_iop_native_module EXCLUDE_FROM_ALL tests/test_iop_native_module.cpp)
    target_include_directories(test_iop_native_module PRIVATE runtime/src)
    target_link_libraries(test_iop_native_module PRIVATE ps2runtime)
    add_executable(test_iop_aot_import_load EXCLUDE_FROM_ALL tests/test_iop_aot_import_load.cpp)
    target_include_directories(test_iop_aot_import_load PRIVATE runtime/src)
    target_link_libraries(test_iop_aot_import_load PRIVATE ps2runtime)
    add_executable(test_padman_rpc EXCLUDE_FROM_ALL tests/test_padman_rpc.cpp)
    add_executable(test_native_controller_input EXCLUDE_FROM_ALL
        tests/test_controller_input.cpp runtime/src/native/controller_input.cpp)
    target_include_directories(test_native_controller_input PRIVATE runtime/include)
    add_executable(test_native_controller_recording EXCLUDE_FROM_ALL tests/test_controller_recording.cpp)
    target_link_libraries(test_native_controller_recording PRIVATE ps2runtime)
    add_executable(test_native_video_producer EXCLUDE_FROM_ALL
        tests/test_video_producer.cpp runtime/src/native/video_producer.cpp)
    target_include_directories(test_native_video_producer PRIVATE runtime/include)
    add_executable(test_native_video_gpu_adapter EXCLUDE_FROM_ALL tests/test_video_gpu_adapter.cpp
        runtime/src/native/video_producer.cpp runtime/src/native/video_gpu_adapter.cpp)
    target_include_directories(test_native_video_gpu_adapter PRIVATE runtime/include)
    add_executable(test_retail_call_observers EXCLUDE_FROM_ALL
        tests/test_retail_call_observers.cpp host/mka_unit_wrappers.cpp)
    target_include_directories(test_retail_call_observers PRIVATE runtime/include host)
    add_executable(test_native_movie_gpu EXCLUDE_FROM_ALL tests/test_movie_gpu.cpp)
    target_include_directories(test_native_movie_gpu PRIVATE runtime/include runtime/src)
    target_link_libraries(test_native_movie_gpu PRIVATE ps2runtime opengl32)
    add_executable(test_native_haptics EXCLUDE_FROM_ALL
        tests/test_haptics.cpp runtime/src/native/haptics.cpp)
    target_include_directories(test_native_haptics PRIVATE runtime/include)
    add_executable(test_native_padman_haptics EXCLUDE_FROM_ALL tests/test_padman_haptics.cpp)
    target_include_directories(test_native_padman_haptics PRIVATE runtime/include runtime/src)
    target_link_libraries(test_native_padman_haptics PRIVATE ps2runtime)
    add_executable(test_native_retail_haptics_bridge EXCLUDE_FROM_ALL
        tests/test_retail_haptics_bridge.cpp
        runtime/src/native/retail_haptics_bridge.cpp
        runtime/src/native/haptics.cpp)
    target_include_directories(test_native_retail_haptics_bridge PRIVATE runtime/include)
    target_include_directories(test_padman_rpc PRIVATE runtime/src)
    target_link_libraries(test_padman_rpc PRIVATE ps2runtime)
    add_executable(test_mwfilefr_read EXCLUDE_FROM_ALL tests/test_mwfilefr_read.cpp)
    target_include_directories(test_mwfilefr_read PRIVATE runtime/src)
    target_link_libraries(test_mwfilefr_read PRIVATE ps2runtime)
    add_executable(test_iop_native_import_link EXCLUDE_FROM_ALL tests/test_iop_native_import_link.cpp)
    target_include_directories(test_iop_native_import_link PRIVATE runtime/src)
    target_link_libraries(test_iop_native_import_link PRIVATE ps2runtime)
    add_executable(test_spu2_mix EXCLUDE_FROM_ALL tests/test_spu2_mix.cpp)
    target_include_directories(test_spu2_mix PRIVATE runtime/src)
    target_link_libraries(test_spu2_mix PRIVATE ps2runtime)
    add_executable(test_mpeg2_stream EXCLUDE_FROM_ALL tests/test_mpeg2_stream.cpp)
    target_include_directories(test_mpeg2_stream PRIVATE runtime/src)
    target_link_libraries(test_mpeg2_stream PRIVATE ps2runtime)
    add_executable(test_mpeg2_decode EXCLUDE_FROM_ALL tests/test_mpeg2_decode.cpp)
    target_include_directories(test_mpeg2_decode PRIVATE runtime/src)
    target_link_libraries(test_mpeg2_decode PRIVATE ps2runtime)
    if(WIN32)
        add_executable(test_native_image_upload EXCLUDE_FROM_ALL tests/test_native_image_upload.cpp)
        target_include_directories(test_native_image_upload PRIVATE runtime/include runtime/src)
        target_link_libraries(test_native_image_upload PRIVATE ps2runtime)
        add_executable(test_native_gl_alpha EXCLUDE_FROM_ALL tests/test_native_gl_alpha.cpp runtime/src/gs/raster.cpp)
        target_compile_definitions(test_native_gl_alpha PRIVATE MKA_REFERENCE_RASTER=1)
        target_include_directories(test_native_gl_alpha PRIVATE runtime/include runtime/src)
        target_link_libraries(test_native_gl_alpha PRIVATE ps2runtime)
        add_executable(test_gl_vs_raster EXCLUDE_FROM_ALL tests/test_gl_vs_raster.cpp runtime/src/gs/raster.cpp)
        target_compile_definitions(test_gl_vs_raster PRIVATE MKA_REFERENCE_RASTER=1)
        target_include_directories(test_gl_vs_raster PRIVATE runtime/include runtime/src)
        target_link_libraries(test_gl_vs_raster PRIVATE ps2runtime)
        add_executable(test_gl_lazy_readback EXCLUDE_FROM_ALL tests/test_gl_lazy_readback.cpp)
        target_include_directories(test_gl_lazy_readback PRIVATE runtime/include runtime/src)
        target_link_libraries(test_gl_lazy_readback PRIVATE ps2runtime)
    endif()
    # These tests use assert for both checks and calls under test. Keep those
    # expressions active even when the surrounding build is Release.
    foreach(assert_test test_iop_native_module test_iop_native_import_link test_iop_aot_import_load)
        if(MSVC)
            target_compile_options(${assert_test} PRIVATE /UNDEBUG)
        else()
            target_compile_options(${assert_test} PRIVATE -UNDEBUG)
        endif()
    endforeach()
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        target_sources(ps2run PRIVATE host/profile.cpp)
        target_compile_definitions(ps2run PRIVATE PS2_PROFILE)
    elseif(WIN32 AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        target_sources(ps2run PRIVATE host/profile_win.cpp)
        target_compile_definitions(ps2run PRIVATE PS2_PROFILE)
        target_link_libraries(ps2run PRIVATE winmm)
    endif()
    if(WIN32)
        list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/third_party/sdl2")
        set(SDL2_NO_MWINDOWS ON)
    endif()
    # A normal Windows game target must always own a window/input backend.
    # Missing SDL is a configure error, never a silently headless game build.
    if(WIN32)
        find_package(SDL2 REQUIRED)
    else()
        find_package(SDL2 QUIET)
    endif()
    if(SDL2_FOUND AND NOT EMSCRIPTEN)
        target_sources(ps2run PRIVATE host/window.cpp host/keymap.cpp host/stretch.cpp)
        target_compile_definitions(ps2run PRIVATE PS2_WINDOW)
        target_link_libraries(ps2run PRIVATE SDL2::SDL2)
        if(WIN32)
            add_custom_command(TARGET ps2run POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:ps2run>)
            # Keep the per-run log viewer beside ps2run so a visible game launch can open a live tail in its own console window.
            add_custom_command(TARGET ps2run POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/scripts/watch_ps2run_log.ps1" $<TARGET_FILE_DIR:ps2run>)
            # Microsoft's shader compiler for Direct3D 12 (scripts/fetch_dxc.sh): used when the two DLLs sit next to ps2run.exe,
            # FXC otherwise. Not shipped with the source: each PC downloads it.
            foreach(dxc_dll dxcompiler.dll dxil.dll)
                if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/dxc/${dxc_dll}")
                    add_custom_command(TARGET ps2run POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/third_party/dxc/${dxc_dll}" $<TARGET_FILE_DIR:ps2run>)
                endif()
            endforeach()
            # A window (GUI) program, so a double-click or a shortcut opens no console window (-mwindows; SDL2_NO_MWINDOWS above
            # keeps SDL's own package from adding it). The entry point still reaches main() in host/main.cpp, which gives stdout
            # and stderr back: a pipe or file of the parent as it is, else the console of the parent, else NUL.
            set_target_properties(ps2run PROPERTIES WIN32_EXECUTABLE ON)
        endif()
        # the game controller path tested with SDL's virtual controllers, no game needed: ninja -C build/native-<game> ps2padtest
        add_executable(test_native_hook EXCLUDE_FROM_ALL tests/test_native_hook.cpp)
        target_include_directories(test_native_hook PRIVATE runtime/include runtime/src)
        target_link_libraries(test_native_hook PRIVATE ps2runtime)

        add_executable(test_observer_hook EXCLUDE_FROM_ALL tests/test_observer_hook.cpp)
        target_include_directories(test_observer_hook PRIVATE runtime/include runtime/src)
        target_link_libraries(test_observer_hook PRIVATE ps2runtime)

add_executable(ps2padtest EXCLUDE_FROM_ALL host/pad_test.cpp host/window.cpp host/keymap.cpp host/stretch.cpp)
        target_include_directories(ps2padtest PRIVATE runtime/src)
        target_link_libraries(ps2padtest PRIVATE ps2runtime SDL2::SDL2)
    endif()
endif()
