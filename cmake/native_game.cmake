# Native retail game state/caller components. No guest ABI/runtime linkage.
add_library(mka_native_game_actions STATIC EXCLUDE_FROM_ALL
    runtime/src/native/controller_input.cpp runtime/src/native/game_action_input.cpp
    runtime/src/native/game_input_tasks.cpp runtime/src/native/game_transition_state.cpp
    runtime/src/native/frontend_scheduler.cpp)
target_include_directories(mka_native_game_actions PUBLIC runtime/include)
target_compile_features(mka_native_game_actions PUBLIC cxx_std_20)
add_executable(test_native_game_action_input EXCLUDE_FROM_ALL tests/test_game_action_input.cpp)
target_link_libraries(test_native_game_action_input PRIVATE mka_native_game_actions)
add_executable(test_native_game_input_tasks EXCLUDE_FROM_ALL tests/test_game_input_tasks.cpp)
target_link_libraries(test_native_game_input_tasks PRIVATE mka_native_game_actions)
add_executable(test_native_game_transition_state EXCLUDE_FROM_ALL tests/test_game_transition_state.cpp)
target_link_libraries(test_native_game_transition_state PRIVATE mka_native_game_actions)
add_executable(test_native_frontend_scheduler EXCLUDE_FROM_ALL tests/test_frontend_scheduler.cpp)
target_link_libraries(test_native_frontend_scheduler PRIVATE mka_native_game_actions)

# Native material game data/updates; no guest runtime or software-FPU include.
add_library(mka_native_material STATIC EXCLUDE_FROM_ALL
    runtime/src/native/material_translation.cpp runtime/src/native/matfx_binding.cpp
    runtime/src/native/material_maintenance.cpp
    runtime/src/native/geometry_attributes.cpp runtime/src/native/geometry_companion_quantizer.cpp)
target_include_directories(mka_native_material PUBLIC runtime/include)
target_link_libraries(mka_native_material PUBLIC mka_native_game_actions)
add_executable(test_native_material_translation EXCLUDE_FROM_ALL tests/test_material_translation.cpp)
target_link_libraries(test_native_material_translation PRIVATE mka_native_material)
add_executable(test_native_matfx_binding EXCLUDE_FROM_ALL tests/test_matfx_binding.cpp)
target_link_libraries(test_native_matfx_binding PRIVATE mka_native_material)
add_executable(test_native_material_maintenance EXCLUDE_FROM_ALL tests/test_material_maintenance.cpp)
target_link_libraries(test_native_material_maintenance PRIVATE mka_native_material)
add_executable(test_native_material_hierarchy EXCLUDE_FROM_ALL tests/test_material_hierarchy.cpp)
target_link_libraries(test_native_material_hierarchy PRIVATE mka_native_material)
add_executable(test_native_geometry_attributes EXCLUDE_FROM_ALL tests/test_geometry_attributes.cpp)
target_link_libraries(test_native_geometry_attributes PRIVATE mka_native_material)
add_executable(test_native_geometry_companion_quantizer EXCLUDE_FROM_ALL tests/test_geometry_companion_quantizer.cpp)
target_link_libraries(test_native_geometry_companion_quantizer PRIVATE mka_native_material)
# Separate OFFLINE source oracle, never a candidate dependency.
add_executable(geometry_companion_source_oracle EXCLUDE_FROM_ALL tests/geometry_companion_source_oracle.cpp)
target_include_directories(geometry_companion_source_oracle PRIVATE runtime/include)
# Separate offline project-helper oracle ONLY. No candidate links this target.
add_executable(material_translation_source_oracle EXCLUDE_FROM_ALL tests/material_translation_source_oracle.cpp)
target_include_directories(material_translation_source_oracle PRIVATE runtime/include)

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
    add_library(mka_native_logon_frontend STATIC EXCLUDE_FROM_ALL runtime/src/native/logon_frontend_task.cpp)
    target_include_directories(mka_native_logon_frontend PUBLIC runtime/include)
    target_link_libraries(mka_native_logon_frontend PUBLIC mka_native_logon)
    add_dependencies(mka_native_logon_frontend mka_native_logon)
    add_executable(test_native_logon_frontend EXCLUDE_FROM_ALL tests/test_logon_frontend_task.cpp)
    target_link_libraries(test_native_logon_frontend PRIVATE mka_native_logon_frontend mka_native_material)
endif()

# Native pending ownership barriers only: no media decoder or physical endpoint.
add_executable(test_native_logon_pending EXCLUDE_FROM_ALL tests/test_logon_pending.cpp)
target_link_libraries(test_native_logon_pending PRIVATE mka_native_game_actions)

# Native startup content and AOT retail state. No sector/content_vfs/runtime TU.
add_library(mka_native_content_volume STATIC EXCLUDE_FROM_ALL
    runtime/src/native/native_file.cpp runtime/src/native/content_volume.cpp)
target_include_directories(mka_native_content_volume PUBLIC runtime/include)
target_compile_features(mka_native_content_volume PUBLIC cxx_std_20)
add_executable(test_native_content_volume_owner EXCLUDE_FROM_ALL tests/test_native_content_volume_owner.cpp)
target_link_libraries(test_native_content_volume_owner PRIVATE mka_native_content_volume)
add_executable(test_native_startup_codegen EXCLUDE_FROM_ALL
    tests/test_native_startup_codegen.cpp recomp/native_closure_codegen.cpp recomp/native_startup_codegen.cpp)
target_include_directories(test_native_startup_codegen PRIVATE recomp)
if(WIN32)
    file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/recomp/native_startup_codegen.cpp" _startup_recipe_hash)
    file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/recomp/native_closure_codegen.cpp" _startup_checker_hash)
    string(SHA256 _startup_key "${_startup_recipe_hash}:${_startup_checker_hash}")
    set(_startup_dir "${CMAKE_CURRENT_BINARY_DIR}/native-retail-startup-${_startup_key}")
    set(_startup_elf "${CMAKE_CURRENT_SOURCE_DIR}/../../../research/inputs/SLUS_214.10")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/recomp/native_startup_codegen.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/recomp/native_closure_codegen.cpp")
    add_custom_command(OUTPUT "${_startup_dir}/native_retail_startup.hpp"
        "${_startup_dir}/native_retail_startup.cpp" "${_startup_dir}/native_retail_entry.cpp"
        COMMAND $<TARGET_FILE:eerecomp> --native-startup "${_startup_elf}" --out "${_startup_dir}"
        DEPENDS eerecomp "${_startup_elf}" recomp/native_startup_codegen.cpp recomp/native_closure_codegen.cpp
        VERBATIM COMMENT "AOT compile original retail startup into owned native state")
    add_library(mka_native_retail_startup STATIC EXCLUDE_FROM_ALL
        "${_startup_dir}/native_retail_startup.cpp" "${_startup_dir}/native_retail_entry.cpp")
    target_include_directories(mka_native_retail_startup PUBLIC "${_startup_dir}" runtime/include)
    target_link_libraries(mka_native_retail_startup PUBLIC mka_native_content_volume mka_native_retail_memory_owners)
    add_executable(test_native_retail_startup EXCLUDE_FROM_ALL tests/test_native_retail_startup.cpp)
    target_link_libraries(test_native_retail_startup PRIVATE mka_native_retail_startup)
    # Expected-negative link fixture: never included in a successful build batch.
    # Four real production obligations are missing; no test success providers.
    add_executable(test_native_startup_missing_bindings EXCLUDE_FROM_ALL tests/test_native_startup_missing_bindings.cpp)
    target_link_libraries(test_native_startup_missing_bindings PRIVATE mka_native_retail_startup)
endif()

# Native identities for the actual ordered retail main memory-owner phase.
# Unsupported source allocator policies remain explicit, not ordinary fallbacks.
add_library(mka_native_retail_memory_owners STATIC EXCLUDE_FROM_ALL runtime/src/native/retail_memory_owners.cpp)
target_link_libraries(mka_native_retail_memory_owners PUBLIC mka_native_wave_allocation)
add_executable(test_native_retail_memory_owners EXCLUDE_FROM_ALL tests/test_retail_memory_owners.cpp)
target_link_libraries(test_native_retail_memory_owners PRIVATE mka_native_retail_memory_owners)
add_executable(test_native_retail_memory_owner_faults EXCLUDE_FROM_ALL tests/test_retail_memory_owner_faults.cpp)
target_link_libraries(test_native_retail_memory_owner_faults PRIVATE mka_native_retail_memory_owners)

# Native file regression must be available without the legacy generated game.
add_executable(test_native_file EXCLUDE_FROM_ALL tests/test_native_file.cpp)
target_link_libraries(test_native_file PRIVATE mka_native_content_volume)

# Real native allocation ownership, independent of the guest runtime.
add_library(mka_native_heap STATIC EXCLUDE_FROM_ALL runtime/src/native/native_heap.cpp)
target_include_directories(mka_native_heap PUBLIC runtime/include)
target_compile_features(mka_native_heap PUBLIC cxx_std_20)
add_executable(test_native_heap_owner EXCLUDE_FROM_ALL tests/test_native_heap_owner.cpp)
target_link_libraries(test_native_heap_owner PRIVATE mka_native_heap)
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    # Test-only host allocator/deleter gates. Production has no injection path.
    add_executable(test_native_heap_inflight EXCLUDE_FROM_ALL tests/test_native_heap_inflight.cpp)
    target_link_libraries(test_native_heap_inflight PRIVATE mka_native_heap)
    target_link_options(test_native_heap_inflight PRIVATE
        "-Wl,--wrap=_ZnwySt11align_val_t" "-Wl,--wrap=_ZdlPvSt11align_val_t")
endif()

# Actual retail Wave transaction policy; startup/overlay bindings remain required.
add_library(mka_native_wave_allocation STATIC EXCLUDE_FROM_ALL runtime/src/native/wave_allocation.cpp)
target_link_libraries(mka_native_wave_allocation PUBLIC mka_native_heap)
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_executable(test_native_wave_allocation EXCLUDE_FROM_ALL tests/test_wave_allocation.cpp)
    target_link_libraries(test_native_wave_allocation PRIVATE mka_native_wave_allocation)
    target_link_options(test_native_wave_allocation PRIVATE "-Wl,--wrap=_ZnwySt11align_val_t")
endif()

# Native direct allocation-class payload and source-derived game descriptors.
add_library(mka_native_allocation_class STATIC EXCLUDE_FROM_ALL runtime/src/native/allocation_class.cpp)
target_link_libraries(mka_native_allocation_class PUBLIC mka_native_heap)
add_executable(test_native_allocation_class EXCLUDE_FROM_ALL tests/test_allocation_class.cpp)
target_link_libraries(test_native_allocation_class PRIVATE mka_native_allocation_class)
add_library(mka_native_allocation_profile STATIC EXCLUDE_FROM_ALL runtime/src/native/allocation_profile.cpp)
target_link_libraries(mka_native_allocation_profile PUBLIC mka_native_allocation_class)
add_executable(test_native_class_profile EXCLUDE_FROM_ALL tests/test_allocation_profile.cpp)
target_link_libraries(test_native_class_profile PRIVATE mka_native_allocation_profile)

# Source-verified native class-tier and complete Wave allocation policy.
add_library(mka_native_class_tier_allocation STATIC EXCLUDE_FROM_ALL runtime/src/native/class_tier_allocation.cpp)
target_link_libraries(mka_native_class_tier_allocation PUBLIC mka_native_allocation_class mka_native_wave_allocation)
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_executable(test_native_class_tier_allocation EXCLUDE_FROM_ALL tests/test_class_tier_allocation.cpp)
    target_link_libraries(test_native_class_tier_allocation PRIVATE mka_native_class_tier_allocation)
    target_link_options(test_native_class_tier_allocation PRIVATE "-Wl,--wrap=_ZnwySt11align_val_t")
endif()

# Mutable native HUD positions with owned object lifetime; no guest runtime.
find_package(Threads REQUIRED)
add_library(mka_native_hud_health_geometry STATIC EXCLUDE_FROM_ALL runtime/src/native/hud_health_geometry.cpp)
target_include_directories(mka_native_hud_health_geometry PUBLIC runtime/include)
target_compile_features(mka_native_hud_health_geometry PUBLIC cxx_std_20)
target_link_libraries(mka_native_hud_health_geometry PUBLIC Threads::Threads)
add_executable(test_native_hud_health_geometry EXCLUDE_FROM_ALL tests/test_hud_health_geometry.cpp)
target_link_libraries(test_native_hud_health_geometry PRIVATE mka_native_hud_health_geometry)

# Owned serialized art data from native filesystem ranges; no texture-ready claim.
add_library(mka_native_art_section_data STATIC EXCLUDE_FROM_ALL runtime/src/native/art_section_data.cpp)
target_link_libraries(mka_native_art_section_data PUBLIC mka_native_content_volume)
add_executable(test_native_art_section_data EXCLUDE_FROM_ALL tests/test_art_section_data.cpp)
target_link_libraries(test_native_art_section_data PRIVATE mka_native_art_section_data)
