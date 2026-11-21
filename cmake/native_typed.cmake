# The decoder/compiler below runs OFFLINE only. Generated native game objects
# link the real session owner and never link the compiler or guest runtime.
add_library(mka_native_typed_compiler STATIC EXCLUDE_FROM_ALL
    recomp/native_typed_ir.cpp recomp/native_typed_codegen.cpp
    recomp/native_closure_codegen.cpp recomp/r5900.cpp)
target_include_directories(mka_native_typed_compiler PUBLIC recomp)
target_compile_features(mka_native_typed_compiler PUBLIC cxx_std_20)
add_executable(native_typed_recomp EXCLUDE_FROM_ALL recomp/native_typed_main.cpp)
target_link_libraries(native_typed_recomp PRIVATE mka_native_typed_compiler)
add_executable(test_native_typed_codegen EXCLUDE_FROM_ALL tests/test_native_typed_codegen.cpp)
target_link_libraries(test_native_typed_codegen PRIVATE mka_native_typed_compiler)

set(_typed_elf "${CMAKE_CURRENT_SOURCE_DIR}/../../../research/inputs/SLUS_214.10")
set(_typed_inputs
    recomp/native_typed_ir.hpp recomp/native_typed_ir.cpp
    recomp/native_typed_codegen.hpp recomp/native_typed_codegen.cpp
    recomp/native_typed_main.cpp recomp/native_closure_codegen.hpp
    recomp/native_closure_codegen.cpp recomp/r5900.hpp recomp/r5900.cpp)
set(_typed_key_data "")
foreach(_typed_input IN LISTS _typed_inputs)
    file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/${_typed_input}" _typed_hash)
    string(APPEND _typed_key_data "${_typed_input}:${_typed_hash}\n")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/${_typed_input}")
endforeach()
file(SHA256 "${_typed_elf}" _typed_elf_hash)
string(SHA256 _typed_key "${_typed_key_data}${_typed_elf_hash}")
set(_typed_dir "${CMAKE_CURRENT_BINARY_DIR}/native-typed-retail-${_typed_key}")
add_custom_target(mka_native_typed_authenticate
    BYPRODUCTS "${_typed_dir}/native_typed_retail.hpp" "${_typed_dir}/native_typed_retail.cpp"
    COMMAND $<TARGET_FILE:native_typed_recomp> "${_typed_elf}" "${_typed_dir}"
    DEPENDS native_typed_recomp "${_typed_elf}" ${_typed_inputs}
    VERBATIM COMMENT "AOT compile source-verified retail scalar functions to typed native C++")
set_source_files_properties("${_typed_dir}/native_typed_retail.cpp" PROPERTIES GENERATED TRUE)
add_library(mka_native_typed_retail STATIC EXCLUDE_FROM_ALL "${_typed_dir}/native_typed_retail.cpp")
add_dependencies(mka_native_typed_retail mka_native_typed_authenticate)
target_include_directories(mka_native_typed_retail PUBLIC "${_typed_dir}")
target_link_libraries(mka_native_typed_retail PUBLIC mka_native_retail_game_session)
if(TARGET mka_native_retail_startup)
    target_link_libraries(mka_native_retail_startup PUBLIC mka_native_typed_retail)
    add_dependencies(mka_native_retail_startup mka_native_typed_authenticate)
endif()
add_executable(test_native_typed_retail EXCLUDE_FROM_ALL tests/test_native_typed_retail.cpp)
target_link_libraries(test_native_typed_retail PRIVATE mka_native_typed_retail)

add_executable(test_native_retail_task_registry EXCLUDE_FROM_ALL tests/test_retail_task_registry.cpp)
target_link_libraries(test_native_retail_task_registry PRIVATE mka_native_retail_task_registry)
add_executable(test_native_retail_task_execution EXCLUDE_FROM_ALL tests/test_retail_task_execution.cpp)
target_link_libraries(test_native_retail_task_execution PRIVATE mka_native_retail_task_registry)
if(TARGET mka_native_retail_startup)
    add_library(mka_native_retail_initial_task STATIC EXCLUDE_FROM_ALL
        runtime/src/native/retail_initial_task.cpp)
    target_include_directories(mka_native_retail_initial_task PUBLIC runtime/include)
    target_compile_features(mka_native_retail_initial_task PUBLIC cxx_std_20)
    target_link_libraries(mka_native_retail_initial_task PUBLIC
        mka_native_retail_startup mka_native_retail_task_registry)
    add_executable(test_native_retail_initial_task EXCLUDE_FROM_ALL tests/test_retail_initial_task.cpp)
    target_link_libraries(test_native_retail_initial_task PRIVATE mka_native_retail_initial_task)
    # Deliberately has no fixture child definitions. It must remain unlinked
    # until every actual required native child effect exists.
    add_executable(test_native_retail_initial_task_missing_children EXCLUDE_FROM_ALL
        tests/test_retail_initial_task_missing_children.cpp)
    target_link_libraries(test_native_retail_initial_task_missing_children PRIVATE mka_native_retail_initial_task)
endif()
