# Native sound resources are available without PS2_GENERATED or ps2runtime.
add_library(mka_native_sound_resources STATIC EXCLUDE_FROM_ALL
    runtime/src/native/audio_resource_domain.cpp
    runtime/src/native/native_file.cpp runtime/src/native/sound_bank.cpp
    runtime/src/native/sound_archive.cpp runtime/src/native/sample_payload.cpp
    runtime/src/native/stream_source.cpp runtime/src/native/bank_asset.cpp
    runtime/src/native/normal_sample.cpp runtime/src/native/adpcm_codec.cpp
    runtime/src/native/bank_preparation.cpp runtime/src/native/audio_banks.cpp
    runtime/src/native/audio_bank_protocol.cpp runtime/src/native/audio_bank_service.cpp
    runtime/src/native/program_executor.cpp runtime/src/native/program_copy.cpp runtime/src/native/audio_controls.cpp
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

# Actual native playback/resource ownership only; no device or game linkage.
add_executable(test_native_playback_reservation EXCLUDE_FROM_ALL tests/test_playback_reservation.cpp)
target_link_libraries(test_native_playback_reservation PRIVATE mka_native_sound_resources)
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_executable(test_native_playback_reservation_gaps EXCLUDE_FROM_ALL tests/test_playback_reservation_gaps.cpp)
    target_link_libraries(test_native_playback_reservation_gaps PRIVATE mka_native_sound_resources)
    foreach(symbol
        _ZN3mka5sound16AudioBankService23acquire_runtime_programERKNS0_16AudioBankCommandERNS0_18PreparationFailureERNS0_18BankServiceFailureE
        _ZN3mka5sound16AudioBankService6unloadERKNS0_16AudioBankCommandERNS0_18BankServiceFailureE
        _ZN3mka5sound16AudioBankService15close_when_idleERNS0_18BankServiceFailureE
        _ZN3mka5sound16AudioBankService9load_syncERKNS0_16AudioBankCommandENS0_13BankWireTokenERNS0_18BankServiceFailureE
        _ZN3mka5sound16AudioBankService7run_oneERNS0_18BankServiceFailureE)
        target_link_options(test_native_playback_reservation_gaps PRIVATE "-Wl,--wrap=${symbol}")
    endforeach()
endif()

# Actual native fresh-work/cache ownership, no hardware/runtime linkage.
add_executable(test_native_deferred_program_work EXCLUDE_FROM_ALL tests/test_deferred_program_work.cpp)
target_link_libraries(test_native_deferred_program_work PRIVATE mka_native_sound_resources)
add_executable(test_native_deferred_cache_policy EXCLUDE_FROM_ALL tests/test_deferred_cache_policy.cpp)
target_link_libraries(test_native_deferred_cache_policy PRIVATE mka_native_sound_resources)

# Actual Service-owned deferred work; CPU/files only.
add_executable(test_native_program_admission EXCLUDE_FROM_ALL tests/test_program_admission.cpp)
target_link_libraries(test_native_program_admission PRIVATE mka_native_sound_resources)

if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_executable(test_native_program_admission_gates EXCLUDE_FROM_ALL tests/test_program_admission_gates.cpp)
    target_link_libraries(test_native_program_admission_gates PRIVATE mka_native_sound_resources)
    target_link_options(test_native_program_admission_gates PRIVATE
        "-Wl,--wrap=_ZN3mka5sound17AudioBankProtocol6unloadERKNS0_16AudioBankCommandERNS0_20AudioProtocolFailureE")
endif()

# Actual bank-owned runtime preparation/cache; no device/runtime linkage.
add_executable(test_native_runtime_preparation EXCLUDE_FROM_ALL tests/test_runtime_preparation.cpp)
target_link_libraries(test_native_runtime_preparation PRIVATE mka_native_sound_resources)

add_executable(test_native_program_copy EXCLUDE_FROM_ALL tests/test_program_copy.cpp)
target_link_libraries(test_native_program_copy PRIVATE mka_native_sound_resources)

if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_executable(test_native_program_copy_failure EXCLUDE_FROM_ALL tests/test_program_copy_failure.cpp)
    target_link_libraries(test_native_program_copy_failure PRIVATE mka_native_sound_resources)
    target_link_options(test_native_program_copy_failure PRIVATE "-Wl,--wrap=_Znwy")
endif()

add_executable(test_native_service_runtime EXCLUDE_FROM_ALL tests/test_service_runtime.cpp)
target_link_libraries(test_native_service_runtime PRIVATE mka_native_sound_resources)
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_executable(test_native_service_runtime_gaps EXCLUDE_FROM_ALL tests/test_service_runtime_gaps.cpp)
    target_link_libraries(test_native_service_runtime_gaps PRIVATE mka_native_sound_resources)
    target_link_options(test_native_service_runtime_gaps PRIVATE
        "-Wl,--wrap=_ZN3mka5sound17AudioBankProtocol23acquire_runtime_programERKNS0_16AudioBankCommandERNS0_18PreparationFailureERNS0_20AudioProtocolFailureE")
endif()

# Default native Owner -> runtime reference -> private copy/voice lifecycle.
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_executable(test_native_default_playback_runtime EXCLUDE_FROM_ALL tests/test_default_playback_runtime.cpp)
    target_link_libraries(test_native_default_playback_runtime PRIVATE mka_native_sound_resources)
    target_link_options(test_native_default_playback_runtime PRIVATE
        "-Wl,--wrap=_ZN3mka5sound17AudioBankProtocol6unloadERKNS0_16AudioBankCommandERNS0_20AudioProtocolFailureE")
endif()
