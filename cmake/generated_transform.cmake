# Phase 4's pinned generation pipeline supplies the untracked C++ inputs.
# This dependency performs the actual offline C++ AST -> typed native transform
# during the build. The emitted library has no dependency on ps2runtime or Cpu.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(_overlay_transform_dir "${CMAKE_CURRENT_BINARY_DIR}/native-overlay-initializers")
set(_overlay_transform_script "${CMAKE_SOURCE_DIR}/tools/research/transform_generated_units.py")
set(_overlay_transform_contract "${CMAKE_SOURCE_DIR}/recomp/overlay_initializer_contract.json")
set(_overlay_generated_dir "${CMAKE_SOURCE_DIR}/build/generated-aot/phase4-all")
file(GLOB_RECURSE _overlay_ast_headers CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/runtime/include/*.hpp")

# Separate offline relative-receiver prototype. This adds no production caller
# or source-layout compatibility struct; the native owner is one signed field.
set(_index_transform_dir "${CMAKE_CURRENT_BINARY_DIR}/native-index-selection")
set(_index_transform_contract "${CMAKE_SOURCE_DIR}/recomp/index_selection_contract.json")
add_custom_command(
    OUTPUT "${_index_transform_dir}/native_index_selection.cpp"
           "${_index_transform_dir}/native_index_selection.hpp"
           "${_index_transform_dir}/index_transform_proof.json"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_index_transform_dir}"
    COMMAND "${Python3_EXECUTABLE}" -B "${_overlay_transform_script}"
        --compiler "${CMAKE_CXX_COMPILER}" --generated-dir "${_overlay_generated_dir}"
        --contract "${_index_transform_contract}" --out-dir "${_index_transform_dir}"
        --self-test --replace-generated
    DEPENDS "${_overlay_transform_script}" "${_index_transform_contract}"
        "${_overlay_generated_dir}/unit_0013.cpp" "${_overlay_generated_dir}/units.hpp" ${_overlay_ast_headers}
    VERBATIM COMMENT "Transform original generated signed-index leaf to typed native field")
add_library(mka_transformed_index_selection STATIC EXCLUDE_FROM_ALL "${_index_transform_dir}/native_index_selection.cpp")
target_include_directories(mka_transformed_index_selection PUBLIC "${_index_transform_dir}")
target_compile_features(mka_transformed_index_selection PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(mka_transformed_index_selection PRIVATE /W4 /WX)
else()
    target_compile_options(mka_transformed_index_selection PRIVATE -Wall -Wextra -Werror)
endif()
if(TARGET ps2_phase4_aot)
    add_executable(test_phase5_index_transform EXCLUDE_FROM_ALL
        tests/test_phase5_index_transform.cpp
        "${CMAKE_SOURCE_DIR}/build/generated-aot/vu1/vu1_table.cpp"
        runtime/src/dispatch.cpp runtime/src/observer_hook.cpp runtime/src/native_hook.cpp
        runtime/src/cop.cpp runtime/src/vu_micro.cpp)
    target_include_directories(test_phase5_index_transform PRIVATE runtime/include runtime/src)
    target_link_libraries(test_phase5_index_transform PRIVATE ps2_phase4_aot mka_transformed_index_selection)
endif()
add_custom_command(
    OUTPUT "${_overlay_transform_dir}/native_overlay_initializers.cpp"
           "${_overlay_transform_dir}/native_overlay_initializers.hpp"
           "${_overlay_transform_dir}/transform_proof.json"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_overlay_transform_dir}"
    COMMAND "${Python3_EXECUTABLE}" -B "${_overlay_transform_script}"
        --compiler "${CMAKE_CXX_COMPILER}"
        --generated-dir "${_overlay_generated_dir}"
        --contract "${_overlay_transform_contract}"
        --out-dir "${_overlay_transform_dir}" --self-test --replace-generated
    DEPENDS "${_overlay_transform_script}" "${_overlay_transform_contract}"
        "${_overlay_generated_dir}/m_GAMER_unit_0000.cpp"
        "${_overlay_generated_dir}/m_MOVIER_unit_0021.cpp"
        "${_overlay_generated_dir}/units.hpp" ${_overlay_ast_headers}
    VERBATIM COMMENT "Transform actual generated overlay initializer C++ to typed native objects")
add_library(mka_transformed_overlay_initializers STATIC EXCLUDE_FROM_ALL
    "${_overlay_transform_dir}/native_overlay_initializers.cpp")
target_include_directories(mka_transformed_overlay_initializers PUBLIC "${_overlay_transform_dir}")
target_compile_features(mka_transformed_overlay_initializers PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(mka_transformed_overlay_initializers PRIVATE /W4 /WX)
else()
    target_compile_options(mka_transformed_overlay_initializers PRIVATE -Wall -Wextra -Werror)
endif()


# Guest state belongs exclusively to this differential test's reference side.
# It is not linked into the transformed native library.
if(TARGET ps2_phase4_aot)
add_executable(test_phase5_generated_transform EXCLUDE_FROM_ALL
    tests/test_phase5_generated_transform.cpp
    "${CMAKE_SOURCE_DIR}/build/generated-aot/vu1/vu1_table.cpp"
    runtime/src/dispatch.cpp runtime/src/observer_hook.cpp
    runtime/src/native_hook.cpp runtime/src/cop.cpp runtime/src/vu_micro.cpp)
target_include_directories(test_phase5_generated_transform PRIVATE runtime/include runtime/src)
target_link_libraries(test_phase5_generated_transform PRIVATE
    ps2_phase4_aot mka_transformed_overlay_initializers)
endif()

# S3 reference-only frame boundaries. No product target links these Cpu/RAM
# copies; the manifest preserves the original28 probes in the existing39 set.
set(PS2_STACK_REFERENCE_MANIFEST "${CMAKE_SOURCE_DIR}/build/oracle-telemetry/stack-frame-analysis-current/SLICES.json"
    CACHE FILEPATH "Offline original generated-C++ stack slice manifest for reference tests")
if(TARGET ps2_phase4_aot AND EXISTS "${PS2_STACK_REFERENCE_MANIFEST}")
    set(_stack_reference_dir "${CMAKE_CURRENT_BINARY_DIR}/generated-stack-reference")
    file(READ "${PS2_STACK_REFERENCE_MANIFEST}" _stack_reference_manifest)
    string(JSON _stack_reference_count LENGTH "${_stack_reference_manifest}" targets)
    math(EXPR _stack_reference_last "${_stack_reference_count}-1")
    set(_stack_reference_sources)
    set(_stack_original_sources)
    foreach(_stack_i RANGE 0 ${_stack_reference_last})
        string(JSON _stack_unit GET "${_stack_reference_manifest}" targets ${_stack_i} source_unit)
        get_filename_component(_stack_stem "${_stack_unit}" NAME_WE)
        list(APPEND _stack_reference_sources "${_stack_reference_dir}/reference_${_stack_stem}.cpp")
        list(APPEND _stack_original_sources "${CMAKE_SOURCE_DIR}/${_stack_unit}")
    endforeach()
    list(REMOVE_DUPLICATES _stack_reference_sources)
    list(REMOVE_DUPLICATES _stack_original_sources)
    add_custom_command(
        OUTPUT "${_stack_reference_dir}/stack_reference_slices.hpp"
               "${_stack_reference_dir}/stack_reference_slices.cpp"
               "${_stack_reference_dir}/stack_reference_proof.json" ${_stack_reference_sources}
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_SOURCE_DIR}/tools/research/generate_stack_reference_slices.py"
            --manifest "${PS2_STACK_REFERENCE_MANIFEST}"
            --generated-dir "${_overlay_generated_dir}"
            --original-elf "${CMAKE_SOURCE_DIR}/../../../private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/SLUS_214.10"
            --out-dir "${_stack_reference_dir}" --replace-reference-output
        DEPENDS "${CMAKE_SOURCE_DIR}/tools/research/generate_stack_reference_slices.py"
            "${PS2_STACK_REFERENCE_MANIFEST}" ${_stack_original_sources}
            "${CMAKE_SOURCE_DIR}/../../../private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/SLUS_214.10"
        VERBATIM COMMENT "Copy authenticated original AOT frame boundaries for test-only replay")
    add_executable(test_generated_stack_frames EXCLUDE_FROM_ALL
        tests/test_generated_stack_frames.cpp
        "${_stack_reference_dir}/stack_reference_slices.cpp" ${_stack_reference_sources}
        "${CMAKE_SOURCE_DIR}/build/generated-aot/vu1/vu1_table.cpp"
        runtime/src/dispatch.cpp runtime/src/observer_hook.cpp runtime/src/native_hook.cpp
        runtime/src/cop.cpp runtime/src/vu_micro.cpp)
    target_include_directories(test_generated_stack_frames PRIVATE runtime/include runtime/src
        "${_overlay_generated_dir}" "${_stack_reference_dir}")
    target_link_libraries(test_generated_stack_frames PRIVATE ps2_phase4_aot)
endif()
