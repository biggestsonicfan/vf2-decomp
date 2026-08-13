#define VF2_PLAYER_17710_ROTATION_ENTRY UINT32_C(0x000178bc)
#define VF2_PLAYER_17710_ROTATION_TAIL_INSTRUCTIONS UINT64_C(23)

#include "player_i960_bridge_planar_rotation.inc"

static vf2_status player_execute_17710_rotation_tail(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint64_t max_steps
)
{
    const uint32_t player = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 7u] : 0u;
    const uint32_t port = cpu != NULL
        ? cpu->registers[VF2_I960_G0_REGISTER + 11u] +
          cpu->registers[VF2_I960_G0_REGISTER + 12u]
        : 0u;
    uint32_t base_x_bits = 0u;
    uint32_t base_z_bits = 0u;
    uint32_t position_x_bits = 0u;
    uint32_t position_z_bits = 0u;
    uint32_t delta_x_bits = 0u;
    uint32_t delta_z_bits = 0u;
    uint32_t rotated_x_bits = 0u;
    uint32_t rotated_z_bits = 0u;
    uint32_t new_x_bits = 0u;
    uint32_t new_z_bits = 0u;
    uint16_t current_angle = 0u;
    float base_x = 0.0f;
    float base_z = 0.0f;
    float position_x = 0.0f;
    float position_z = 0.0f;
    float delta_x = 0.0f;
    float delta_z = 0.0f;
    float rotated_x = 0.0f;
    float rotated_z = 0.0f;
    float new_x = 0.0f;
    float new_z = 0.0f;
    int32_t current_angle_signed = 0;
    int32_t status_sum = 0;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL ||
        cpu->ip != VF2_PLAYER_17710_ROTATION_ENTRY ||
        cpu->local_frame_depth < 3u || player == 0u || port == 0u ||
        cpu->local_frames[cpu->local_frame_depth - 1u].registers[2] !=
            VF2_PLAYER_17710_STOP ||
        (max_steps != 0u &&
         max_steps < VF2_PLAYER_17710_ROTATION_TAIL_INSTRUCTIONS)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    /* Preflight every read and destination before the first write. */
    status = vf2_model2a_read_u32(
        machine, player + UINT32_C(0x01f4), &base_x_bits
    );
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player + UINT32_C(0x0018), &position_x_bits
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player + UINT32_C(0x01fc), &base_z_bits
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(
            machine, player + UINT32_C(0x0020), &position_z_bits
        );
    }
    if (status == VF2_OK) {
        status = player_tail_read_u16(
            machine, player + UINT32_C(0x0026), &current_angle
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    base_x = player_planar_rotation_float(base_x_bits);
    base_z = player_planar_rotation_float(base_z_bits);
    position_x = player_planar_rotation_float(position_x_bits);
    position_z = player_planar_rotation_float(position_z_bits);
    if (!isfinite(base_x) || !isfinite(base_z) ||
        !isfinite(position_x) || !isfinite(position_z)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    /* i960 SUBR at 0x178c4/0x178d0 produces the relative X/Z pair. */
    delta_x = position_x - base_x;
    delta_z = position_z - base_z;
    if (!isfinite(delta_x) || !isfinite(delta_z)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    delta_x_bits = player_planar_rotation_bits(delta_x);
    delta_z_bits = player_planar_rotation_bits(delta_z);

    status = player_planar_rotation_apply(
        (int16_t)(uint16_t)cpu->registers[9],
        delta_x_bits,
        delta_z_bits,
        &rotated_x_bits,
        &rotated_z_bits
    );
    if (status != VF2_OK) {
        return status;
    }
    rotated_x = player_planar_rotation_float(rotated_x_bits);
    rotated_z = player_planar_rotation_float(rotated_z_bits);
    new_x = base_x + rotated_x;
    new_z = base_z + rotated_z;
    if (!isfinite(new_x) || !isfinite(new_z)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    new_x_bits = player_planar_rotation_bits(new_x);
    new_z_bits = player_planar_rotation_bits(new_z);

    current_angle_signed = (int32_t)(int16_t)current_angle;
    status_sum = current_angle_signed +
                 (int32_t)(int16_t)(uint16_t)cpu->registers[9];

    status = vf2_model2a_write_u32(
        machine, player + UINT32_C(0x0018), new_x_bits
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, player + UINT32_C(0x0020), new_z_bits
        );
    }
    if (status == VF2_OK) {
        status = player_tail_write_u16(
            machine, player + UINT32_C(0x0026), (uint16_t)status_sum
        );
    }
    if (status != VF2_OK) {
        return status;
    }

    /* Preserve the architectural register effects of 0x178bc..0x17914. */
    cpu->registers[3] = delta_x_bits;
    cpu->registers[4] = delta_z_bits;
    cpu->registers[5] = rotated_x_bits;
    cpu->registers[6] = rotated_z_bits;
    cpu->registers[13] = base_z_bits;
    cpu->registers[14] = (uint32_t)current_angle_signed;
    cpu->registers[15] = (uint32_t)status_sum;

    /* With a flat coprocessor port the final pre-read backing word is the Z
     * argument. Keep that observable compatibility state without pretending
     * that the flat RAM supplied the two rotation results. Capture callbacks
     * are intentionally not fed a fake TGP RPC here. */
    if (machine->copro_read_callback == NULL &&
        machine->copro_write_callback == NULL) {
        status = vf2_model2a_write_u32(machine, port, delta_z_bits);
        if (status != VF2_OK) {
            return status;
        }
    }

    cpu->ip = VF2_PLAYER_17710_RET;
    cpu->executed_instructions +=
        VF2_PLAYER_17710_ROTATION_TAIL_INSTRUCTIONS - UINT64_C(1);
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK) {
        return status;
    }
    ++cpu->executed_instructions;
    return cpu->ip == VF2_PLAYER_17710_STOP
        ? VF2_OK : VF2_ERROR_UNSUPPORTED;
}

/*
 * Transactional entry helper for rotation paths.
 *
 * The generalized scalar planner handles every non-TGP branch. If it rejects
 * because the live control flow falls through to 0x178bc, probe the read-only
 * prefix on a CPU copy with the architectural decoder. No memory is mutated
 * before 0x178bc, so a failed probe can be discarded safely. On success the
 * exact prefix CPU state (including r9 and condition codes) is committed and
 * only the TGP rotation tail is replaced by semantic C.
 */
static vf2_status player_execute_17710_rotation(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint64_t max_steps
)
{
    vf2_i960_cpu probe;
    vf2_i960_run_options options;
    vf2_i960_run_result result;
    const uint64_t start_count = cpu != NULL
        ? cpu->executed_instructions : 0u;
    uint64_t prefix_instructions = 0u;
    uint64_t remaining = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || cpu == NULL ||
        cpu->ip != VF2_PLAYER_17710_ENTRY ||
        cpu->local_frame_depth < 3u ||
        cpu->local_frames[cpu->local_frame_depth - 1u].registers[2] !=
            VF2_PLAYER_17710_STOP) {
        return VF2_ERROR_UNSUPPORTED;
    }

    probe = *cpu;
    memset(&options, 0, sizeof(options));
    memset(&result, 0, sizeof(result));
    options.stop_address = VF2_PLAYER_17710_ROTATION_ENTRY;
    options.max_steps = UINT64_C(192);
    options.stop_on_self_branch = true;
    status = vf2_i960_run(&probe, machine, &options, &result);
    if (status != VF2_OK ||
        result.halt_reason != VF2_I960_HALT_STOP_ADDRESS ||
        probe.ip != VF2_PLAYER_17710_ROTATION_ENTRY) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }

    prefix_instructions = probe.executed_instructions - start_count;
    if (max_steps != 0u &&
        (prefix_instructions > max_steps ||
         VF2_PLAYER_17710_ROTATION_TAIL_INSTRUCTIONS >
            max_steps - prefix_instructions)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    *cpu = probe;
    remaining = max_steps == 0u ? 0u : max_steps - prefix_instructions;
    return player_execute_17710_rotation_tail(machine, cpu, remaining);
}
