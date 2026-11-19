# Native host services: no ps2runtime, console device or generated-game link.
if(NOT WIN32)
    return()
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Native media currently requires Windows64")
endif()
set(MKA_NATIVE_MEDIA_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/../../../../dependencies/native-media/ffmpeg-7.1.4" CACHE PATH "Pinned FFmpeg7.1.4 public header root")
foreach(header libavcodec/avcodec.h libavformat/avformat.h libavutil/avutil.h)
    if(NOT EXISTS "${MKA_NATIVE_MEDIA_HEADERS}/${header}")
        message(FATAL_ERROR "Native media requires pinned FFmpeg7.1.4 headers: ${MKA_NATIVE_MEDIA_HEADERS}/${header}")
    endif()
endforeach()
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/native-media-headers/libavutil")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/native-media-headers/libavutil/avconfig.h"
    "#ifndef AVUTIL_AVCONFIG_H\n#define AVUTIL_AVCONFIG_H\n#define AV_HAVE_BIGENDIAN 0\n#define AV_HAVE_FAST_UNALIGNED 1\n#endif\n")

add_library(mka_native_media STATIC EXCLUDE_FROM_ALL
    runtime/src/native/native_file.cpp
    runtime/src/native/native_user_paths.cpp
    runtime/src/native/font_atlas.cpp
    runtime/src/native/font_metrics.cpp
    runtime/src/native/font_layout.cpp
    runtime/src/native/font_geometry.cpp
    runtime/src/native/movie_assets.cpp
    runtime/src/native/movie_container.cpp
    runtime/src/native/movie_pes_index.cpp
    runtime/src/native/movie_metadata.cpp
    runtime/src/native/mpeg2_timing.cpp
    runtime/src/native/movie_timecode.cpp
    runtime/src/native/movie_clock_history.cpp
    runtime/src/native/movie_clock_math.cpp
    runtime/src/native/movie_control.cpp
    runtime/src/native/movie_catalog.cpp
    runtime/src/native/movie_source.cpp
    runtime/src/native/movie_gain.cpp
    runtime/src/native/movie_subtitles.cpp
    runtime/src/native/movie_decode.cpp
    runtime/src/native/movie_color.cpp
    runtime/src/native/movie_gpu.cpp
    runtime/src/native/movie_gl_device.cpp)
target_include_directories(mka_native_media PUBLIC runtime/include PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/native-media-headers" "${MKA_NATIVE_MEDIA_HEADERS}")
target_compile_features(mka_native_media PUBLIC cxx_std_20)
target_link_libraries(mka_native_media PRIVATE opengl32 gdi32 user32 bcrypt shell32 ole32 uuid)
# Source font math uses ordered float32 steps and finite-value checks. These
# modules must not inherit fast-math/reassociation or fused multiply-add from
# a caller's global build flags. This does not modify the host FP environment.
if(MSVC)
    set_property(SOURCE runtime/src/native/font_metrics.cpp
        runtime/src/native/font_layout.cpp runtime/src/native/font_geometry.cpp
        runtime/src/native/movie_timecode.cpp runtime/src/native/movie_control.cpp
        APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set_property(SOURCE runtime/src/native/font_metrics.cpp
        runtime/src/native/font_layout.cpp runtime/src/native/font_geometry.cpp
        runtime/src/native/movie_timecode.cpp runtime/src/native/movie_control.cpp
        APPEND PROPERTY COMPILE_OPTIONS -fno-fast-math -ffp-contract=off)
else()
    message(FATAL_ERROR "Native font math requires a verified strict float32 compiler configuration")
endif()

foreach(pair
    "test_native_font_atlas|tests/test_font_atlas.cpp"
    "test_native_font_metrics|tests/test_font_metrics.cpp"
    "test_native_font_layout|tests/test_font_layout.cpp"
    "test_native_font_geometry|tests/test_font_geometry.cpp"
    "test_native_movie_color|tests/test_movie_color.cpp"
    "test_native_movie_gpu_layout|tests/test_movie_gpu_layout.cpp"
    "test_native_movie_assets|tests/test_movie_assets.cpp"
    "test_native_movie_gl_device|tests/test_movie_gl_device.cpp"
    "test_native_movie_decode|tests/test_movie_decode.cpp"
    "test_native_movie_source|tests/test_movie_source.cpp"
    "test_native_movie_source_failure|tests/test_movie_source_failure.cpp"
    "test_native_movie_pipeline|tests/test_native_movie_pipeline.cpp"
    "test_native_movie_inventory|tests/test_native_movie_inventory.cpp"
    "test_native_movie_source_association|tests/test_movie_source_association.cpp"
    "test_native_movie_container|tests/test_native_movie_container.cpp"
    "test_native_movie_pes_index|tests/test_movie_pes_index.cpp"
    "test_native_movie_metadata|tests/test_movie_metadata.cpp"
    "test_native_movie_subtitles|tests/test_movie_subtitles.cpp")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 target)
    list(GET parts 1 source)
    add_executable(${target} EXCLUDE_FROM_ALL "${source}")
    target_link_libraries(${target} PRIVATE mka_native_media)
endforeach()

# Pure serialized-header fixture: no codec libraries, renderer or runtime link.
add_executable(test_native_mpeg2_timing EXCLUDE_FROM_ALL
    tests/test_mpeg2_timing.cpp runtime/src/native/mpeg2_timing.cpp)
target_include_directories(test_native_mpeg2_timing PRIVATE runtime/include)
target_compile_features(test_native_mpeg2_timing PRIVATE cxx_std_20)
add_executable(test_native_movie_timecode EXCLUDE_FROM_ALL
    tests/test_movie_timecode.cpp runtime/src/native/movie_timecode.cpp)
target_include_directories(test_native_movie_timecode PRIVATE runtime/include)
target_compile_features(test_native_movie_timecode PRIVATE cxx_std_20)

add_executable(test_native_movie_control EXCLUDE_FROM_ALL
    tests/test_movie_control.cpp runtime/src/native/movie_control.cpp)
target_include_directories(test_native_movie_control PRIVATE runtime/include)
target_compile_features(test_native_movie_control PRIVATE cxx_std_20)

add_executable(test_native_movie_gain EXCLUDE_FROM_ALL
    tests/test_movie_gain.cpp runtime/src/native/movie_gain.cpp)
target_include_directories(test_native_movie_gain PRIVATE runtime/include)
target_compile_features(test_native_movie_gain PRIVATE cxx_std_20)

add_executable(test_native_movie_gain_domain EXCLUDE_FROM_ALL
    tests/test_movie_gain_domain.cpp runtime/src/native/movie_gain.cpp)
target_include_directories(test_native_movie_gain_domain PRIVATE runtime/include)
target_compile_features(test_native_movie_gain_domain PRIVATE cxx_std_20)

# Native request/file integration only; no player or device is constructed.
add_executable(test_native_movie_catalog_assets EXCLUDE_FROM_ALL
    tests/test_movie_catalog_assets.cpp)
target_link_libraries(test_native_movie_catalog_assets PRIVATE mka_native_media)
target_compile_features(test_native_movie_catalog_assets PRIVATE cxx_std_20)

# Semantic owned requests only: no codecs, renderer, assets or game runtime.
add_executable(test_native_movie_catalog EXCLUDE_FROM_ALL
    tests/test_movie_catalog.cpp runtime/src/native/movie_catalog.cpp)
target_include_directories(test_native_movie_catalog PRIVATE runtime/include)
target_compile_features(test_native_movie_catalog PRIVATE cxx_std_20)

# Native platform paths: no media codecs, runtime or generated game dependency.
add_executable(test_native_user_paths EXCLUDE_FROM_ALL
    tests/test_native_user_paths.cpp runtime/src/native/native_user_paths.cpp)
target_include_directories(test_native_user_paths PRIVATE runtime/include)
target_compile_features(test_native_user_paths PRIVATE cxx_std_20)
target_link_libraries(test_native_user_paths PRIVATE shell32 ole32 uuid)

# Pure native residual math; no media/device/runtime linkage.
add_executable(test_native_movie_clock_history EXCLUDE_FROM_ALL
    tests/test_movie_clock_history.cpp runtime/src/native/movie_clock_history.cpp)
target_include_directories(test_native_movie_clock_history PRIVATE runtime/include)
target_compile_features(test_native_movie_clock_history PRIVATE cxx_std_20)

# Pure native rational arithmetic; no media/device/runtime linkage.
add_executable(test_native_movie_clock_math EXCLUDE_FROM_ALL
    tests/test_movie_clock_math.cpp runtime/src/native/movie_clock_math.cpp)
target_include_directories(test_native_movie_clock_math PRIVATE runtime/include)
target_compile_features(test_native_movie_clock_math PRIVATE cxx_std_20)

# Pure owned-history staging; no media/device/runtime linkage.
add_executable(test_native_movie_clock_history_clone EXCLUDE_FROM_ALL
    tests/test_movie_clock_history_clone.cpp runtime/src/native/movie_clock_history.cpp)
target_include_directories(test_native_movie_clock_history_clone PRIVATE runtime/include)
target_compile_features(test_native_movie_clock_history_clone PRIVATE cxx_std_20)
