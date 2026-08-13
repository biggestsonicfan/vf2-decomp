static void scratch_test_code0_triple_copy_plan(void)
{
    vf2_model2a machine;
    vf2_player_selector_scratch_plan plan;
    uint32_t count_pointer = 0u;
    uint8_t raw[20] = {0u};

    raw[0] = UINT8_C(0x40);
    CHECK(scratch_test_prepare_machine(&machine));
    if (machine.work_ram == NULL) {
        return;
    }
    scratch_test_install_raw(raw);
    CHECK(scratch_test_locate(&machine, &plan, &count_pointer));
    CHECK(plan.payload_start == SCRATCH_TEST_TABLE + UINT32_C(24));

    scratch_test_main_data_u32(plan.payload_start, UINT32_C(0x11111111));
    scratch_test_main_data_u32(
        plan.payload_start + UINT32_C(4), UINT32_C(0x22222222)
    );
    scratch_test_main_data_u32(
        plan.payload_start + UINT32_C(8), UINT32_C(0x33333333)
    );

    CHECK(player_selector_scratch_preflight(
        &machine, &plan, 0u, count_pointer
    ) == VF2_OK);
    CHECK(plan.operation_count == 1u);
    CHECK(plan.operations[0].kind == VF2_PLAYER_SELECTOR_SCRATCH_COPY_TRIPLE);
    CHECK(plan.operations[0].target ==
          SCRATCH_TEST_PLAYER + UINT32_C(0x80));
    CHECK(plan.operations[0].raw[0] == UINT32_C(0x11111111));
    CHECK(plan.operations[0].raw[1] == UINT32_C(0x22222222));
    CHECK(plan.operations[0].raw[2] == UINT32_C(0x33333333));
    CHECK(plan.payload_end == plan.payload_start + UINT32_C(12));
    vf2_model2a_shutdown(&machine);
}

static void scratch_test_code1_dynamic_cvtri_plan(void)
{
    vf2_model2a machine;
    vf2_player_selector_scratch_plan plan;
    uint32_t count_pointer = 0u;
    uint8_t raw[20] = {0u};

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
        plan.payload_start, scratch_test_float_bits(1.5f)
    );
    scratch_test_main_data_u32(
        plan.payload_start + UINT32_C(4), scratch_test_float_bits(-2.5f)
    );
    scratch_test_main_data_u32(
        plan.payload_start + UINT32_C(8), scratch_test_float_bits(3.4f)
    );

    CHECK(player_selector_scratch_preflight(
        &machine, &plan, 0u, count_pointer
    ) == VF2_OK);
    CHECK(plan.operation_count == 1u);
    CHECK(plan.operations[0].kind ==
          VF2_PLAYER_SELECTOR_SCRATCH_STORE_SHORT3);
    CHECK(plan.operations[0].target ==
          SCRATCH_TEST_PLAYER + UINT32_C(0x200));
    CHECK(plan.operations[0].converted[0] == UINT16_C(2));
    CHECK(plan.operations[0].converted[1] == UINT16_C(0xfffe));
    CHECK(plan.operations[0].converted[2] == UINT16_C(3));
    CHECK(plan.dynamic_mask == UINT8_C(1));
    CHECK(plan.payload_end == plan.payload_start + UINT32_C(12));
    vf2_model2a_shutdown(&machine);
}
