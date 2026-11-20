# Native sound resources are available without PS2_GENERATED or ps2runtime.
add_library(mka_native_sound_resources STATIC EXCLUDE_FROM_ALL
    runtime/src/native/audio_resource_domain.cpp
    runtime/src/native/native_file.cpp runtime/src/native/sound_bank.cpp
    runtime/src/native/sound_archive.cpp runtime/src/native/sample_payload.cpp
    runtime/src/native/stream_source.cpp runtime/src/native/bank_asset.cpp
    runtime/src/native/normal_sample.cpp runtime/src/native/adpcm_codec.cpp
    runtime/src/native/bank_preparation.cpp runtime/src/native/audio_banks.cpp
    runtime/src/native/audio_bank_protocol.cpp runtime/src/native/audio_bank_service.cpp
    runtime/src/native/program_executor.cpp runtime/src/native/audio_controls.cpp
    runtime/src/native/host_voice.cpp runtime/src/native/playback_owner.cpp)
target_include_directories(mka_native_sound_resources PUBLIC runtime/include)
target_compile_features(mka_native_sound_resources PUBLIC cxx_std_20)
add_executable(test_sound_archive EXCLUDE_FROM_ALL tests/test_sound_archive.cpp)
target_link_libraries(test_sound_archive PRIVATE mka_native_sound_resources)
add_executable(test_bank_asset EXCLUDE_FROM_ALL tests/test_bank_asset.cpp)
target_link_libraries(test_bank_asset PRIVATE mka_native_sound_resources)
add_executable(test_native_audio_banks EXCLUDE_FROM_ALL tests/test_audio_banks.cpp)
target_link_libraries(test_native_audio_banks PRIVATE mka_native_sound_resources)
add_executable(test_native_audio_bank_protocol EXCLUDE_FROM_ALL tests/test_audio_bank_protocol.cpp)
target_link_libraries(test_native_audio_bank_protocol PRIVATE mka_native_sound_resources)
add_executable(test_native_bank_preparation EXCLUDE_FROM_ALL tests/test_bank_preparation.cpp)
target_link_libraries(test_native_bank_preparation PRIVATE mka_native_sound_resources)
add_executable(test_native_audio_bank_service EXCLUDE_FROM_ALL tests/test_audio_bank_service.cpp)
target_link_libraries(test_native_audio_bank_service PRIVATE mka_native_sound_resources)
add_executable(test_native_audio_resource_domain EXCLUDE_FROM_ALL tests/test_audio_resource_domain.cpp)
target_link_libraries(test_native_audio_resource_domain PRIVATE mka_native_sound_resources)
add_executable(test_native_program_executor EXCLUDE_FROM_ALL tests/test_program_executor.cpp)
target_link_libraries(test_native_program_executor PRIVATE mka_native_sound_resources)
add_executable(test_native_host_voice EXCLUDE_FROM_ALL tests/test_host_voice.cpp)
target_link_libraries(test_native_host_voice PRIVATE mka_native_sound_resources)
add_executable(test_native_playback_owner EXCLUDE_FROM_ALL tests/test_playback_owner.cpp)
target_link_libraries(test_native_playback_owner PRIVATE mka_native_sound_resources)
if(WIN32)
    add_executable(test_native_stream_source EXCLUDE_FROM_ALL tests/test_stream_source.cpp)
    target_link_libraries(test_native_stream_source PRIVATE mka_native_sound_resources)
    target_link_libraries(test_native_stream_source PRIVATE bcrypt)
endif()
# Unexpected native preparation exceptions are injected only in this test.
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_executable(test_native_audio_resource_exception EXCLUDE_FROM_ALL tests/test_audio_resource_exception.cpp)
    target_link_libraries(test_native_audio_resource_exception PRIVATE mka_native_sound_resources)
    target_link_options(test_native_audio_resource_exception PRIVATE
        "-Wl,--wrap=_ZN3mka5sound12PreparedBank7prepareENS0_15BankAssetSourceERNS0_21BankPreparationReportERNS0_18PreparationFailureE")
endif()
