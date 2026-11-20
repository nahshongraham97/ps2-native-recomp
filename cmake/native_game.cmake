# Native retail game state/caller components. No guest ABI/runtime linkage.
add_library(mka_native_game_actions STATIC EXCLUDE_FROM_ALL
    runtime/src/native/controller_input.cpp runtime/src/native/game_action_input.cpp)
target_include_directories(mka_native_game_actions PUBLIC runtime/include)
target_compile_features(mka_native_game_actions PUBLIC cxx_std_20)
add_executable(test_native_game_action_input EXCLUDE_FROM_ALL tests/test_game_action_input.cpp)
target_link_libraries(test_native_game_action_input PRIVATE mka_native_game_actions)

# Strict source-checked typed retail closure, isolated from the Cpu-generated glob.
add_executable(test_native_closure_codegen EXCLUDE_FROM_ALL
    tests/test_native_closure_codegen.cpp recomp/native_closure_codegen.cpp)
target_include_directories(test_native_closure_codegen PRIVATE recomp)
target_compile_features(test_native_closure_codegen PRIVATE cxx_std_20)
if(WIN32 AND TARGET mka_native_media)
    set(_native_logon_elf "${CMAKE_CURRENT_SOURCE_DIR}/../../../research/inputs/SLUS_214.10")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/recomp/native_closure_codegen.cpp")
    file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/recomp/native_closure_codegen.cpp" _native_logon_source_hash)
    set(_native_logon_dir "${CMAKE_CURRENT_BINARY_DIR}/native-retail-logon-${_native_logon_source_hash}")
    add_custom_command(OUTPUT "${_native_logon_dir}/native_logon.hpp" "${_native_logon_dir}/native_logon.cpp"
        COMMAND $<TARGET_FILE:eerecomp> --native-logon "${_native_logon_elf}" --out "${_native_logon_dir}"
        DEPENDS eerecomp "${_native_logon_elf}" recomp/native_closure_codegen.cpp recomp/native_closure_codegen.hpp
        VERBATIM COMMENT "AOT compile the source-checked native retail LOGON caller")
    add_library(mka_native_logon STATIC EXCLUDE_FROM_ALL "${_native_logon_dir}/native_logon.cpp")
    target_include_directories(mka_native_logon PUBLIC "${_native_logon_dir}" runtime/include)
    target_compile_features(mka_native_logon PUBLIC cxx_std_20)
    target_link_libraries(mka_native_logon PUBLIC mka_native_media mka_native_game_actions)
    add_executable(test_native_logon_runtime EXCLUDE_FROM_ALL tests/test_native_logon_runtime.cpp)
    target_link_libraries(test_native_logon_runtime PRIVATE mka_native_logon)
endif()
