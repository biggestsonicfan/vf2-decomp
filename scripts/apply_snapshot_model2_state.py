from pathlib import Path


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text()
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected one match, found {count}: {old[:80]!r}")
    path.write_text(text.replace(old, new, 1))


header = Path("include/vf2/i960/snapshot.h")
replace_once(header, "#define VF2_I960_SNAPSHOT_VERSION 5u", "#define VF2_I960_SNAPSHOT_VERSION 6u")
replace_once(
    header,
    "typedef struct vf2_i960_snapshot {\n    vf2_i960_cpu cpu;\n",
    "typedef struct vf2_i960_snapshot {\n"
    "    vf2_i960_cpu cpu;\n"
    "    uint32_t geometry_write_start;\n"
    "    uint32_t geometry_read_start;\n"
    "    uint32_t geometry_control;\n"
    "    uint32_t geometry_program_count;\n",
)

snapshot = Path("src/i960/snapshot.c")
replace_once(
    snapshot,
    "    snapshot->cpu = *cpu;\n    writable_regions(snapshot, destination);\n",
    "    snapshot->cpu = *cpu;\n"
    "    snapshot->geometry_write_start = machine->geometry_write_start;\n"
    "    snapshot->geometry_read_start = machine->geometry_read_start;\n"
    "    snapshot->geometry_control = machine->geometry_control;\n"
    "    snapshot->geometry_program_count = machine->geometry_program_count;\n"
    "    writable_regions(snapshot, destination);\n",
)
replace_once(
    snapshot,
    "    for (index = 0u; index < VF2_SNAPSHOT_REGION_COUNT; ++index) {\n"
    "        memcpy((uint8_t *)destination[index].data, source[index].data, source[index].size);\n"
    "    }\n"
    "    return VF2_OK;\n"
    "}\n\n"
    "vf2_status vf2_i960_snapshot_write_file",
    "    for (index = 0u; index < VF2_SNAPSHOT_REGION_COUNT; ++index) {\n"
    "        memcpy((uint8_t *)destination[index].data, source[index].data, source[index].size);\n"
    "    }\n"
    "    machine->geometry_write_start = snapshot->geometry_write_start;\n"
    "    machine->geometry_read_start = snapshot->geometry_read_start;\n"
    "    machine->geometry_control = snapshot->geometry_control;\n"
    "    machine->geometry_program_count = snapshot->geometry_program_count;\n"
    "    return VF2_OK;\n"
    "}\n\n"
    "vf2_status vf2_i960_snapshot_write_file",
)
replace_once(
    snapshot,
    "    ok = ok && write_u32(file, VF2_SNAPSHOT_REGION_COUNT);\n",
    "    ok = ok && write_u32(file, snapshot->geometry_write_start);\n"
    "    ok = ok && write_u32(file, snapshot->geometry_read_start);\n"
    "    ok = ok && write_u32(file, snapshot->geometry_control);\n"
    "    ok = ok && write_u32(file, snapshot->geometry_program_count);\n"
    "    ok = ok && write_u32(file, VF2_SNAPSHOT_REGION_COUNT);\n",
)
replace_once(
    snapshot,
    "        !read_u32(file, &version) || version != VF2_I960_SNAPSHOT_VERSION ||\n",
    "        !read_u32(file, &version) || version < 5u ||\n"
    "        version > VF2_I960_SNAPSHOT_VERSION ||\n",
)
replace_once(
    snapshot,
    "    if (!read_u32(file, &region_count) || region_count != VF2_SNAPSHOT_REGION_COUNT) {\n",
    "    if (version >= 6u &&\n"
    "        (!read_u32(file, &snapshot->geometry_write_start) ||\n"
    "         !read_u32(file, &snapshot->geometry_read_start) ||\n"
    "         !read_u32(file, &snapshot->geometry_control) ||\n"
    "         !read_u32(file, &snapshot->geometry_program_count))) {\n"
    "        fclose(file);\n"
    "        vf2_i960_snapshot_destroy(snapshot);\n"
    "        return VF2_ERROR_IO;\n"
    "    }\n"
    "    if (!read_u32(file, &region_count) || region_count != VF2_SNAPSHOT_REGION_COUNT) {\n",
)
replace_once(
    snapshot,
    "    if (fclose(file) != 0 && status == VF2_OK) {\n",
    "    if (status == VF2_OK && version == 5u) {\n"
    "        if (snapshot->geometry_size < UINT32_C(0x300c) ||\n"
    "            snapshot->video_control_size < UINT32_C(12)) {\n"
    "            status = VF2_ERROR_BAD_SIZE;\n"
    "        } else {\n"
    "            const uint8_t *write_start = snapshot->geometry + UINT32_C(0x1008);\n"
    "            const uint8_t *read_start = snapshot->geometry + UINT32_C(0x3008);\n"
    "            const uint8_t *control = snapshot->video_control + UINT32_C(8);\n"
    "            snapshot->geometry_write_start =\n"
    "                ((uint32_t)write_start[0] | ((uint32_t)write_start[1] << 8u) |\n"
    "                 ((uint32_t)write_start[2] << 16u) | ((uint32_t)write_start[3] << 24u)) &\n"
    "                UINT32_C(0x000fffff);\n"
    "            snapshot->geometry_read_start =\n"
    "                ((uint32_t)read_start[0] | ((uint32_t)read_start[1] << 8u) |\n"
    "                 ((uint32_t)read_start[2] << 16u) | ((uint32_t)read_start[3] << 24u)) &\n"
    "                UINT32_C(0x000fffff);\n"
    "            snapshot->geometry_control =\n"
    "                (uint32_t)control[0] | ((uint32_t)control[1] << 8u) |\n"
    "                ((uint32_t)control[2] << 16u) | ((uint32_t)control[3] << 24u);\n"
    "            /* v5 did not serialize the transient program-word count. */\n"
    "            snapshot->geometry_program_count = 0u;\n"
    "        }\n"
    "    }\n"
    "    if (fclose(file) != 0 && status == VF2_OK) {\n",
)

live_anchor = """    if (expected_cpu->local_frame_depth != actual_cpu->local_frame_depth) {
        return VF2_OK;
    }
"""
live_insert = live_anchor + """    {
        const uint32_t expected_model2[4] = {
            expected_machine->geometry_write_start,
            expected_machine->geometry_read_start,
            expected_machine->geometry_control,
            expected_machine->geometry_program_count
        };
        const uint32_t actual_model2[4] = {
            actual_machine->geometry_write_start,
            actual_machine->geometry_read_start,
            actual_machine->geometry_control,
            actual_machine->geometry_program_count
        };
        for (index = 0u; index < 4u; ++index) {
            if (expected_model2[index] != actual_model2[index]) {
                if (diff->equal) {
                    diff->equal = false;
                    (void)snprintf(diff->component, sizeof(diff->component), "model2-state");
                    diff->first_offset = index;
                    diff->expected_value = expected_model2[index];
                    diff->actual_value = actual_model2[index];
                }
                ++diff->differing_bytes;
            }
        }
    }
"""
replace_once(snapshot, live_anchor, live_insert)

snapshot_compare_anchor = """    for (index = 0u; index < VF2_I960_REGISTER_COUNT; ++index) {
"""
# There are two register loops. Insert hardware comparison only in snapshot_compare by
# anchoring after its cpu-state block, which is unique near the function tail.
needle = """        ++diff->differing_bytes;
    }
    for (index = 0u; index < VF2_I960_REGISTER_COUNT; ++index) {
        if (expected->cpu.registers[index] != actual->cpu.registers[index]) {
"""
replacement = """        ++diff->differing_bytes;
    }
    {
        const uint32_t expected_model2[4] = {
            expected->geometry_write_start,
            expected->geometry_read_start,
            expected->geometry_control,
            expected->geometry_program_count
        };
        const uint32_t actual_model2[4] = {
            actual->geometry_write_start,
            actual->geometry_read_start,
            actual->geometry_control,
            actual->geometry_program_count
        };
        for (index = 0u; index < 4u; ++index) {
            if (expected_model2[index] != actual_model2[index]) {
                if (diff->equal) {
                    diff->equal = false;
                    (void)snprintf(diff->component, sizeof(diff->component), "model2-state");
                    diff->first_offset = index;
                    diff->expected_value = expected_model2[index];
                    diff->actual_value = actual_model2[index];
                }
                ++diff->differing_bytes;
            }
        }
    }
    for (index = 0u; index < VF2_I960_REGISTER_COUNT; ++index) {
        if (expected->cpu.registers[index] != actual->cpu.registers[index]) {
"""
replace_once(snapshot, needle, replacement)

test = Path("tests/i960/test_snapshot.c")
replace_once(
    test,
    "    memset(machine.system_control, 0x9a, machine.system_control_size);\n",
    "    memset(machine.system_control, 0x9a, machine.system_control_size);\n"
    "    machine.geometry_write_start = UINT32_C(0x1234);\n"
    "    machine.geometry_read_start = UINT32_C(0x5678);\n"
    "    machine.geometry_control = UINT32_C(0x80000005);\n"
    "    machine.geometry_program_count = UINT32_C(42);\n",
)
replace_once(
    test,
    "        second.texture_ram1[0x600u] != UINT8_C(0x82)) {\n",
    "        second.texture_ram1[0x600u] != UINT8_C(0x82) ||\n"
    "        second.geometry_write_start != UINT32_C(0x1234) ||\n"
    "        second.geometry_read_start != UINT32_C(0x5678) ||\n"
    "        second.geometry_control != UINT32_C(0x80000005) ||\n"
    "        second.geometry_program_count != UINT32_C(42)) {\n",
)
replace_once(
    test,
    "        if (status != VF2_OK || !diff.equal) {\n"
    "            vf2_model2a_shutdown(&live_machine);\n",
    "        if (status != VF2_OK || !diff.equal) {\n"
    "            vf2_model2a_shutdown(&live_machine);\n",
)
# Insert model2-state negative check immediately after the successful live-state check.
marker = """            return 5;
        }
        live_machine.work_ram[7] ^= 1u;
"""
insert = """            return 5;
        }
        live_machine.geometry_write_start ^= UINT32_C(4);
        status = vf2_i960_compare_live_state(
            &cpu, &machine, &live_cpu, &live_machine, &diff
        );
        if (status != VF2_OK || diff.equal ||
            strcmp(diff.component, "model2-state") != 0 ||
            diff.first_offset != 0u) {
            vf2_model2a_shutdown(&live_machine);
            (void)remove(path);
            vf2_i960_snapshot_destroy(&first);
            vf2_i960_snapshot_destroy(&second);
            vf2_model2a_shutdown(&machine);
            return 6;
        }
        live_machine.geometry_write_start ^= UINT32_C(4);
        live_machine.work_ram[7] ^= 1u;
"""
replace_once(test, marker, insert)
# Keep distinct later failure codes cosmetic but monotonic.
text = test.read_text()
text = text.replace("            return 6;\n        }\n        status = vf2_i960_snapshot_capture", "            return 7;\n        }\n        status = vf2_i960_snapshot_capture", 1)
text = text.replace("            return 7;\n        }\n        vf2_model2a_shutdown(&live_machine);", "            return 8;\n        }\n        vf2_model2a_shutdown(&live_machine);", 1)
test.write_text(text)
