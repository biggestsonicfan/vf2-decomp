static void scratch_test_code2_skips_second_float(void)
{
    vf2_model2a machine;
    vf2_player_selector_scratch_plan plan;
    uint32_t count_pointer = 0u;
    uint8_t raw[20] = {0u};

    raw[13] = UINT8_C(0xc0);
    CHECK(scratch_test_prepare_machine(&machine));
    if (machine.work_ram == NULL) {
        return;
    }
    scratch_test_main_rom_u32(
        VF2_PLAYER_SELECTOR_DYNAMIC_TABLE + UINT32_C(4),
        UINT32_C(0x220)
    );
    scratch_test_install_raw(raw);
    CHECK(scratch_test_locate(&machine, &plan, &count_pointer));

    scratch_test_main_data_u32(
        plan.payload_start, scratch_test_float_bits(1.2f)
    );
    scratch_test_main_data_u32(
        plan.payload_start + UINT32_C(4), UINT32_C(0x7fc00000)
    );
    scratch_test_main_data_u32(
        plan.payload_start + UINT32_C(8), scratch_test_float_bits(2.2f)
    );
    scratch_test_main_data_u32(
        plan.payload_start + UINT32_C(12), scratch_test_float_bits(3.2f)
    );
    scratch_test_main_data_u32(
        plan.payload_start + UINT32_C(16), scratch_test_float_bits(4.2f)
    );

    CHECK(player_selector_scratch_preflight(
        &machine, &plan, 0u, count_pointer
    ) == VF2_OK);
    CHECK(plan.operation_count == 1u);
    CHECK(plan.operations[0].kind ==
          VF2_PLAYER_SELECTOR_SCRATCH_STORE_SHORT4);
    CHECK(plan.operations[0].target ==
          SCRATCH_TEST_PLAYER + UINT32_C(0x220));
    CHECK(plan.operations[0].converted[0] == UINT16_C(1));
    CHECK(plan.operations[0].converted[1] == UINT16_C(2));
    CHECK(plan.operations[0].converted[2] == UINT16_C(3));
    CHECK(plan.operations[0].converted[3] == UINT16_C(4));
    CHECK(plan.dynamic_mask == UINT8_C(2));
    CHECK(plan.payload_end == plan.payload_start + UINT32_C(20));
    vf2_model2a_shutdown(&machine);
}

static void scratch_test_nonfinite_preflight_is_transactional(void)
{
    vf2_model2a machine;
    vf2_player_selector_scratch_plan plan;
    uint32_t count_pointer = 0u;
    uint8_t raw[20] = {0u};
    const uint8_t sentinel = UINT8_C(0x5a);

    raw[12] = UINT8_C(0x80);
    CHECK(scratch_test_prepare_machine(&machine));
    if (machine.work_ram == NULL) {
        return;
    }
    scratch_test_main_rom_u32(
        VF2_PLAYER_SELECTOR_DYNAMIC_TABLE, UINT32_C(0x200)
    );
    scratch_test_install_raw(raw);
    CHECK(scratch_test_locate(&machine, &plan, &count_pointer));

    scratch_test_main_data_u32(
        plan.payload_start, UINT32_C(0x7fc00000)
    );
    scratch_test_main_data_u32(
        plan.payload_start + UINT32_C(4), scratch_test_float_bits(2.0f)
    );
    scratch_test_main_data_u32(
        plan.payload_start + UINT32_C(8), scratch_test_float_bits(3.0f)
    );

    CHECK(vf2_model2a_write_u32(
        &machine, SCRATCH_TEST_BASE + UINT32_C(0x780),
        UINT32_C(0xaaaaaaaa)
    ) == VF2_OK);
    CHECK(vf2_model2a_write(
        &machine, SCRATCH_TEST_PLAYER + UINT32_C(0xbdc),
        &sentinel, sizeof(sentinel)
    ) == VF2_OK);

    CHECK(player_selector_scratch_preflight(
        &machine, &plan, 0u, count_pointer
    ) == VF2_ERROR_UNSUPPORTED);
    CHECK(scratch_test_read_u32(
        &machine, SCRATCH_TEST_BASE + UINT32_C(0x780)
    ) == UINT32_C(0xaaaaaaaa));
    CHECK(scratch_test_read_u8(
        &machine, SCRATCH_TEST_PLAYER + UINT32_C(0xbdc)
    ) == UINT8_C(0x5a));
    CHECK(scratch_test_read_u16(
        &machine, SCRATCH_TEST_PLAYER + UINT32_C(0x200)
    ) == 0u);
    vf2_model2a_shutdown(&machine);
}
