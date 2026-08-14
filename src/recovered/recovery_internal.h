#ifndef VF2_RECOVERY_INTERNAL_H
#define VF2_RECOVERY_INTERNAL_H

#include "vf2/hybrid.h"
#include "vf2/model2a.h"

#include <stdint.h>

/* Native runtime scheduler entry is routed through a narrow wrapper so newly
 * recovered initializer tasks can be composed without broadening the generic
 * second-scheduler task whitelist. Other translation units continue to call
 * the public hybrid scheduler directly. */
vf2_status vf2_native_second_scheduler_enter(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_second_scheduler_report *report
);
#define vf2_hybrid_second_scheduler_enter vf2_native_second_scheduler_enter

/* Native runtime must also pass post-frame bridges through the public recovery
 * wrapper.  The low-level implementation intentionally omits condition and
 * selector-specific poststate reconstruction; calling it directly makes the
 * repeated runtime diverge from the standalone differential bridge. */
static inline vf2_status vf2_native_post_frame_bridge_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report
)
{
    return vf2_hybrid_post_frame_bridge_execute(machine, cpu, report);
}
#define vf2_hybrid_post_frame_bridge_execute vf2_native_post_frame_bridge_execute

static inline vf2_status vf2_recovered_table_crc16(
    const vf2_model2a *machine,
    uint32_t source,
    uint32_t stride,
    uint32_t count,
    uint16_t *result
)
{
    uint32_t index = 0u;
    uint16_t crc = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || result == NULL || count == 0u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0u; index < count; ++index) {
        uint8_t raw = 0u;
        uint8_t table_bytes[2] = {0u, 0u};
        uint16_t table_value = 0u;
        const uint16_t high = (uint16_t)((uint32_t)crc << 8u);

        crc = (uint16_t)(crc >> 8u);
        status = vf2_model2a_read(machine, source, &raw, sizeof(raw));
        if (status != VF2_OK) {
            return status;
        }
        source += UINT32_C(1) + stride;
        crc ^= (uint16_t)raw;
        status = vf2_model2a_read(
            machine,
            UINT32_C(0x02000000) +
                (uint32_t)(crc & UINT16_C(0x00ff)) * UINT32_C(2),
            table_bytes,
            sizeof(table_bytes)
        );
        if (status != VF2_OK) {
            return status;
        }
        table_value = (uint16_t)((uint16_t)table_bytes[0] |
                                 ((uint16_t)table_bytes[1] << 8u));
        crc = (uint16_t)(table_value ^ high);
    }
    *result = crc;
    return VF2_OK;
}

#endif
