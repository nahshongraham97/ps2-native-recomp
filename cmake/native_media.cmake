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
    runtime/src/native/movie_clock_filter.cpp
    runtime/src/native/movie_host_clock.cpp
    runtime/src/native/movie_service_clock.cpp
    runtime/src/native/movie_eligibility_numeric.cpp
    runtime/src/native/movie_frame_eligibility.cpp
    runtime/src/native/movie_control.cpp
    runtime/src/native/movie_playback.cpp
    runtime/src/native/movie_playback_adapters.cpp
    runtime/src/native/movie_playback_session.cpp
    runtime/src/native/movie_native_context.cpp
    runtime/src/native/movie_output_position.cpp
    runtime/src/native/movie_service.cpp
    runtime/src/native/movie_backend_driver.cpp
    runtime/src/native/movie_frame_work.cpp
    runtime/src/native/movie_audio_clock_anchor.cpp
    runtime/src/native/movie_descriptor_rewrite.cpp
    runtime/src/native/movie_timestamp_association.cpp
    runtime/src/native/movie_service_host_clock.cpp
    runtime/src/native/movie_service_acquisition.cpp
    runtime/src/native/movie_admission_feed.cpp
    runtime/src/native/movie_audio_progress.cpp
    runtime/src/native/movie_pcm_mix.cpp
    runtime/src/native/movie_pcm_downmix.cpp
    runtime/src/native/movie_adx_blocks.cpp
    runtime/src/native/movie_adx_header.cpp
    runtime/src/native/movie_adx_stream.cpp
    runtime/src/native/movie_adx_profile.cpp
    runtime/src/native/movie_decoder_mode.cpp
    runtime/src/native/movie_prefix_events.cpp
    runtime/src/native/movie_header_boundary.cpp
    runtime/src/native/movie_header_unit.cpp
    runtime/src/native/movie_userdata_retention.cpp
    runtime/src/native/movie_source_syntax_index.cpp
    runtime/src/native/movie_pcm_binding.cpp
    runtime/src/native/movie_kernel_policies.cpp
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
target_link_libraries(mka_native_media PRIVATE mka_native_audio_device)
# Source font math uses ordered float32 steps and finite-value checks. These
# modules must not inherit fast-math/reassociation or fused multiply-add from
# a caller's global build flags. This does not modify the host FP environment.
if(MSVC)
    set_property(SOURCE runtime/src/native/font_metrics.cpp
        runtime/src/native/font_layout.cpp runtime/src/native/font_geometry.cpp
        runtime/src/native/movie_timecode.cpp runtime/src/native/movie_control.cpp
        runtime/src/native/movie_playback.cpp
        APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set_property(SOURCE runtime/src/native/font_metrics.cpp
        runtime/src/native/font_layout.cpp runtime/src/native/font_geometry.cpp
        runtime/src/native/movie_timecode.cpp runtime/src/native/movie_control.cpp
        runtime/src/native/movie_playback.cpp
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
    "test_native_movie_decode_adx|tests/test_movie_decode_adx.cpp"
    "test_native_movie_program_end|tests/test_movie_program_end.cpp"
    "test_native_movie_source_observations|tests/test_movie_source_observations.cpp"
    "test_native_movie_header_unit_corpus|tests/test_movie_header_unit_corpus.cpp"
    "test_native_movie_userdata_retention_corpus|tests/test_movie_userdata_retention_corpus.cpp"
    "test_native_movie_source_syntax_index|tests/test_movie_source_syntax_index.cpp"
    "test_native_movie_source_syntax_corpus|tests/test_movie_source_syntax_corpus.cpp"
    "test_native_movie_playback_session|tests/test_movie_playback_session.cpp"
    "test_native_movie_session_native_audio|tests/test_movie_session_native_audio.cpp"
    "test_native_movie_native_context|tests/test_movie_native_context.cpp"
    "test_native_movie_output_position|tests/test_movie_output_position.cpp"
    "test_native_movie_service|tests/test_movie_service.cpp"
    "test_native_movie_service_acquisition|tests/test_movie_service_acquisition.cpp"
    "test_native_movie_host_clock|tests/test_movie_host_clock.cpp"
    "test_native_movie_accepted_body|tests/test_movie_accepted_body.cpp"
    "test_native_movie_frame_work|tests/test_movie_frame_work.cpp"
    "test_native_movie_audio_clock_anchor|tests/test_movie_audio_clock_anchor.cpp"
    "test_native_movie_descriptor_rewrite|tests/test_movie_descriptor_rewrite.cpp"
    "test_native_movie_timestamp_association|tests/test_movie_timestamp_association.cpp"
    "test_native_movie_outer_header_batch|tests/test_movie_outer_header_batch.cpp"
    "test_native_movie_service_host_clock|tests/test_movie_service_host_clock.cpp"
    "test_native_movie_input_acceptance|tests/test_movie_input_acceptance.cpp"
    "test_native_movie_input_acceptance_consumers|tests/test_movie_input_acceptance_consumers.cpp"
    "test_native_movie_admission_feed|tests/test_movie_admission_feed.cpp"
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

# Independent public-library demux observations; no project media/runtime link.
add_executable(test_native_movie_audio_demux_probe EXCLUDE_FROM_ALL
    tests/test_movie_audio_demux_probe.cpp)
target_include_directories(test_native_movie_audio_demux_probe PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/native-media-headers" "${MKA_NATIVE_MEDIA_HEADERS}")
target_compile_features(test_native_movie_audio_demux_probe PRIVATE cxx_std_20)
target_link_libraries(test_native_movie_audio_demux_probe PRIVATE bcrypt)

# Pure mutable game/content policy data, without runtime or device linkage.
add_executable(test_native_movie_userdata_retention EXCLUDE_FROM_ALL
    tests/test_movie_userdata_retention.cpp runtime/src/native/movie_userdata_retention.cpp
    runtime/src/native/movie_header_unit.cpp)
target_include_directories(test_native_movie_userdata_retention PRIVATE runtime/include)
target_compile_features(test_native_movie_userdata_retention PRIVATE cxx_std_20)

add_executable(test_native_movie_header_unit EXCLUDE_FROM_ALL
    tests/test_movie_header_unit.cpp runtime/src/native/movie_header_unit.cpp)
target_include_directories(test_native_movie_header_unit PRIVATE runtime/include)
target_compile_features(test_native_movie_header_unit PRIVATE cxx_std_20)

add_executable(test_native_movie_header_boundary EXCLUDE_FROM_ALL
    tests/test_movie_header_boundary.cpp runtime/src/native/movie_header_boundary.cpp
    runtime/src/native/movie_prefix_events.cpp)
target_include_directories(test_native_movie_header_boundary PRIVATE runtime/include)
target_compile_features(test_native_movie_header_boundary PRIVATE cxx_std_20)

add_executable(test_native_movie_prefix_events EXCLUDE_FROM_ALL
    tests/test_movie_prefix_events.cpp runtime/src/native/movie_prefix_events.cpp)
target_include_directories(test_native_movie_prefix_events PRIVATE runtime/include)
target_compile_features(test_native_movie_prefix_events PRIVATE cxx_std_20)

add_executable(test_native_movie_decoder_mode EXCLUDE_FROM_ALL
    tests/test_movie_decoder_mode.cpp runtime/src/native/movie_decoder_mode.cpp)
target_include_directories(test_native_movie_decoder_mode PRIVATE runtime/include)
target_compile_features(test_native_movie_decoder_mode PRIVATE cxx_std_20)

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

# Pure filter algorithm and owned history; no media/device/runtime linkage.
add_executable(test_native_movie_clock_filter EXCLUDE_FROM_ALL
    tests/test_movie_clock_filter.cpp runtime/src/native/movie_clock_filter.cpp
    runtime/src/native/movie_clock_history.cpp)
target_include_directories(test_native_movie_clock_filter PRIVATE runtime/include)
target_compile_features(test_native_movie_clock_filter PRIVATE cxx_std_20)

# Atomic scalar service clock; no media/device/runtime linkage.
add_executable(test_native_movie_service_clock EXCLUDE_FROM_ALL
    tests/test_movie_service_clock.cpp runtime/src/native/movie_service_clock.cpp)
target_include_directories(test_native_movie_service_clock PRIVATE runtime/include)
target_compile_features(test_native_movie_service_clock PRIVATE cxx_std_20)

# Function-specific pure business arithmetic; no offline oracle/runtime linkage.
add_executable(test_native_movie_eligibility_numeric EXCLUDE_FROM_ALL
    tests/test_movie_eligibility_numeric.cpp runtime/src/native/movie_eligibility_numeric.cpp)
target_include_directories(test_native_movie_eligibility_numeric PRIVATE runtime/include)
target_compile_features(test_native_movie_eligibility_numeric PRIVATE cxx_std_20)

# Deterministic stream ownership/lifecycle only: fake endpoints, original color
# table data. Does not link codecs, audio devices or the game/runtime.
add_executable(test_native_movie_playback EXCLUDE_FROM_ALL
    tests/test_movie_playback.cpp runtime/src/native/movie_playback.cpp
    runtime/src/native/movie_audio_progress.cpp
    runtime/src/native/movie_audio_clock_anchor.cpp
    runtime/src/native/movie_control.cpp runtime/src/native/movie_gain.cpp
    runtime/src/native/movie_color.cpp runtime/src/native/movie_gpu.cpp
    runtime/src/native/movie_catalog.cpp)
target_include_directories(test_native_movie_playback PRIVATE runtime/include)
target_compile_features(test_native_movie_playback PRIVATE cxx_std_20)
target_link_libraries(test_native_movie_playback PRIVATE bcrypt)

# Deliberate zero-only component fixture. Explicit source/codec/endpoint data;
# actual native adapters, never game or retail audible PCM playback.
add_executable(test_native_movie_playback_adapters EXCLUDE_FROM_ALL
    tests/test_movie_playback_adapters.cpp)
target_link_libraries(test_native_movie_playback_adapters PRIVATE mka_native_media bcrypt ole32 uuid)
target_compile_features(test_native_movie_playback_adapters PRIVATE cxx_std_20)

# Identity-bound native client-buffer retirement only, no device or player link.
add_executable(test_native_movie_audio_progress EXCLUDE_FROM_ALL
    tests/test_movie_audio_progress.cpp runtime/src/native/movie_audio_progress.cpp)
target_include_directories(test_native_movie_audio_progress PRIVATE runtime/include)
target_compile_features(test_native_movie_audio_progress PRIVATE cxx_std_20)

# Ordinary available-clock direct/adaptive game policy only; no clock producer,
# decoder, device, runtime or outer control/retained-clock wrapper linkage.
add_executable(test_native_movie_frame_eligibility EXCLUDE_FROM_ALL
    tests/test_movie_frame_eligibility.cpp runtime/src/native/movie_frame_eligibility.cpp
    runtime/src/native/movie_eligibility_numeric.cpp)
target_include_directories(test_native_movie_frame_eligibility PRIVATE runtime/include)
target_compile_features(test_native_movie_frame_eligibility PRIVATE cxx_std_20)

# Explicit stateless PCM business arithmetic and compiled original lookup data.
# No codec/device/game/runtime linkage or implicit speaker/profile selection.
add_executable(test_native_movie_pcm_mix EXCLUDE_FROM_ALL
    tests/test_movie_pcm_mix.cpp runtime/src/native/movie_pcm_mix.cpp)
target_include_directories(test_native_movie_pcm_mix PRIVATE runtime/include)
target_compile_features(test_native_movie_pcm_mix PRIVATE cxx_std_20)

# Concrete immutable PCM and ordinary-policy composition; isolated native only.
add_executable(test_native_movie_pcm_binding EXCLUDE_FROM_ALL tests/test_movie_pcm_binding.cpp)
target_link_libraries(test_native_movie_pcm_binding PRIVATE mka_native_media)
add_executable(test_native_movie_kernel_policies EXCLUDE_FROM_ALL tests/test_movie_kernel_policies.cpp)
target_link_libraries(test_native_movie_kernel_policies PRIVATE mka_native_media)

# Plain decoded PCM business transform only; no runtime/media/device link.
add_executable(test_native_movie_pcm_downmix EXCLUDE_FROM_ALL
    tests/test_movie_pcm_downmix.cpp runtime/src/native/movie_pcm_downmix.cpp)
target_include_directories(test_native_movie_pcm_downmix PRIVATE runtime/include)
target_compile_features(test_native_movie_pcm_downmix PRIVATE cxx_std_20)

# Native encoded asset codec only; no backup/reference-helper/runtime link.
add_executable(test_native_movie_adx_blocks EXCLUDE_FROM_ALL
    tests/test_movie_adx_blocks.cpp runtime/src/native/movie_adx_blocks.cpp)
target_include_directories(test_native_movie_adx_blocks PRIVATE runtime/include)
target_compile_features(test_native_movie_adx_blocks PRIVATE cxx_std_20)

# Owned native ADX header/stream/profile fixtures; no codec/reference/device link.
add_executable(test_native_movie_adx_header EXCLUDE_FROM_ALL
    tests/test_movie_adx_header.cpp runtime/src/native/movie_adx_header.cpp)
add_executable(test_native_movie_adx_header_assets EXCLUDE_FROM_ALL
    tests/test_movie_adx_header_assets.cpp runtime/src/native/movie_adx_header.cpp)
add_executable(test_native_movie_adx_stream EXCLUDE_FROM_ALL
    tests/test_movie_adx_stream.cpp runtime/src/native/movie_adx_stream.cpp
    runtime/src/native/movie_adx_header.cpp runtime/src/native/movie_adx_blocks.cpp)
add_executable(test_native_movie_adx_profile EXCLUDE_FROM_ALL
    tests/test_movie_adx_profile.cpp runtime/src/native/movie_adx_profile.cpp
    runtime/src/native/movie_adx_stream.cpp runtime/src/native/movie_adx_header.cpp
    runtime/src/native/movie_adx_blocks.cpp)
foreach(target test_native_movie_adx_header test_native_movie_adx_header_assets
    test_native_movie_adx_stream test_native_movie_adx_profile)
    target_include_directories(${target} PRIVATE runtime/include)
    target_compile_features(${target} PRIVATE cxx_std_20)
endforeach()
add_executable(test_native_movie_audio_pes_index EXCLUDE_FROM_ALL tests/test_movie_audio_pes_index.cpp)
target_link_libraries(test_native_movie_audio_pes_index PRIVATE mka_native_media)

add_executable(test_native_movie_adx_audio_assets EXCLUDE_FROM_ALL tests/test_movie_adx_audio_assets.cpp)
target_link_libraries(test_native_movie_adx_audio_assets PRIVATE mka_native_media)

# Actual service-owned orchestration with deterministic output/clock ports.
add_executable(test_native_movie_service_backend EXCLUDE_FROM_ALL tests/test_movie_service_backend.cpp)
target_link_libraries(test_native_movie_service_backend PRIVATE mka_native_media)
