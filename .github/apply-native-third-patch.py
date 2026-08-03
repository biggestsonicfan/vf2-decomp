from pathlib import Path

main_path = Path("tools/vf2i960/main.c")
text = main_path.read_text(encoding="utf-8")

if "static int command_native_third_dispatch" not in text:
    replacements = [
        (
            '#include "vf2/model2a.h"\n#include "vf2/recovered.h"\n',
            '#include "vf2/model2a.h"\n#include "vf2/native_differential.h"\n#include "vf2/recovered.h"\n',
        ),
        (
            '        "  %s native-second-dispatch <rom-directory>\\n"\n'
            '        "  %s compare-texture-bridge <rom-directory>\\n"\n',
            '        "  %s native-second-dispatch <rom-directory>\\n"\n'
            '        "  %s native-third-dispatch <rom-directory>\\n"\n'
            '        "  %s compare-texture-bridge <rom-directory>\\n"\n',
        ),
        (
            "static int command_native_dispatch_ex(\n"
            "    const char *rom_directory,\n"
            "    bool continue_to_second_dispatch,\n"
            "    bool observe_third_sweep\n"
            ")\n",
            "static int command_native_dispatch_ex(\n"
            "    const char *rom_directory,\n"
            "    bool continue_to_second_dispatch,\n"
            "    bool native_third_dispatch,\n"
            "    bool observe_third_sweep\n"
            ")\n",
        ),
        (
            "static int command_native_first_dispatch(const char *rom_directory)\n"
            "{\n"
            "    return command_native_dispatch_ex(rom_directory, false, false);\n"
            "}\n\n"
            "static int command_native_second_dispatch(const char *rom_directory)\n"
            "{\n"
            "    return command_native_dispatch_ex(rom_directory, true, false);\n"
            "}\n\n"
            "static int command_native_observe_third_sweep(const char *rom_directory)\n"
            "{\n"
            "    return command_native_dispatch_ex(rom_directory, true, true);\n"
            "}\n",
            "static int command_native_first_dispatch(const char *rom_directory)\n"
            "{\n"
            "    return command_native_dispatch_ex(rom_directory, false, false, false);\n"
            "}\n\n"
            "static int command_native_second_dispatch(const char *rom_directory)\n"
            "{\n"
            "    return command_native_dispatch_ex(rom_directory, true, false, false);\n"
            "}\n\n"
            "static int command_native_third_dispatch(const char *rom_directory)\n"
            "{\n"
            "    return command_native_dispatch_ex(rom_directory, true, true, false);\n"
            "}\n\n"
            "static int command_native_observe_third_sweep(const char *rom_directory)\n"
            "{\n"
            "    return command_native_dispatch_ex(rom_directory, true, false, true);\n"
            "}\n",
        ),
        (
            "    dispatch_result = command_native_dispatch_ex(rom_directory, true, false);\n",
            "    dispatch_result = command_native_dispatch_ex(\n"
            "        rom_directory, true, false, false\n"
            "    );\n",
        ),
        (
            '    if (strcmp(argv[1], "native-second-dispatch") == 0 && argc == 3) {\n'
            "        return command_native_second_dispatch(argv[2]);\n"
            "    }\n"
            '    if (strcmp(argv[1], "compare-texture-bridge") == 0 && argc == 3) {\n',
            '    if (strcmp(argv[1], "native-second-dispatch") == 0 && argc == 3) {\n'
            "        return command_native_second_dispatch(argv[2]);\n"
            "    }\n"
            '    if (strcmp(argv[1], "native-third-dispatch") == 0 && argc == 3) {\n'
            "        return command_native_third_dispatch(argv[2]);\n"
            "    }\n"
            '    if (strcmp(argv[1], "compare-texture-bridge") == 0 && argc == 3) {\n',
        ),
    ]

    for old, new in replacements:
        count = text.count(old)
        if count != 1:
            raise SystemExit(f"expected one main.c match, got {count}: {old[:120]!r}")
        text = text.replace(old, new, 1)

    usage_start = text.index("static void usage(const char *program)")
    usage_end = text.index("\n}\n\nstatic FILE *g_orchestrator_trace_file", usage_start)
    usage = text[usage_start:usage_end]
    final_argument = "        program\n    );"
    if usage.count(final_argument) != 1:
        raise SystemExit("could not locate final usage argument")
    usage = usage.replace(
        final_argument,
        "        program,\n        program\n    );",
        1,
    )
    text = text[:usage_start] + usage + text[usage_end:]

    insertion_marker = "    if (status == VF2_OK && observe_third_sweep) {\n"
    if text.count(insertion_marker) != 1:
        raise SystemExit("could not locate third-sweep observation block")
    native_block = r'''    if (status == VF2_OK && native_third_dispatch) {
        const uint32_t third_entry = plan.runnable_entry_points[0];
        const uint32_t third_registry = plan.runnable_registry_addresses[0];
        vf2_native_runtime_state runtime_state;
        vf2_native_differential_report third_report;

        stage = "native-third-dispatch";
        memset(&runtime_state, 0, sizeof(runtime_state));
        memset(&third_report, 0, sizeof(third_report));
        status = vf2_native_runtime_initialize(&runtime_state, 4u);
        if (status == VF2_OK) {
            status = vf2_native_differential_run_until_after(
                &original_machine,
                &original_cpu,
                &native_machine,
                &native_cpu,
                &runtime_state,
                third_entry,
                1u,
                4096u,
                &third_report
            );
        }
        if (status == VF2_OK &&
            (third_report.blocks_compared != 42u ||
             third_report.reference_instructions_executed != UINT64_C(55237) ||
             third_report.native_recovered_instructions != UINT64_C(55237) ||
             original_cpu.registers[29] != third_registry ||
             native_cpu.registers[29] != third_registry)) {
            status = VF2_ERROR_UNSUPPORTED;
        }

        if (status != VF2_OK) {
            fprintf(
                stderr,
                "Native third-dispatch validation failed during %s: %s\n",
                stage,
                vf2_status_string(status)
            );
            fprintf(
                stderr,
                "Reference/native IP: 0x%08x/0x%08x blocks=%zu "
                "instructions=%llu/%llu\n",
                (unsigned)third_report.final_reference_address,
                (unsigned)third_report.final_native_address,
                third_report.blocks_compared,
                (unsigned long long)
                    third_report.reference_instructions_executed,
                (unsigned long long)
                    third_report.native_recovered_instructions
            );
            fprintf(
                stderr,
                "Last native step: %s entry=0x%08x exit=0x%08x "
                "bridge=%s task=%s\n",
                vf2_native_runtime_step_kind_name(
                    third_report.last_step.kind
                ),
                (unsigned)third_report.last_step.entry_address,
                (unsigned)third_report.last_step.exit_address,
                vf2_hybrid_bridge_kind_name(
                    third_report.last_step.bridge_kind
                ),
                vf2_hybrid_task_kind_name(
                    third_report.last_step.task_kind
                )
            );
            if (!third_report.diff.equal &&
                third_report.diff.component[0] != '\0') {
                fprintf(
                    stderr,
                    "Difference: %s offset=0x%zx expected=0x%08x "
                    "actual=0x%08x bytes=%zu\n",
                    third_report.diff.component,
                    third_report.diff.first_offset,
                    (unsigned)third_report.diff.expected_value,
                    (unsigned)third_report.diff.actual_value,
                    third_report.diff.differing_bytes
                );
            }
        } else {
            printf("\nNative third-dispatch validation: MATCH\n");
            printf("Repeated-frame blocks compared:     %zu\n",
                   third_report.blocks_compared);
            printf("Repeated-frame instructions:        %llu\n",
                   (unsigned long long)
                       third_report.native_recovered_instructions);
            printf("Reference instructions compared:    %llu\n",
                   (unsigned long long)
                       third_report.reference_instructions_executed);
            printf("Repeated scheduler entries:         %zu\n",
                   runtime_state.scheduler_entries);
            printf("Repeated scheduler transitions:     %zu\n",
                   runtime_state.scheduler_transitions);
            printf("Repeated scheduler finishes:        %zu\n",
                   runtime_state.scheduler_finishes);
            printf("Repeated frame-wait phases:         %zu\n",
                   runtime_state.frame_wait_phases);
            printf("Third task entry:                   0x%08x\n",
                   (unsigned)native_cpu.ip);
            printf("Third registry:                     0x%08x\n",
                   (unsigned)native_cpu.registers[29]);
            printf("Continuous recovered instructions:  %llu\n",
                   (unsigned long long)(
                       bridge_steps +
                       third_report.native_recovered_instructions
                   ));
            printf("Final CPU and memory state:         MATCH\n");
        }
    }

'''
    text = text.replace(insertion_marker, native_block + insertion_marker, 1)
    main_path.write_text(text, encoding="utf-8")

cmake_path = Path("CMakeLists.txt")
cmake = cmake_path.read_text(encoding="utf-8")
if "NAME vf2_native_third_dispatch" not in cmake:
    old = '''        add_test(
            NAME vf2_native_second_dispatch
            COMMAND vf2i960 native-second-dispatch "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_texture_bridge_differential
'''
    new = '''        add_test(
            NAME vf2_native_second_dispatch
            COMMAND vf2i960 native-second-dispatch "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_native_third_dispatch
            COMMAND vf2i960 native-third-dispatch "${VF2_ROM_DIR}"
        )
        add_test(
            NAME vf2_texture_bridge_differential
'''
    if cmake.count(old) != 1:
        raise SystemExit("could not locate native second CTest block")
    cmake = cmake.replace(old, new, 1)
    old = '''            vf2_native_first_dispatch vf2_native_second_dispatch
            vf2_texture_bridge_differential vf2_post_frame_bridge_differential
'''
    new = '''            vf2_native_first_dispatch vf2_native_second_dispatch
            vf2_native_third_dispatch vf2_texture_bridge_differential
            vf2_post_frame_bridge_differential
'''
    if cmake.count(old) != 1:
        raise SystemExit("could not locate native timeout list")
    cmake = cmake.replace(old, new, 1)
    cmake_path.write_text(cmake, encoding="utf-8")

readme_path = Path("README.md")
readme = readme_path.read_text(encoding="utf-8")
if "build/vf2i960 native-third-dispatch" not in readme:
    old = '''build/vf2i960 native-first-dispatch /path/to/vf2
build/vf2i960 native-second-dispatch /path/to/vf2
build/vf2i960 compare-texture-bridge /path/to/vf2
'''
    new = '''build/vf2i960 native-first-dispatch /path/to/vf2
build/vf2i960 native-second-dispatch /path/to/vf2
build/vf2i960 native-third-dispatch /path/to/vf2
build/vf2i960 compare-texture-bridge /path/to/vf2
'''
    if readme.count(old) != 1:
        raise SystemExit("could not locate README native commands")
    readme_path.write_text(readme.replace(old, new, 1), encoding="utf-8")
