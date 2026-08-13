#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "vf2/hash.h"
#include "vf2/i960/executor.h"

#define VF2_PLAYER_POSE_ENTRY UINT32_C(0x00016504)
#define VF2_PLAYER_POSE_RETURN UINT32_C(0x00014418)
#define VF2_PLAYER_POSE_FINAL_RET UINT32_C(0x0001769c)
#define VF2_PLAYER_POSE_INSTRUCTIONS UINT64_C(1690)

static const uint32_t player_pose_entry_registers[VF2_I960_REGISTER_COUNT] = {
    0x005ff5c0u, 0x005ff640u, 0x00000000u, 0x00000000u,
    0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
    0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
    0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
    0x00000000u, 0x00000030u, 0x00550208u, 0x00000002u,
    0x00000000u, 0x00511552u, 0x00029714u, 0x00510980u,
    0x00512980u, 0x010016acu, 0x00800000u, 0x00880000u,
    0x00004000u, 0x00510980u, 0x00000220u, 0x005ff600u,
};

static const uint32_t player_pose_final_registers[VF2_I960_REGISTER_COUNT] = {
    0x005ff580u, 0x005ff600u, 0x00014418u, 0x00000505u,
    0x00000000u, 0x84200000u, 0x00000000u, 0x00000000u,
    0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
    0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u,
    0x0000000fu, 0x00000030u, 0x00550208u, 0x00000002u,
    0x00000000u, 0x00511552u, 0x00029714u, 0x00510980u,
    0x00512980u, 0x010016acu, 0x00800000u, 0x00880000u,
    0x00004000u, 0x00510980u, 0x00000220u, 0x005ff5c0u,
};

static int player_pose_crc_matches(
    const vf2_model2a *machine,
    uint32_t geometry_crc,
    uint32_t copro_crc,
    uint32_t work_crc,
    uint32_t buffer_crc
)
{
    return machine != NULL &&
           machine->geometry != NULL && machine->copro_port != NULL &&
           machine->work_ram != NULL && machine->buffer_ram != NULL &&
           vf2_crc32(machine->geometry, machine->geometry_size) == geometry_crc &&
           vf2_crc32(machine->copro_port, machine->copro_port_size) == copro_crc &&
           vf2_crc32(machine->work_ram, machine->work_ram_size) == work_crc &&
           vf2_crc32(machine->buffer_ram, machine->buffer_ram_size) == buffer_crc;
}

static int player_pose_entry_matches(
    const vf2_model2a *machine,
    const vf2_i960_cpu *cpu
)
{
    if (machine == NULL || cpu == NULL ||
        cpu->ip != VF2_PLAYER_POSE_ENTRY ||
        cpu->local_frame_depth != 3u ||
        cpu->local_frames[2].registers[2] != VF2_PLAYER_POSE_RETURN ||
        cpu->process_control != UINT32_C(0x001f0000) ||
        cpu->arithmetic_control != UINT32_C(0x3f001004) ||
        cpu->compare_result != VF2_I960_COMPARE_LESS ||
        memcmp(cpu->registers, player_pose_entry_registers,
               sizeof(player_pose_entry_registers)) != 0) {
        return 0;
    }
    return player_pose_crc_matches(
        machine,
        UINT32_C(0xae84ffcb), UINT32_C(0xce011c3a),
        UINT32_C(0x526b0ab0), UINT32_C(0xe9dc35e8)
    );
}
