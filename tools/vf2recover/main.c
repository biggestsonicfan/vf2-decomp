#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid.h"
#include "vf2/i960/decoder.h"
#include "vf2/i960/executor.h"
#include "vf2/i960/snapshot.h"
#include "vf2/native_runtime.h"
#include "vf2/rom.h"
#include "vf2/status.h"
#include "vf2/version.h"

typedef struct vf2_recover_options {
    const char *rom_directory;
    const char *snapshot_path;
    const char *runtime_state_path;
    const char *output_path;
    size_t disasm_count;
    size_t trace_count;
} vf2_recover_options;

static void print_usage(FILE *stream, const char *program)
{
    fprintf(
        stream,
        "vf2recover v%s\n"
        "Usage: %s --rom-dir <directory> --snapshot <file> "
        "[--state <file>] [--output <report.md>] "
        "[--disasm <count>] [--trace <count>]\n",
        VF2_VERSION_STRING,
        program
    );
}

static int parse_size(const char *text, size_t *value)
{
    char *end = NULL;
    unsigned long long parsed = 0u;

    if (text == NULL || value == NULL || text[0] == '\0') {
        return 0;
    }
    errno = 0;
    parsed = strtoull(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0' ||
        parsed > (unsigned long long)SIZE_MAX) {
        return 0;
    }
    *value = (size_t)parsed;
    return 1;
}

static int parse_options(
    int argc,
    char **argv,
    vf2_recover_options *options,
    int *show_help
)
{
    int index = 1;

    if (options == NULL || show_help == NULL) {
        return 0;
    }
    memset(options, 0, sizeof(*options));
    options->disasm_count = 32u;
    options->trace_count = 64u;
    *show_help = 0;

    while (index < argc) {
        const char *argument = argv[index];

        if (strcmp(argument, "--help") == 0 || strcmp(argument, "-h") == 0) {
            *show_help = 1;
            return 1;
        }
        if (strcmp(argument, "--rom-dir") == 0 && index + 1 < argc) {
            options->rom_directory = argv[++index];
        } else if (strcmp(argument, "--snapshot") == 0 &&
                   index + 1 < argc) {
            options->snapshot_path = argv[++index];
        } else if (strcmp(argument, "--state") == 0 && index + 1 < argc) {
            options->runtime_state_path = argv[++index];
        } else if (strcmp(argument, "--output") == 0 && index + 1 < argc) {
            options->output_path = argv[++index];
        } else if (strcmp(argument, "--disasm") == 0 && index + 1 < argc) {
            if (!parse_size(argv[++index], &options->disasm_count)) {
                return 0;
            }
        } else if (strcmp(argument, "--trace") == 0 && index + 1 < argc) {
            if (!parse_size(argv[++index], &options->trace_count)) {
                return 0;
            }
        } else {
            return 0;
        }
        ++index;
    }

    return options->rom_directory != NULL && options->snapshot_path != NULL;
}

static char *append_suffix(const char *path, const char *suffix)
{
    const size_t path_size = path != NULL ? strlen(path) : 0u;
    const size_t suffix_size = suffix != NULL ? strlen(suffix) : 0u;
    char *result = NULL;

    if (path == NULL || suffix == NULL ||
        path_size > SIZE_MAX - suffix_size - 1u) {
        return NULL;
    }
    result = (char *)malloc(path_size + suffix_size + 1u);
    if (result == NULL) {
        return NULL;
    }
    memcpy(result, path, path_size);
    memcpy(result + path_size, suffix, suffix_size + 1u);
    return result;
}

static vf2_status initialize_machine(
    vf2_model2a *machine,
    const uint8_t *main_rom,
    size_t main_rom_size,
    const uint8_t *main_data,
    size_t main_data_size
)
{
    vf2_status status = VF2_OK;

    memset(machine, 0, sizeof(*machine));
    if (!vf2_model2a_initialize(machine)) {
        return VF2_ERROR_OUT_OF_MEMORY;
    }
    status = vf2_model2a_attach_main_rom(
        machine, main_rom, main_rom_size
    );
    if (status == VF2_OK) {
        status = vf2_model2a_attach_main_data(
            machine, main_data, main_data_size
        );
    }
    if (status != VF2_OK) {
        vf2_model2a_shutdown(machine);
    }
    return status;
}

static void print_registers(FILE *output, const vf2_i960_cpu *cpu)
{
    size_t index = 0u;

    fprintf(output, "## Architectural state\n\n");
    fprintf(output, "- IP: `0x%08x`\n", (unsigned)cpu->ip);
    fprintf(output, "- SAT: `0x%08x`\n", (unsigned)cpu->sat);
    fprintf(output, "- PRCB: `0x%08x`\n", (unsigned)cpu->prcb);
    fprintf(output, "- Process control: `0x%08x`\n",
            (unsigned)cpu->process_control);
    fprintf(output, "- Arithmetic control: `0x%08x`\n",
            (unsigned)cpu->arithmetic_control);
    fprintf(output, "- Interrupt control: `0x%08x`\n",
            (unsigned)cpu->interrupt_control);
    fprintf(output, "- Compare result: `%u`\n",
            (unsigned)cpu->compare_result);
    fprintf(output, "- Local frame depth: `%u` (max `%u`)\n",
            (unsigned)cpu->local_frame_depth,
            (unsigned)cpu->maximum_local_frame_depth);
    fprintf(output, "- Executed instructions: `%llu`\n",
            (unsigned long long)cpu->executed_instructions);
    fprintf(output, "- Calls/returns: `%llu/%llu`\n",
            (unsigned long long)cpu->procedure_calls,
            (unsigned long long)cpu->procedure_returns);
    fprintf(output, "- Interrupt entries/returns: `%llu/%llu`\n\n",
            (unsigned long long)cpu->interrupt_entries,
            (unsigned long long)cpu->interrupt_returns);

    fprintf(output, "| Register | Value |\n|---|---:|\n");
    for (index = 0u; index < VF2_I960_REGISTER_COUNT; ++index) {
        fprintf(
            output,
            "| `%s` (`r%zu`) | `0x%08x` |\n",
            vf2_i960_register_name((uint8_t)index),
            index,
            (unsigned)cpu->registers[index]
        );
    }
    fputc('\n', output);
}

static void print_runtime_state(
    FILE *output,
    const vf2_native_runtime_state *state
)
{
    fprintf(output, "## Native host runtime state\n\n");
    fprintf(output, "| Counter | Value |\n|---|---:|\n");
    fprintf(output, "| frame-wait visits | %zu |\n", state->frame_wait.visits);
    fprintf(output, "| frame-wait threshold | %zu |\n",
            state->frame_wait.visits_before_interrupt);
    fprintf(output, "| injected frame IRQs | %zu |\n",
            state->frame_wait.interrupts_injected);
    fprintf(output, "| recovered blocks | %zu |\n", state->blocks_executed);
    fprintf(output, "| task bodies | %zu |\n", state->task_bodies_executed);
    fprintf(output, "| frame-wait phases | %zu |\n", state->frame_wait_phases);
    fprintf(output, "| scheduler entries | %zu |\n", state->scheduler_entries);
    fprintf(output, "| scheduler transitions | %zu |\n",
            state->scheduler_transitions);
    fprintf(output, "| scheduler finishes | %zu |\n", state->scheduler_finishes);
    fprintf(output, "| recovered instructions | %llu |\n",
            (unsigned long long)state->recovered_instruction_count);
    fprintf(output, "| recovered calls | %llu |\n",
            (unsigned long long)state->recovered_procedure_calls);
    fprintf(output, "| recovered returns | %llu |\n\n",
            (unsigned long long)state->recovered_procedure_returns);
}

static vf2_status print_disassembly(
    FILE *output,
    const uint8_t *image,
    size_t image_size,
    uint32_t start_address,
    size_t instruction_count
)
{
    uint32_t address = start_address;
    size_t index = 0u;

    fprintf(output, "## Static disassembly from checkpoint\n\n");
    fprintf(output, "| # | Address | Words | Instruction | Flow | Target |\n");
    fprintf(output, "|---:|---:|---|---|---|---:|\n");

    for (index = 0u; index < instruction_count; ++index) {
        vf2_i960_instruction instruction;
        char text[256];
        char words[40];
        char target[24];
        vf2_status status = vf2_i960_decode(
            image, image_size, address, &instruction
        );

        if (status != VF2_OK) {
            fprintf(output, "| %zu | `0x%08x` | - | decode failed: `%s` | - | - |\n",
                    index, (unsigned)address, vf2_status_string(status));
            return status;
        }
        status = vf2_i960_format_instruction(
            &instruction, text, sizeof(text)
        );
        if (status != VF2_OK) {
            return status;
        }
        if (instruction.size > 4u) {
            (void)snprintf(
                words,
                sizeof(words),
                "%08x %08x",
                (unsigned)instruction.words[0],
                (unsigned)instruction.words[1]
            );
        } else {
            (void)snprintf(
                words,
                sizeof(words),
                "%08x",
                (unsigned)instruction.words[0]
            );
        }
        if (instruction.has_target) {
            (void)snprintf(
                target, sizeof(target), "`0x%08x`", (unsigned)instruction.target
            );
        } else {
            (void)snprintf(target, sizeof(target), "-");
        }
        fprintf(
            output,
            "| %zu | `0x%08x` | `%s` | `%s` | %s | %s |\n",
            index,
            (unsigned)address,
            words,
            text,
            vf2_i960_flow_name(instruction.flow),
            target
        );
        if (instruction.size == 0u ||
            address > UINT32_MAX - instruction.size) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        address += instruction.size;
    }
    fputc('\n', output);
    return VF2_OK;
}

static void print_cpu_deltas(
    FILE *output,
    const vf2_i960_cpu *before,
    const vf2_i960_cpu *after
)
{
    size_t index = 0u;
    int first = 1;

    fprintf(output, "  - changed: ");
    for (index = 0u; index < VF2_I960_REGISTER_COUNT; ++index) {
        if (before->registers[index] != after->registers[index]) {
            fprintf(
                output,
                "%s`%s` 0x%08x→0x%08x",
                first ? "" : ", ",
                vf2_i960_register_name((uint8_t)index),
                (unsigned)before->registers[index],
                (unsigned)after->registers[index]
            );
            first = 0;
        }
    }
    if (before->arithmetic_control != after->arithmetic_control) {
        fprintf(output, "%s`ac` 0x%08x→0x%08x",
                first ? "" : ", ",
                (unsigned)before->arithmetic_control,
                (unsigned)after->arithmetic_control);
        first = 0;
    }
    if (before->process_control != after->process_control) {
        fprintf(output, "%s`pc` 0x%08x→0x%08x",
                first ? "" : ", ",
                (unsigned)before->process_control,
                (unsigned)after->process_control);
        first = 0;
    }
    if (before->interrupt_control != after->interrupt_control) {
        fprintf(output, "%s`ic` 0x%08x→0x%08x",
                first ? "" : ", ",
                (unsigned)before->interrupt_control,
                (unsigned)after->interrupt_control);
        first = 0;
    }
    if (before->compare_result != after->compare_result) {
        fprintf(output, "%s`cmp` %u→%u",
                first ? "" : ", ",
                (unsigned)before->compare_result,
                (unsigned)after->compare_result);
        first = 0;
    }
    if (before->local_frame_depth != after->local_frame_depth) {
        fprintf(output, "%s`frame-depth` %u→%u",
                first ? "" : ", ",
                (unsigned)before->local_frame_depth,
                (unsigned)after->local_frame_depth);
        first = 0;
    }
    if (first) {
        fprintf(output, "none");
    }
    fputc('\n', output);
}

static vf2_status print_reference_trace(
    FILE *output,
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_frame_wait_state *frame_wait,
    size_t step_count
)
{
    size_t index = 0u;

    fprintf(output, "## Dynamic reference trace\n\n");
    fprintf(output,
            "This trace starts from the exact checkpoint and applies the same "
            "host frame-wait scheduler used by the recovered runtime.\n\n");

    for (index = 0u; index < step_count; ++index) {
        vf2_i960_cpu before = *cpu;
        vf2_i960_trace_event event;
        vf2_hybrid_frame_wait_report wait_report;
        char text[256];
        vf2_status status = VF2_OK;
        vf2_status format_status = VF2_OK;

        memset(&event, 0, sizeof(event));
        memset(&wait_report, 0, sizeof(wait_report));
        status = vf2_i960_step(cpu, machine, &event);
        if (event.instruction.valid) {
            format_status = vf2_i960_format_instruction(
                &event.instruction, text, sizeof(text)
            );
        } else {
            (void)snprintf(text, sizeof(text), "<invalid instruction>");
        }
        if (format_status != VF2_OK) {
            return format_status;
        }

        fprintf(
            output,
            "%zu. `0x%08x` → `0x%08x`: `%s`",
            index,
            (unsigned)event.ip_before,
            (unsigned)cpu->ip,
            text
        );
        if (status != VF2_OK) {
            fprintf(output, " — **step failed: %s**\n", vf2_status_string(status));
            return status;
        }
        fputc('\n', output);

        status = vf2_hybrid_frame_wait_observe(
            machine, cpu, frame_wait, &wait_report
        );
        if (status != VF2_OK) {
            fprintf(output, "  - host frame-wait failed: `%s`\n",
                    vf2_status_string(status));
            return status;
        }
        print_cpu_deltas(output, &before, cpu);
        if (wait_report.wait_observed) {
            fprintf(
                output,
                "  - frame-wait visit: %zu%s; resulting IP `0x%08x`\n",
                wait_report.visit_count,
                wait_report.interrupt_injected ? " (IRQ 12 injected)" : "",
                (unsigned)cpu->ip
            );
        }
    }
    fputc('\n', output);
    return VF2_OK;
}

int main(int argc, char **argv)
{
    vf2_recover_options options;
    vf2_verify_summary verify_summary;
    vf2_i960_snapshot snapshot;
    vf2_native_runtime_state runtime_state;
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    uint8_t *main_rom = NULL;
    size_t main_rom_size = 0u;
    uint8_t *main_data = NULL;
    size_t main_data_size = 0u;
    char *derived_state_path = NULL;
    const char *state_path = NULL;
    FILE *output = stdout;
    int machine_initialized = 0;
    int show_help = 0;
    vf2_status status = VF2_OK;

    memset(&verify_summary, 0, sizeof(verify_summary));
    memset(&runtime_state, 0, sizeof(runtime_state));
    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    vf2_i960_snapshot_init(&snapshot);

    if (!parse_options(argc, argv, &options, &show_help)) {
        print_usage(stderr, argv[0]);
        return EXIT_FAILURE;
    }
    if (show_help) {
        print_usage(stdout, argv[0]);
        return EXIT_SUCCESS;
    }

    state_path = options.runtime_state_path;
    if (state_path == NULL) {
        derived_state_path = append_suffix(options.snapshot_path, ".runtime");
        if (derived_state_path == NULL) {
            status = VF2_ERROR_OUT_OF_MEMORY;
        } else {
            state_path = derived_state_path;
        }
    }
    if (status == VF2_OK && options.output_path != NULL) {
        output = fopen(options.output_path, "w");
        if (output == NULL) {
            status = VF2_ERROR_IO;
        }
    }
    if (status == VF2_OK) {
        status = vf2_romset_verify(
            options.rom_directory, NULL, &verify_summary
        );
    }
    if (status == VF2_OK) {
        status = vf2_romset_build_region(
            options.rom_directory,
            VF2_REGION_MAINCPU,
            &main_rom,
            &main_rom_size
        );
    }
    if (status == VF2_OK) {
        status = vf2_romset_build_region(
            options.rom_directory,
            VF2_REGION_MAIN_DATA,
            &main_data,
            &main_data_size
        );
    }
    if (status == VF2_OK) {
        status = initialize_machine(
            &machine,
            main_rom,
            main_rom_size,
            main_data,
            main_data_size
        );
        machine_initialized = status == VF2_OK;
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_read_file(&snapshot, options.snapshot_path);
    }
    if (status == VF2_OK) {
        status = vf2_native_runtime_state_read_file(
            &runtime_state, state_path
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_restore(&snapshot, &cpu, &machine);
    }

    if (status == VF2_OK) {
        fprintf(output, "# VF2 recovery report\n\n");
        fprintf(output, "- Tool version: `%s`\n", VF2_VERSION_STRING);
        fprintf(output, "- Snapshot: `%s`\n", options.snapshot_path);
        fprintf(output, "- Runtime state: `%s`\n", state_path);
        fprintf(output, "- ROM verification: `%zu/%zu` valid\n",
                verify_summary.valid, verify_summary.total);
        fprintf(output, "- Checkpoint IP: `0x%08x`\n\n",
                (unsigned)snapshot.cpu.ip);

        print_registers(output, &cpu);
        print_runtime_state(output, &runtime_state);
        status = print_disassembly(
            output,
            main_rom,
            main_rom_size,
            snapshot.cpu.ip,
            options.disasm_count
        );
    }
    if (status == VF2_OK) {
        vf2_hybrid_frame_wait_state frame_wait = runtime_state.frame_wait;
        status = print_reference_trace(
            output,
            &machine,
            &cpu,
            &frame_wait,
            options.trace_count
        );
    }
    if (status == VF2_OK) {
        fprintf(output, "## Recovery target\n\n");
        fprintf(output,
                "Implement the recovered native block beginning at `0x%08x`, "
                "then re-run `vf2cycles` from this checkpoint. Do not record a "
                "new dispatch boundary until the differential state matches.\n",
                (unsigned)snapshot.cpu.ip);
    }

    if (options.output_path != NULL && output != stdout) {
        if (fclose(output) != 0 && status == VF2_OK) {
            status = VF2_ERROR_IO;
        }
        output = NULL;
    }
    if (status != VF2_OK) {
        fprintf(stderr, "Recovery report failed: %s\n", vf2_status_string(status));
    } else if (options.output_path != NULL) {
        printf("Recovery report: %s\n", options.output_path);
    }

    if (machine_initialized) {
        vf2_model2a_shutdown(&machine);
    }
    vf2_i960_snapshot_destroy(&snapshot);
    free(main_data);
    free(main_rom);
    free(derived_state_path);
    return status == VF2_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
