from pathlib import Path
p=Path('src/i960/executor.c')
s=p.read_text()
old='''    if (strcmp(mnemonic, "cvtir") == 0) {
        status = operand_value(cpu, &instruction->operands[0], &first);
        if (status != VF2_OK) {
            return status;
        }
        return set_register(
            cpu, &instruction->operands[1],
            float_to_bits((float)(int32_t)first)
        );
    }
'''
new='''    if (strcmp(mnemonic, "cvtir") == 0) {
        status = operand_value(cpu, &instruction->operands[0], &first);
        if (status != VF2_OK) {
            return status;
        }
        return set_register(
            cpu, &instruction->operands[1],
            float_to_bits((float)(int32_t)first)
        );
    }
    if (strcmp(mnemonic, "cvtilr") == 0) {
        uint8_t source = 0u;
        uint64_t raw = 0u;
        int64_t value = 0;
        if (instruction->operands[0].kind != VF2_I960_OPERAND_REGISTER ||
            instruction->operands[0].value.reg + 1u >= VF2_I960_REGISTER_COUNT) {
            return VF2_ERROR_UNSUPPORTED;
        }
        source = instruction->operands[0].value.reg;
        raw = (uint64_t)cpu->registers[source] |
            ((uint64_t)cpu->registers[source + 1u] << 32u);
        memcpy(&value, &raw, sizeof(value));
        return set_register(
            cpu, &instruction->operands[1],
            float_to_bits((float)value)
        );
    }
'''
if s.count(old)!=1: raise SystemExit(f'cvtir anchor count={s.count(old)}')
p.write_text(s.replace(old,new,1))
