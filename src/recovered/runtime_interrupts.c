#include "vf2/recovered.h"

#include <string.h>

#define VF2_TIMER3_IRQ_MASK UINT32_C(0x00000020)
#define VF2_TIMER3_INDEX UINT32_C(3)
#define VF2_TIMER_RELOAD UINT32_C(0x000fffff)
#define VF2_RUNTIME_WAIT_FLAG UINT32_C(0x0050008c)
#define VF2_RUNTIME_TIMER_ENABLE UINT32_C(0x00000421)

static vf2_status write_u8(vf2_model2a *machine, uint32_t address, uint8_t value)
{
    return vf2_model2a_write(machine, address, &value, sizeof(value));
}

vf2_status vf2_recovered_timer_irq_dispatch(
    vf2_model2a *machine,
    vf2_recovered_timer_irq_report *report
)
{
    vf2_recovered_timer_irq_report local_report;
    uint32_t request = 0u;
    uint32_t enable = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(&local_report, 0, sizeof(local_report));
    status = vf2_model2a_get_interrupt_state(machine, &request, &enable);
    if (status != VF2_OK) {
        return status;
    }
    local_report.initial_request = request;
    local_report.initial_enable = enable;
    local_report.wait_flag_address = VF2_RUNTIME_WAIT_FLAG;

    /* This accepted recovery intentionally covers the proven VF2 runtime path:
     * timer 3 is enabled, request bit 5 is pending, and no other timer source is
     * selected by the handler's local scan. Broader interrupt combinations are
     * left unsupported until separately captured and compared. */
    if ((request & VF2_TIMER3_IRQ_MASK) == 0u ||
        enable != VF2_RUNTIME_TIMER_ENABLE) {
        return VF2_ERROR_UNSUPPORTED;
    }

    status = vf2_model2a_set_interrupt_enable(
        machine, enable & ~VF2_TIMER3_IRQ_MASK
    );
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_TIMER_BASE + VF2_TIMER3_INDEX * 4u, 0u
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_set_interrupt_enable(machine, enable);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine,
            VF2_TIMER_BASE + VF2_TIMER3_INDEX * 4u,
            VF2_TIMER_RELOAD
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, VF2_INTERRUPT_CONTROL_BASE, ~VF2_TIMER3_IRQ_MASK
        );
    }
    if (status == VF2_OK) {
        status = write_u8(machine, VF2_RUNTIME_WAIT_FLAG, 1u);
    }
    if (status != VF2_OK) {
        return status;
    }

    local_report.serviced_mask = VF2_TIMER3_IRQ_MASK;
    local_report.timer_index = VF2_TIMER3_INDEX;
    local_report.timer_reload = VF2_TIMER_RELOAD;
    local_report.interrupts_serviced = 1u;
    local_report.wait_released = 1;
    status = vf2_model2a_get_interrupt_state(
        machine, &local_report.final_request, &local_report.final_enable
    );
    if (status == VF2_OK && report != NULL) {
        *report = local_report;
    }
    return status;
}
