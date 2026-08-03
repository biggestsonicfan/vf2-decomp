cmake_minimum_required(VERSION 3.20)

project(
    vf2_decomp
    VERSION 0.0.25
    DESCRIPTION "Clean-room, non-matching C decompilation project for Virtua Fighter 2 Version 2.1"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(VF2_BUILD_TESTS "Build unit tests" ON)
option(VF2_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(VF2_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
set(VF2_ROM_DIR "" CACHE PATH "Optional supported VF2 ROM directory for integration tests")
if(NOT VF2_ROM_DIR)
  set(VF2_ROM_DIR "${CMAKE_SOURCE_DIR}/roms/vf2")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(VF2Warnings)

add_library(vf2_core STATIC
    src/analysis/boundaries.c
    src/analysis/cfg.c
    src/analysis/image_map.c
    src/analysis/orchestrator_gates.c
    src/analysis/orchestrator_limits.c
    src/analysis/orchestrator_scan.c
    src/analysis/pseudoc.c
    src/analysis/semantics.c
    src/analysis/symbols.c
    src/analysis/tasks.c
    src/analysis/xref.c
    src/core/boot.c
    src/core/file.c
    src/core/hash.c
    src/core/rom_layout.c
    src/core/rom_manifest.c
    src/core/status.c
    src/game/game.c
    src/hardware/model2a.c
    src/hardware/scsp.c
    src/hardware/tgp.c
    src/i960/decoder.c
    src/i960/executor.c
    src/i960/formatter.c
    src/i960/instruction.c
    src/i960/snapshot.c
    src/platform/platform_null.c
    src/recovered/boot_entry.c
    src/recovered/camera_viewport.c
    src/recovered/hybrid.c
    src/recovered/frame_wait.c
    src/recovered/native_runtime.c
    src/recovered/native_differential.c
    src/recovered/texture_bridge.c
    src/recovered/texture_bridge_texture.c
    src/recovered/texture_bridge_video.c
    src/recovered/texture_bridge_geometry.c
    src/recovered/texture_bridge_input.c
    src/recovered/texture_bridge_match.c
    src/recovered/boot/boot_stage1.c
    src/recovered/boot/boot_stage2.c
    src/recovered/boot/memory_init.c
    src/recovered/task_registry.c
    src/recovered/runtime_interrupts.c
    src/recovered/scheduler.c
    src/recovered/tasks.c
)

target_include_directories(vf2_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

vf2_set_project_warnings(vf2_core)

add_executable(vf2 src/app/main.c)
target_link_libraries(vf2 PRIVATE vf2_core)
vf2_set_project_warnings(vf2)

add_executable(vf2rom tools/vf2rom/main.c)
target_link_libraries(vf2rom PRIVATE vf2_core)
vf2_set_project_warnings(vf2rom)

add_executable(vf2i960 tools/vf2i960/main.c)
target_link_libraries(vf2i960 PRIVATE vf2_core)
vf2_set_project_warnings(vf2i960)

include(GNUInstallDirs)
install(TARGETS vf2 vf2rom vf2i960
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

if(VF2_BUILD_TESTS)
    enable_testing()

    add_executable(vf2_tests
        tests/test_main.c
        tests/i960/test_decoder.c
        tests/i960/test_executor.c
        tests/i960/test_snapshot.c
        tests/i960/test_interrupts.c
        tests/analysis/test_cfg.c
        tests/analysis/test_semantics.c
        tests/analysis/test_tasks.c
    )
    target_link_libraries(vf2_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_tests)

    add_test(NAME vf2_tests COMMAND vf2_tests)

    add_executable(vf2_orchestrator_limits_tests
        tests/analysis/test_orchestrator_limits.c
    )
    target_link_libraries(vf2_orchestrator_limits_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_orchestrator_limits_tests)

    add_test(
        NAME vf2_orchestrator_limits
        COMMAND vf2_orchestrator_limits_tests
    )

    add_executable(vf2_orchestrator_scan_tests
        tests/analysis/test_orchestrator_scan.c
    )
    target_link_libraries(vf2_orchestrator_scan_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_orchestrator_scan_tests)

    add_test(
        NAME vf2_orchestrator_scan
        COMMAND vf2_orchestrator_scan_tests
    )

    add_executable(vf2_orchestrator_gates_tests
        tests/analysis/test_orchestrator_gates.c
    )
    target_link_libraries(vf2_orchestrator_gates_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_orchestrator_gates_tests)

    add_test(
        NAME vf2_orchestrator_gates
        COMMAND vf2_orchestrator_gates_tests
    )

    add_library(vf2_test_owned_main_rom OBJECT
        tests/support/owned_main_rom.c
    )
    target_include_directories(vf2_test_owned_main_rom PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )
    vf2_set_project_warnings(vf2_test_owned_main_rom)

    add_executable(vf2_orchestrator_bridge_tests
        tests/analysis/test_orchestrator_bridge.c
        $<TARGET_OBJECTS:vf2_test_owned_main_rom>
    )
    if(MSVC)
        target_compile_options(vf2_orchestrator_bridge_tests PRIVATE
            "/FI${CMAKE_CURRENT_SOURCE_DIR}/tests/support/owned_main_rom.h"
        )
    else()
        target_compile_options(vf2_orchestrator_bridge_tests PRIVATE
            -include
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/support/owned_main_rom.h"
        )
    endif()
    target_link_libraries(vf2_orchestrator_bridge_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_orchestrator_bridge_tests)

    add_test(
        NAME vf2_orchestrator_bridge
        COMMAND vf2_orchestrator_bridge_tests
    )

    add_executable(vf2_native_runtime_tests
        tests/recovered/test_native_runtime.c
    )
    target_link_libraries(vf2_native_runtime_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_native_runtime_tests)

    add_test(
        NAME vf2_native_runtime
        COMMAND vf2_native_runtime_tests
    )

    add_executable(vf2_native_differential_tests
        tests/recovered/test_native_differential.c
    )
    target_link_libraries(vf2_native_differential_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_native_differential_tests)

    add_test(
        NAME vf2_native_differential
        COMMAND vf2_native_differential_tests
    )

    add_executable(vf2_native_scheduler_tests
        tests/recovered/test_native_scheduler.c
    )
    target_link_libraries(vf2_native_scheduler_tests PRIVATE vf2_core)
    vf2_set_project_warnings(vf2_native_scheduler_tests)

    add_test(
        NAME vf2_native_scheduler
        COMMAND vf2_native_scheduler_tests
    )

    if(EXISTS "${VF2_ROM_DIR}")
        add_test(
            NAME vf2_boot_differential
            COMMAND vf2i960 compare-boot "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_initialization_differential
            COMMAND vf2i960 compare-init "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_task_registry_differential
            COMMAND vf2i960 compare-task-registry "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_task_catalog
            COMMAND vf2i960 tasks "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_runtime_checkpoint
            COMMAND vf2i960 runtime-checkpoint "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_runtime_events
            COMMAND vf2i960 scheduler-pass "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_timer_irq_differential
            COMMAND vf2i960 compare-timer-irq "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_scheduler_dispatch
            COMMAND vf2i960 scheduler-dispatch "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_task_recoveries_differential
            COMMAND vf2i960 compare-task-recoveries "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_task_profile
            COMMAND vf2i960 task-profile "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_first_dispatch_recoveries
            COMMAND vf2i960 compare-first-dispatch "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_camera_classifier_differential
            COMMAND vf2i960 compare-camera-classifier "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_camera_viewport_differential
            COMMAND vf2i960 compare-camera-viewport "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_hybrid_first_dispatch
            COMMAND vf2i960 hybrid-first-dispatch "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_native_first_dispatch
            COMMAND vf2i960 native-first-dispatch "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_native_second_dispatch
            COMMAND vf2i960 native-second-dispatch "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_texture_bridge_differential
            COMMAND vf2i960 compare-texture-bridge "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_post_frame_bridge_differential
            COMMAND vf2i960 compare-post-frame-bridge "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_geometry_boundary_differential
            COMMAND vf2i960 compare-geometry-boundary "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_second_scheduler_entry_differential
            COMMAND vf2i960 compare-second-scheduler-entry "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_game_geometry_helpers_differential
            COMMAND vf2i960 compare-game-geometry-helpers "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_third_sweep_observation
            COMMAND vf2i960 observe-third-sweep "${VF2_ROM_DIR}"
        )
        set_tests_properties(
            vf2_runtime_checkpoint vf2_runtime_events vf2_timer_irq_differential
            vf2_scheduler_dispatch vf2_task_recoveries_differential vf2_task_profile
            vf2_first_dispatch_recoveries vf2_camera_classifier_differential
            vf2_camera_viewport_differential vf2_hybrid_first_dispatch
            vf2_native_first_dispatch vf2_native_second_dispatch
            vf2_texture_bridge_differential vf2_post_frame_bridge_differential
            vf2_geometry_boundary_differential
            vf2_second_scheduler_entry_differential
            vf2_game_geometry_helpers_differential
            vf2_third_sweep_observation
            PROPERTIES TIMEOUT 180
        )
    endif()
endif()
