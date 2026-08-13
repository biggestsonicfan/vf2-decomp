static void scratch_test_code5_code6_counts_and_fixed_apply(void)
{
    vf2_model2a machine;
    vf2_player_selector_scratch_plan plan;
    uint32_t count_pointer = 0u;
    uint8_t raw[20] = {0u};

    raw[0] = UINT8_C(0x0a);
    raw[1] = UINT8_C(0x0f);
    CHECK(scratch_test_prepare_machine(&machine));
    if (machine.work_ram == NULL) {
        return;
    }
    scratch_test_install_raw(raw);
    scratch_test_main_data_u8(
        SCRATCH_TEST_TABLE + UINT32_C(22), UINT8_C(2)
    );
    scratch_test_main_data_u8(
        SCRATCH_TEST_TABLE + UINT32_C(23), UINT8_C(3)
    );
    scratch_test_main_data_u8(
        SCRATCH_TEST_TABLE + UINT32_C(24), UINT8_C(1)
    );
    scratch_test_main_data_u8(
        SCRATCH_TEST_TABLE + UINT32_C(25), UINT8_C(2)
    );

    CHECK(scratch_test_locate(&machine, &plan, &count_pointer));
    CHECK(count_pointer == SCRATCH_TEST_TABLE + UINT32_C(22));
    CHECK(plan.aligned_count_pointer ==
          SCRATCH_TEST_TABLE + UINT32_C(28));
    CHECK(plan.payload_start == SCRATCH_TEST_TABLE + UINT32_C(60));
    CHECK(player_selector_scratch_preflight(
        &machine, &plan, 0u, count_pointer
    ) == VF2_OK);
    CHECK(plan.payload_end == plan.payload_start + UINT32_C(56));
    CHECK(plan.codes[0] == UINT8_C(5));
    CHECK(plan.codes[1] == UINT8_C(5));
    CHECK(plan.codes[2] == UINT8_C(3));
    CHECK(plan.codes[3] == UINT8_C(6));
    CHECK(plan.codes[4] == UINT8_C(6));
    CHECK(plan.codes[5] == UINT8_C(3));
    CHECK(plan.operation_count == 0u);

    CHECK(player_selector_scratch_apply_fixed(&machine, &plan) == VF2_OK);
    CHECK(scratch_test_read_u32(
        &machine, SCRATCH_TEST_BASE + UINT32_C(0x784)
    ) == SCRATCH_TEST_TABLE + UINT32_C(22));
    CHECK(scratch_test_read_u32(
        &machine, SCRATCH_TEST_BASE + UINT32_C(0x788)
    ) == SCRATCH_TEST_TABLE + UINT32_C(28));
    CHECK(scratch_test_read_u32(
        &machine, SCRATCH_TEST_BASE + UINT32_C(0x780)
    ) == SCRATCH_TEST_TABLE + UINT32_C(60));
    CHECK(scratch_test_read_u8(
        &machine, SCRATCH_TEST_BASE + UINT32_C(0x78c)
    ) == UINT8_C(5));
    CHECK(scratch_test_read_u8(
        &machine, SCRATCH_TEST_BASE + UINT32_C(0x78d)
    ) == UINT8_C(5));
    CHECK(scratch_test_read_u8(
        &machine, SCRATCH_TEST_PLAYER + UINT32_C(0xbdc)
    ) == 0u);
    vf2_model2a_shutdown(&machine);
}
