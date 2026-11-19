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
    runtime/src/native/font_atlas.cpp
    runtime/src/native/font_metrics.cpp
    runtime/src/native/font_layout.cpp
    runtime/src/native/movie_assets.cpp
    runtime/src/native/movie_container.cpp
    runtime/src/native/movie_subtitles.cpp
    runtime/src/native/movie_decode.cpp
    runtime/src/native/movie_color.cpp
    runtime/src/native/movie_gpu.cpp
    runtime/src/native/movie_gl_device.cpp)
target_include_directories(mka_native_media PUBLIC runtime/include PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/native-media-headers" "${MKA_NATIVE_MEDIA_HEADERS}")
target_compile_features(mka_native_media PUBLIC cxx_std_20)
target_link_libraries(mka_native_media PRIVATE opengl32 gdi32 user32 bcrypt)

foreach(pair
    "test_native_font_atlas|tests/test_font_atlas.cpp"
    "test_native_font_metrics|tests/test_font_metrics.cpp"
    "test_native_font_layout|tests/test_font_layout.cpp"
    "test_native_movie_color|tests/test_movie_color.cpp"
    "test_native_movie_gpu_layout|tests/test_movie_gpu_layout.cpp"
    "test_native_movie_assets|tests/test_movie_assets.cpp"
    "test_native_movie_gl_device|tests/test_movie_gl_device.cpp"
    "test_native_movie_decode|tests/test_movie_decode.cpp"
    "test_native_movie_pipeline|tests/test_native_movie_pipeline.cpp"
    "test_native_movie_inventory|tests/test_native_movie_inventory.cpp"
    "test_native_movie_container|tests/test_native_movie_container.cpp"
    "test_native_movie_subtitles|tests/test_movie_subtitles.cpp")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 target)
    list(GET parts 1 source)
    add_executable(${target} EXCLUDE_FROM_ALL "${source}")
    target_link_libraries(${target} PRIVATE mka_native_media)
endforeach()
