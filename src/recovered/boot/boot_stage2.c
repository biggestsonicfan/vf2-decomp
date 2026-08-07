#include "vf2/recovered.h"

#include <string.h>

static vf2_status read_u8(const vf2_model2a *machine, uint32_t address, uint8_t *value)
{
    return vf2_model2a_read(machine, address, value, 1u);
}

static vf2_status read_u16(const vf2_model2a *machine, uint32_t address, uint16_t *value)
{
    uint8_t data[2];
    vf2_status status = vf2_model2a_read(machine, address, data, sizeof(data));
    if (status == VF2_OK) {
        *value = (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8u));
    }
    return status;
}

static vf2_status write_u8(vf2_model2a *machine, uint32_t address, uint8_t value)
{
    return vf2_model2a_write(machine, address, &value, 1u);
}

static vf2_status write_u16(vf2_model2a *machine, uint32_t address, uint16_t value)
{
    uint8_t data[2];
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8u);
    return vf2_model2a_write(machine, address, data, sizeof(data));
}

static uint16_t scaled_channel(uint32_t product, uint8_t multiplier, uint8_t bias)
{
    uint32_t value = product;
    if (value != 0u) {
        value = ((uint32_t)multiplier * value) >> 8u;
        value += bias;
        if (value >= 256u) {
            return UINT16_MAX;
        }
    }
    return (uint16_t)value;
}

static vf2_status build_palette_tables(vf2_model2a *machine, size_t *written_entries)
{
    uint8_t red_bias = 0u;
    uint8_t red_multiplier = 0u;
    uint8_t green_bias = 0u;
    uint8_t green_multiplier = 0u;
    uint8_t blue_bias = 0u;
    uint8_t blue_multiplier = 0u;
    uint8_t ramp_multiplier = 0u;
    uint32_t bank = 0u;
    uint32_t entry = 0u;
    uint32_t base = VF2_PALETTE_RAM_BASE;
    vf2_status status = VF2_OK;

    status = read_u8(machine, VF2_WORK_RAM_BASE + 0x234u, &red_bias);
    if (status == VF2_OK) {
        status = read_u8(machine, VF2_WORK_RAM_BASE + 0x235u, &red_multiplier);
    }
    if (status == VF2_OK) {
        status = read_u8(machine, VF2_WORK_RAM_BASE + 0x236u, &green_bias);
    }
    if (status == VF2_OK) {
        status = read_u8(machine, VF2_WORK_RAM_BASE + 0x237u, &green_multiplier);
    }
    if (status == VF2_OK) {
        status = read_u8(machine, VF2_WORK_RAM_BASE + 0x238u, &blue_bias);
    }
    if (status == VF2_OK) {
        status = read_u8(machine, VF2_WORK_RAM_BASE + 0x239u, &blue_multiplier);
    }
    if (status == VF2_OK) {
        status = read_u8(machine, VF2_WORK_RAM_BASE + 0x23au, &ramp_multiplier);
    }

    for (bank = 0u; status == VF2_OK && bank < 32u; ++bank) {
        for (entry = 0u; status == VF2_OK && entry < 64u; ++entry) {
            const uint32_t product = bank * entry;
            status = write_u16(machine, base + 0x10000u,
                               scaled_channel(product, red_multiplier, red_bias));
            if (status == VF2_OK) {
                status = write_u16(machine, base + 0x14000u,
                                   scaled_channel(product, green_multiplier, green_bias));
            }
            if (status == VF2_OK) {
                status = write_u16(machine, base + 0x18000u,
                                   scaled_channel(product, blue_multiplier, blue_bias));
            }
            base += 2u;
        }

        if (status == VF2_OK) {
            const uint32_t product = bank * ramp_multiplier;
            const uint16_t red = scaled_channel(product, red_multiplier, red_bias);
            const uint16_t green = scaled_channel(product, green_multiplier, green_bias);
            const uint16_t blue = scaled_channel(product, blue_multiplier, blue_bias);
            uint32_t repeat = 0u;
            for (repeat = 0u; status == VF2_OK && repeat < 64u; ++repeat) {
                status = write_u16(machine, base + 0x10000u, red);
                if (status == VF2_OK) {
                    status = write_u16(machine, base + 0x14000u, green);
                }
                if (status == VF2_OK) {
                    status = write_u16(machine, base + 0x18000u, blue);
                }
                base += 2u;
            }
            base += 0x100u;
        }
    }

    if (written_entries != NULL) {
        *written_entries = 32u * 128u * 3u;
    }
    return status;
}

static vf2_status initialize_color_translation(vf2_model2a *machine, size_t *written_entries)
{
    uint32_t index = 0u;
    vf2_status status = VF2_OK;
    for (index = 0u; status == VF2_OK && index < 128u; ++index) {
        status = write_u16(
            machine,
            VF2_LUMA_RAM_BASE + index * 2u,
            (uint16_t)((index + 1u) >> 1u)
        );
    }
    if (written_entries != NULL) {
        *written_entries = index;
    }
    return status;
}

static vf2_status initialize_tile_memory(vf2_model2a *machine, size_t *cleared_halfwords)
{
    uint32_t index = 0u;
    vf2_status status = write_u16(machine, VF2_WORK_RAM_BASE + 0x3050u, UINT16_C(0xffac));
    if (status == VF2_OK) {
        status = write_u16(machine, VF2_TILE_RAM_BASE + 0x40000u, UINT16_C(0xffac));
    }
    if (status == VF2_OK) {
        status = write_u16(machine, VF2_WORK_RAM_BASE + 0x3052u, UINT16_C(0xfffd));
    }
    if (status == VF2_OK) {
        status = write_u16(machine, VF2_TILE_RAM_BASE + 0x60000u, UINT16_C(0xfffd));
    }
    for (index = 0u; status == VF2_OK && index < 0x5008u; ++index) {
        status = write_u16(machine, VF2_TILE_RAM_BASE + index * 2u, 0u);
    }
    for (index = 0u; status == VF2_OK && index < 0x1000u; ++index) {
        status = write_u16(machine, VF2_TILE_RAM_BASE + 0xc000u + index * 2u, 0u);
    }
    if (cleared_halfwords != NULL) {
        *cleared_halfwords = 0x5008u + 0x1000u;
    }
    return status;
}

static vf2_status install_boot_palette(vf2_model2a *machine, size_t *copied_halfwords)
{
    uint32_t index = 0u;
    vf2_status status = VF2_OK;
    for (index = 0u; status == VF2_OK && index < 16u; ++index) {
        uint16_t value = 0u;
        status = read_u16(machine, 0x000038bcu + index * 2u, &value);
        if (status == VF2_OK) {
            status = write_u16(machine, VF2_PALETTE_RAM_BASE + index * 2u, value);
        }
        if (status == VF2_OK) {
            status = write_u16(machine, VF2_PALETTE_RAM_BASE + 0x2000u + index * 2u, value);
        }
    }
    if (copied_halfwords != NULL) {
        *copied_halfwords = index * 2u;
    }
    return status;
}

vf2_status vf2_recovered_boot_stage2_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_recovered_boot_stage2_report *report
)
{
    uint32_t flags = 0u;
    uint32_t interrupt_control = 0u;
    uint8_t board_probe = 1u;
    const uint32_t incoming_arithmetic_control = cpu->arithmetic_control;
    vf2_status status = VF2_OK;
    vf2_recovered_boot_stage2_report local_report;

    if (machine == NULL || cpu == NULL || machine->main_rom == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(&local_report, 0, sizeof(local_report));

    status = vf2_model2a_read_u32(machine, VF2_WORK_RAM_BASE + 0x68u, &flags);
    if (status == VF2_OK) {
        flags &= ~UINT32_C(0x80000000);
        status = vf2_model2a_write_u32(machine, VF2_WORK_RAM_BASE + 0x68u, flags);
    }
    if (status == VF2_OK) {
        status = write_u8(machine, VF2_IO_CONTROL_BASE + 0x24u, board_probe);
    }
    if (status == VF2_OK) {
        status = read_u8(machine, VF2_IO_CONTROL_BASE + 0x24u, &board_probe);
    }
    if (status == VF2_OK) {
        status = write_u8(machine, VF2_WORK_RAM_BASE + 0xf00u, (uint8_t)(board_probe == 1u ? 0u : 1u));
    }

    if (status == VF2_OK) {
        static const uint16_t reset_sequence[] = {0u, 0u, 0u, 64u, 78u};
        size_t index = 0u;
        for (index = 0u; status == VF2_OK && index < sizeof(reset_sequence) / sizeof(reset_sequence[0]); ++index) {
            status = write_u16(machine, VF2_IO_CONTROL_BASE + 0x80002u, reset_sequence[index]);
        }
        local_report.io_reset_writes = sizeof(reset_sequence) / sizeof(reset_sequence[0]);
    }
    if (status == VF2_OK) {
        status = write_u8(machine, VF2_IO_CONTROL_BASE + 0x40u, 0u);
    }
    if (status == VF2_OK) {
        status = write_u8(machine, VF2_IO_CONTROL_BASE + 0x24u, 1u);
    }
    if (status == VF2_OK) {
        status = write_u8(machine, VF2_IO_CONTROL_BASE + 0x34u, (uint8_t)'S');
    }
    if (status == VF2_OK) {
        status = write_u8(machine, VF2_IO_CONTROL_BASE + 0x36u, (uint8_t)'E');
    }
    if (status == VF2_OK) {
        status = write_u8(machine, VF2_IO_CONTROL_BASE + 0x38u, (uint8_t)'G');
    }
    if (status == VF2_OK) {
        status = write_u8(machine, VF2_IO_CONTROL_BASE + 0x3au, (uint8_t)'A');
    }
    if (status == VF2_OK) {
        status = write_u16(machine, VF2_COPRO_CONTROL_BASE, 4u);
    }
    if (status == VF2_OK) {
        status = build_palette_tables(machine, &local_report.palette_entries_written);
    }
    if (status == VF2_OK) {
        status = initialize_color_translation(machine, &local_report.color_entries_written);
    }
    if (status == VF2_OK) {
        status = initialize_tile_memory(machine, &local_report.tile_halfwords_cleared);
    }
    if (status == VF2_OK) {
        status = install_boot_palette(machine, &local_report.boot_palette_halfwords_written);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, 0x00003850u, &interrupt_control);
    }
    if (status == VF2_OK) {
        cpu->interrupt_control = interrupt_control;
        status = vf2_model2a_write_u32(machine, VF2_INTERRUPT_CONTROL_BASE, 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, VF2_INTERRUPT_CONTROL_BASE + 4u, 0x21u);
    }
    if (status != VF2_OK) {
        return status;
    }

    cpu->process_control = UINT32_C(0x001f0000);
    cpu->arithmetic_control = UINT32_C(0x3f001000);
    cpu->compare_result = VF2_I960_COMPARE_EQUAL;
    cpu->ip = UINT32_C(0x0000052c);
    cpu->executed_instructions += UINT64_C(182514);
    ++cpu->procedure_calls;
    ++cpu->procedure_returns;

    /* Like stage 1, the hardware-init prefix does not reset the register
     * file. Only materialize registers the ROM actually changes. This keeps
     * cold boot exact while also preserving the live caller context during
     * the phase-17 soft-reset handoff. r5 captures the arithmetic-control
     * value that was live on entry before stage 2 installs its new AC. */
    cpu->registers[2] = UINT32_C(0x00000410);
    cpu->registers[3] = 0u;
    cpu->registers[4] = UINT32_C(0xff1f917f);
    cpu->registers[5] = incoming_arithmetic_control;
    cpu->registers[7] = UINT32_C(0x0000ffff);
    cpu->registers[9] = 0u;
    cpu->registers[11] = UINT32_C(0x01c00024);
    cpu->registers[15] = UINT32_C(0x00000100);
    cpu->registers[30] = UINT32_C(0x00000220);

    local_report.start_address = UINT32_C(0x000001b0);
    local_report.stop_address = cpu->ip;
    local_report.interpreted_instruction_equivalent = UINT64_C(182514);
    if (report != NULL) {
        *report = local_report;
    }
    return VF2_OK;
}
