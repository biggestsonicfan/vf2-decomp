static int scratch_test_prepare_machine(vf2_model2a *machine)
{
    const uint32_t selector_entry =
        VF2_PLAYER_SELECTOR_SCRATCH_TABLE +
        (uint32_t)SCRATCH_TEST_SELECTOR * UINT32_C(4);
    const uint8_t selector_raw[2] = {
        (uint8_t)SCRATCH_TEST_SELECTOR,
        (uint8_t)(SCRATCH_TEST_SELECTOR >> 8u)
    };

    memset(machine, 0, sizeof(*machine));
    memset(scratch_test_main_rom, 0, sizeof(scratch_test_main_rom));
    memset(scratch_test_main_data, 0, sizeof(scratch_test_main_data));
    if (!vf2_model2a_initialize(machine)) {
        return 0;
    }
    memset(machine->work_ram, 0, machine->work_ram_size);
    if (vf2_model2a_attach_main_rom(
            machine, scratch_test_main_rom,
            sizeof(scratch_test_main_rom)) != VF2_OK ||
        vf2_model2a_attach_main_data(
            machine, scratch_test_main_data,
            sizeof(scratch_test_main_data)) != VF2_OK) {
        vf2_model2a_shutdown(machine);
        return 0;
    }

    scratch_test_main_data_u32(selector_entry, SCRATCH_TEST_TABLE);
    CHECK(vf2_model2a_write(
        machine, SCRATCH_TEST_PLAYER + UINT32_C(0x1a8),
        selector_raw, sizeof(selector_raw)) == VF2_OK);
    CHECK(vf2_model2a_write_u32(
        machine, SCRATCH_TEST_PLAYER + UINT32_C(0xbd8),
        SCRATCH_TEST_BASE) == VF2_OK);
    return 1;
}

static void scratch_test_install_raw(const uint8_t raw[20])
{
    size_t index = 0u;
    for (index = 0u; index < 20u; ++index) {
        scratch_test_main_data_u8(
            SCRATCH_TEST_TABLE + UINT32_C(2) + (uint32_t)index,
            raw[index]
        );
    }
}

static int scratch_test_locate(
    const vf2_model2a *machine,
    vf2_player_selector_scratch_plan *plan,
    uint32_t *count_pointer
)
{
    const vf2_status status = player_selector_scratch_locate(
        machine, SCRATCH_TEST_PLAYER, plan, count_pointer
    );
    CHECK(status == VF2_OK);
    return status == VF2_OK;
}
