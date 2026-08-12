#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "vf2/hash.h"
#include "vf2/i960/executor.h"

#define VF2_PLAYER_POSE_ENTRY UINT32_C(0x00016504)
#define VF2_PLAYER_POSE_RETURN UINT32_C(0x00014418)
#define VF2_PLAYER_POSE_FINAL_RET UINT32_C(0x0001769c)
#define VF2_PLAYER_POSE_INSTRUCTIONS UINT64_C(1690)
#define VF2_PLAYER_POSE_PRE_RET_INSTRUCTIONS UINT64_C(1689)
#define VF2_PLAYER_POSE_NESTED_CALLS UINT64_C(16)
#define VF2_PLAYER_POSE_NESTED_RETURNS UINT64_C(16)
#define VF2_PLAYER_POSE_COPRO_ADDRESS UINT32_C(0x00884000)
#define VF2_PLAYER_POSE_BLOB_SIZE 3245u

static const uint32_t player_pose_entry_registers[VF2_I960_REGISTER_COUNT] = {
    0x005ff5c0u, 0x005ff640u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
    0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
    0x00000000u, 0x00000030u, 0x00550208u, 0x00000002u, 0x00000000u, 0x00511552u, 0x00029714u, 0x00510980u,
    0x00512980u, 0x010016acu, 0x00800000u, 0x00880000u, 0x00004000u, 0x00510980u, 0x00000220u, 0x005ff600u,
};
static const uint32_t player_pose_pre_ret_registers[VF2_I960_REGISTER_COUNT] = {
    0x005ff5c0u, 0x005ff640u, 0x000175f4u, 0x00000009u, 0x00000000u, 0x00000000u, 0x00000000u, 0x020080d0u,
    0x07800f0fu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x0201c2fcu, 0x00000011u, 0x00008a00u,
    0x0000000fu, 0x00000030u, 0x00550208u, 0x00000002u, 0x00000000u, 0x00511552u, 0x00029714u, 0x00510980u,
    0x00512980u, 0x010016acu, 0x00800000u, 0x00880000u, 0x00004000u, 0x00510980u, 0x00000220u, 0x005ff600u,
};
static const uint32_t player_pose_final_registers[VF2_I960_REGISTER_COUNT] = {
    0x005ff580u, 0x005ff600u, 0x00014418u, 0x00000505u, 0x00000000u, 0x84200000u, 0x00000000u, 0x00000000u,
    0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u,
    0x0000000fu, 0x00000030u, 0x00550208u, 0x00000002u, 0x00000000u, 0x00511552u, 0x00029714u, 0x00510980u,
    0x00512980u, 0x010016acu, 0x00800000u, 0x00880000u, 0x00004000u, 0x00510980u, 0x00000220u, 0x005ff5c0u,
};

/* Compact evidence blob:
 * P165, u8 op-dictionary-count, u16 op-count,
 * dictionary {u8(write|offset), u32 value}, u8 op-index[op-count],
 * then three patch sections (work/buffer/copro):
 * u8 run-count, {u32 offset,u16 size}[count], concatenated final bytes.
 */
static const char player_pose_blob_base64[] =
#include "player_i960_bridge_pose_blob.inc"
;

static uint16_t player_pose_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8u));
}

static uint32_t player_pose_u32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8u) |
           ((uint32_t)p[2] << 16u) |
           ((uint32_t)p[3] << 24u);
}

static int player_pose_b64_value(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return c == '=' ? -2 : -1;
}

static int player_pose_decode(uint8_t out[VF2_PLAYER_POSE_BLOB_SIZE])
{
    size_t source = 0u;
    size_t target = 0u;
    uint32_t accumulator = 0u;
    unsigned bits = 0u;

    while (player_pose_blob_base64[source] != '\0') {
        const int value = player_pose_b64_value(player_pose_blob_base64[source++]);
        if (value == -2) break;
        if (value < 0) return 0;
        accumulator = (accumulator << 6u) | (uint32_t)value;
        bits += 6u;
        if (bits >= 8u) {
            bits -= 8u;
            if (target >= VF2_PLAYER_POSE_BLOB_SIZE) return 0;
            out[target++] = (uint8_t)(accumulator >> bits);
            accumulator &= bits == 0u ? 0u : ((UINT32_C(1) << bits) - 1u);
        }
    }
    return target == VF2_PLAYER_POSE_BLOB_SIZE;
}

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

