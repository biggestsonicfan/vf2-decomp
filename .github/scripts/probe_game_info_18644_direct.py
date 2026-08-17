from pathlib import Path

hybrid = Path("src/recovered/hybrid.c")
text = hybrid.read_text()
anchor = '''/* Translate the observed 0x181c0 -> 0x184ec conditional corridor. Controlled
'''
wrapper = '''vf2_status vf2_probe_game_info_18644(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t return_address
)
{
    return hybrid_execute_game_info_18644(machine, cpu, return_address);
}

'''
if anchor not in text:
    raise SystemExit("hybrid wrapper anchor not found")
text = text.replace(anchor, wrapper + anchor, 1)
hybrid.write_text(text)

main = Path("tools/vf2i960/main.c")
text = main.read_text()
anchor = '''int main(int argc, char **argv)
{
'''
probe = r'''extern vf2_status vf2_probe_game_info_18644(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t return_address
);

static int command_compare_game_info_18644_direct(
    const char *rom_directory,
    const char *snapshot_path
)
{
    uint8_t *image = NULL;
    size_t image_size = 0u;
    vf2_i960_boot_vectors vectors;
    vf2_model2a reference_machine;
    vf2_model2a native_machine;
    vf2_i960_cpu reference_cpu;
    vf2_i960_cpu native_cpu;
    vf2_i960_snapshot snapshot;
    vf2_i960_snapshot_diff diff;
    vf2_status status = VF2_OK;
    uint64_t steps = 0u;
    int reference_initialized = 0;
    int native_initialized = 0;

    memset(&reference_machine, 0, sizeof(reference_machine));
    memset(&native_machine, 0, sizeof(native_machine));
    memset(&reference_cpu, 0, sizeof(reference_cpu));
    memset(&native_cpu, 0, sizeof(native_cpu));
    memset(&diff, 0, sizeof(diff));
    vf2_i960_snapshot_init(&snapshot);

    status = load_maincpu(rom_directory, &image, &image_size, &vectors);
    if (status == VF2_OK) {
        status = initialize_boot_machine(
            rom_directory, &reference_machine, image, image_size
        );
        reference_initialized = status == VF2_OK;
    }
    if (status == VF2_OK) {
        status = initialize_boot_machine(
            rom_directory, &native_machine, image, image_size
        );
        native_initialized = status == VF2_OK;
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_read_file(&snapshot, snapshot_path);
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_restore(
            &snapshot, &reference_cpu, &reference_machine
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_snapshot_restore(
            &snapshot, &native_cpu, &native_machine
        );
    }
    if (status == VF2_OK && reference_cpu.ip != UINT32_C(0x000164ac)) {
        fprintf(stderr, "Direct probe requires IP 0x000164ac, got 0x%08x\n",
                (unsigned)reference_cpu.ip);
        status = VF2_ERROR_INVALID_ARGUMENT;
    }

    if (status == VF2_OK) {
        status = vf2_i960_step(&reference_cpu, &reference_machine, NULL);
    }
    if (status == VF2_OK && reference_cpu.ip != UINT32_C(0x00018644)) {
        status = VF2_ERROR_UNSUPPORTED;
    }
    while (status == VF2_OK &&
           reference_cpu.ip != UINT32_C(0x000164b0) && steps < UINT64_C(1024)) {
        status = vf2_i960_step(&reference_cpu, &reference_machine, NULL);
        ++steps;
    }
    if (status == VF2_OK && reference_cpu.ip != UINT32_C(0x000164b0)) {
        status = VF2_ERROR_UNSUPPORTED;
    }

    if (status == VF2_OK) {
        native_cpu.ip = UINT32_C(0x000164b0);
        status = vf2_probe_game_info_18644(
            &native_machine, &native_cpu, UINT32_C(0x000164b0)
        );
    }
    if (status == VF2_OK) {
        status = vf2_i960_compare_live_state(
            &reference_cpu, &reference_machine,
            &native_cpu, &native_machine, &diff
        );
    }
    if (status == VF2_OK &&
        (reference_cpu.executed_instructions != native_cpu.executed_instructions ||
         reference_cpu.procedure_calls != native_cpu.procedure_calls ||
         reference_cpu.procedure_returns != native_cpu.procedure_returns ||
         reference_cpu.maximum_local_frame_depth != native_cpu.maximum_local_frame_depth)) {
        fprintf(stderr,
                "Direct counters mismatch: ins=%llu/%llu calls=%llu/%llu returns=%llu/%llu max-depth=%u/%u\n",
                (unsigned long long)reference_cpu.executed_instructions,
                (unsigned long long)native_cpu.executed_instructions,
                (unsigned long long)reference_cpu.procedure_calls,
                (unsigned long long)native_cpu.procedure_calls,
                (unsigned long long)reference_cpu.procedure_returns,
                (unsigned long long)native_cpu.procedure_returns,
                reference_cpu.maximum_local_frame_depth,
                native_cpu.maximum_local_frame_depth);
        status = VF2_ERROR_UNSUPPORTED;
    }
    if (status == VF2_OK && !diff.equal) {
        fprintf(stderr,
                "Direct state mismatch: %s offset=0x%zx expected=0x%08x actual=0x%08x bytes=%zu\n",
                diff.component, diff.first_offset,
                (unsigned)diff.expected_value, (unsigned)diff.actual_value,
                diff.differing_bytes);
        status = VF2_ERROR_UNSUPPORTED;
    }

    if (status == VF2_OK) {
        printf("Game-info 0x18644 direct differential: MATCH instructions=%llu\n",
               (unsigned long long)(reference_cpu.executed_instructions -
                                    snapshot.cpu.executed_instructions));
    } else if (diff.component[0] != '\0') {
        fprintf(stderr,
                "Game-info 0x18644 direct differential failed: %s diff=%s offset=0x%zx\n",
                vf2_status_string(status), diff.component, diff.first_offset);
    } else {
        fprintf(stderr, "Game-info 0x18644 direct differential failed: %s\n",
                vf2_status_string(status));
    }

    vf2_i960_snapshot_destroy(&snapshot);
    if (native_initialized) {
        vf2_model2a_shutdown(&native_machine);
    }
    if (reference_initialized) {
        vf2_model2a_shutdown(&reference_machine);
    }
    free(image);
    return status == VF2_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}

'''
if anchor not in text:
    raise SystemExit("main probe anchor not found")
text = text.replace(anchor, probe + anchor, 1)
old = '''int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "native-nth-dispatch") == 0) {
'''
new = '''int main(int argc, char **argv)
{
    if (argc == 4 && strcmp(argv[1], "compare-game-info-18644-direct") == 0) {
        return command_compare_game_info_18644_direct(argv[2], argv[3]);
    }

    if (argc >= 2 && strcmp(argv[1], "native-nth-dispatch") == 0) {
'''
if old not in text:
    raise SystemExit("main command anchor not found")
text = text.replace(old, new, 1)
main.write_text(text)
