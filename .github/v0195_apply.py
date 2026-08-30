from pathlib import Path

HELPERS = r'''static vf2_status execute_texture_record_publish(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t value,
    uint32_t flags,
    uint32_t record,
    uint32_t priority,
    uint64_t *instructions,
    uint64_t *changed_values,
    size_t *bytes_written
)
{
    uint16_t old_value = 0u;
    uint16_t current_priority = 0u;
    uint16_t old_metric = 0u;
    vf2_status status = VF2_OK;

    /* The original helper compares the fixed Model 2A board signature
     * before touching the record. This recovered backend is already bound
     * to that board, so preserve the three architectural instructions
     * without a ROM/device read. */
    *instructions += UINT64_C(3);
    *instructions += UINT64_C(2);
    if (value > UINT32_C(0x56)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    *instructions += UINT64_C(1);
    if ((flags & UINT32_C(0x10)) != 0u) {
        status = read_u16(machine, record + UINT32_C(8), &old_metric);
        if (status == VF2_OK) {
            status = write_u16(
                machine, record + UINT32_C(8),
                (uint16_t)cpu->registers[VF2_I960_G0_REGISTER + 4u]
            );
        }
        *instructions += UINT64_C(3);
        if (status != VF2_OK) {
            return status;
        }
        ++*changed_values;
        *bytes_written += 2u;
        if (old_metric !=
            (uint16_t)cpu->registers[VF2_I960_G0_REGISTER + 4u]) {
            goto publish;
        }
    }

    status = read_u16(machine, record, &old_value);
    *instructions += UINT64_C(2);
    if (status != VF2_OK) {
        return status;
    }
    if (old_value == (uint16_t)value) {
        *instructions += UINT64_C(1);
        return VF2_OK;
    }

publish:
    status = vf2_model2a_write_u32(
        machine, UINT32_C(0x00550000), UINT32_C(1)
    );
    if (status == VF2_OK) {
        status = write_u16(machine, record, (uint16_t)value);
    }
    if (status == VF2_OK) {
        status = write_u16(machine, record + UINT32_C(2), UINT16_MAX);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(
            machine, record + UINT32_C(0x10), flags
        );
    }
    if (status == VF2_OK) {
        status = write_u16(
            machine, record + UINT32_C(0x1c), (uint16_t)priority
        );
    }
    *instructions += UINT64_C(8);
    if (status != VF2_OK) {
        return status;
    }
    *changed_values += UINT64_C(5);
    *bytes_written += 14u;

    status = read_u16(machine, UINT32_C(0x0055c2f0), &current_priority);
    *instructions += UINT64_C(2);
    if (status != VF2_OK) {
        return status;
    }
    if (priority <= (uint32_t)current_priority) {
        *instructions += UINT64_C(1);
        return VF2_OK;
    }

    cpu->registers[VF2_I960_G0_REGISTER] = 0u;
    status = vf2_model2a_write_u32(machine, UINT32_C(0x0055000c), 0u);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x00550080), 0u);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, UINT32_C(0x005500f4), 0u);
    }
    *instructions += UINT64_C(5);
    if (status != VF2_OK) {
        return status;
    }
    *changed_values += UINT64_C(3);
    *bytes_written += 12u;
    return VF2_OK;
}

static vf2_status execute_texture_counter_expiry(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t argument0,
    uint32_t argument1,
    uint32_t argument2,
    uint32_t return_address,
    uint64_t *instructions,
    uint64_t *changed_values,
    size_t *bytes_written
)
{
    uint32_t queue = 0u;
    uint32_t first_record = 0u;
    uint32_t second_record = 0u;
    uint64_t helper_instructions = 0u;
    vf2_status status = VF2_OK;

    if (argument0 > UINT32_C(0x56)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    status = vf2_i960_cpu_enter_procedure(
        cpu, UINT32_C(0x0004b934), return_address
    );
    if (status != VF2_OK) {
        return status;
    }

    helper_instructions += UINT64_C(4);
    if (argument1 == 0u) {
        queue = UINT32_C(0x005502a8);
        first_record = UINT32_C(0x00550188);
        second_record = UINT32_C(0x00550248);
        helper_instructions += UINT64_C(4);
    } else {
        queue = UINT32_C(0x005502b0);
        first_record = UINT32_C(0x005501a8);
        second_record = UINT32_C(0x00550268);
        helper_instructions += UINT64_C(3);
    }

    status = write_u16(machine, queue, UINT16_C(1));
    if (status == VF2_OK) {
        status = write_u16(machine, queue + UINT32_C(2), (uint16_t)argument1);
    }
    if (status == VF2_OK) {
        status = write_u16(machine, queue + UINT32_C(4), (uint16_t)argument2);
    }
    helper_instructions += UINT64_C(8);
    if (status != VF2_OK) {
        return status;
    }
    *changed_values += UINT64_C(3);
    *bytes_written += 6u;

    cpu->registers[VF2_I960_G0_REGISTER] = argument0;
    cpu->registers[VF2_I960_G0_REGISTER + 1u] = argument1;
    cpu->registers[VF2_I960_G0_REGISTER + 2u] = first_record;
    cpu->registers[VF2_I960_G0_REGISTER + 3u] = UINT32_C(3);
    helper_instructions += UINT64_C(5);
    status = execute_texture_record_publish(
        machine, cpu, argument0, argument1, first_record, UINT32_C(3),
        &helper_instructions, changed_values, bytes_written
    );
    if (status != VF2_OK) {
        return status;
    }

    cpu->registers[VF2_I960_G0_REGISTER] = argument0 + UINT32_C(1);
    cpu->registers[VF2_I960_G0_REGISTER + 1u] = argument1;
    cpu->registers[VF2_I960_G0_REGISTER + 2u] = second_record;
    cpu->registers[VF2_I960_G0_REGISTER + 3u] = UINT32_C(1);
    helper_instructions += UINT64_C(5);
    status = execute_texture_record_publish(
        machine, cpu, argument0 + UINT32_C(1), argument1,
        second_record, UINT32_C(1),
        &helper_instructions, changed_values, bytes_written
    );
    if (status != VF2_OK) {
        return status;
    }

    helper_instructions += UINT64_C(1);
    account_nested_procedure(cpu, UINT64_C(3), UINT64_C(3));
    status = vf2_i960_cpu_return_procedure(cpu, machine);
    if (status != VF2_OK || cpu->ip != return_address) {
        return status == VF2_OK ? VF2_ERROR_UNSUPPORTED : status;
    }
    *instructions += helper_instructions;
    return VF2_OK;
}

'''

def expiry_block(counter, return_address):
    return f'''    if ({counter} == UINT32_C(1)) {{
        uint32_t argument0 = 0u;
        uint32_t argument1 = 0u;
        uint32_t argument2 = 0u;
        cpu->registers[4] = 0u;
        set_equal_condition(cpu);
        status = vf2_model2a_write_u32(machine, VF2_TEXTURE_COUNTER{counter[-1]}, 0u);
        if (status == VF2_OK) {{
            status = vf2_model2a_read_u32(
                machine, VF2_TEXTURE_COUNTER{counter[-1]} + UINT32_C(4), &argument0
            );
        }}
        if (status == VF2_OK) {{
            status = vf2_model2a_read_u32(
                machine, VF2_TEXTURE_COUNTER{counter[-1]} + UINT32_C(8), &argument1
            );
        }}
        if (status == VF2_OK) {{
            status = vf2_model2a_read_u32(
                machine, VF2_TEXTURE_COUNTER{counter[-1]} + UINT32_C(12), &argument2
            );
        }}
        if (status != VF2_OK) {{
            return status;
        }}
        instructions += UINT64_C(6);
        ++changed_values;
        bytes_written += 4u;
        status = execute_texture_counter_expiry(
            machine, cpu, argument0, argument1, argument2,
            UINT32_C({return_address}),
            &instructions, &changed_values, &bytes_written
        );
        if (status != VF2_OK) {{
            return status;
        }}
        procedure_calls += UINT64_C(4);
        procedure_returns += UINT64_C(4);
    }}
'''

p = Path('src/recovered/texture_bridge_texture.c')
s = p.read_text()
fn = s.index('vf2_status execute_texture_counter_update(')
s = s[:fn] + HELPERS + s[fn:]
fn = s.index('vf2_status execute_texture_counter_update(')
for counter, ret in [('counter0', '0x0004bbd8'), ('counter1', '0x0004bc18')]:
    a = s.index(f'    if ({counter} == UINT32_C(1)) {{', fn)
    b = s.index(f'    if ({counter} > UINT32_C(1)) {{', a)
    s = s[:a] + expiry_block(counter, ret) + s[b:]

decl = '    size_t bytes_written = 0u;\n    vf2_status status = VF2_OK;\n'
if decl not in s[fn:]:
    raise SystemExit('counter declarations missing')
s = s[:fn] + s[fn:].replace(
    decl,
    '    size_t bytes_written = 0u;\n'
    '    uint64_t procedure_calls = 0u;\n'
    '    uint64_t procedure_returns = 0u;\n'
    '    vf2_status status = VF2_OK;\n',
    1
)
zero = '        report->recovered_instruction_count = instructions;\n        report->cpu_poststate_applied = 1;\n'
pos = s.index(zero, fn)
s = s[:pos] + s[pos:].replace(
    zero,
    '        report->recovered_instruction_count = instructions;\n'
    '        report->recovered_procedure_calls = procedure_calls;\n'
    '        report->recovered_procedure_returns = procedure_returns;\n'
    '        report->cpu_poststate_applied = 1;\n',
    1
)
fixed = '        report->recovered_procedure_calls = UINT64_C(3);\n        report->recovered_procedure_returns = UINT64_C(3);\n'
if fixed not in s[fn:]:
    raise SystemExit('counter2 report anchor missing')
s = s[:fn] + s[fn:].replace(
    fixed,
    '        report->recovered_procedure_calls = UINT64_C(3) + procedure_calls;\n'
    '        report->recovered_procedure_returns = UINT64_C(3) + procedure_returns;\n',
    1
)
final = '    report->recovered_instruction_count = instructions;\n    report->cpu_poststate_applied = 1;\n    return VF2_OK;\n'
pos = s.index(final, fn)
s = s[:pos] + s[pos:].replace(
    final,
    '    report->recovered_instruction_count = instructions;\n'
    '    report->recovered_procedure_calls = procedure_calls;\n'
    '    report->recovered_procedure_returns = procedure_returns;\n'
    '    report->cpu_poststate_applied = 1;\n'
    '    return VF2_OK;\n',
    1
)
p.write_text(s)

p = Path('src/recovered/native_runtime.c')
s = p.read_text()
a = s.index('static vf2_status\nexecute_texture_counter_interpreter(')
b = s.index('\ntypedef struct vf2_native_wrapper_boundary_context', a)
s = s[:a] + s[b + 1:]
a = s.index('    } else if (cpu->ip == VF2_TEXTURE_COUNTER_UPDATE_ENTRY) {')
b = s.index('            vf2_hybrid_bridge_report bridge_report;', a)
s = s[:a] + '    } else if (cpu->ip == VF2_TEXTURE_COUNTER_UPDATE_ENTRY) {\n        if (status == VF2_OK) {\n' + s[b:]
p.write_text(s)
