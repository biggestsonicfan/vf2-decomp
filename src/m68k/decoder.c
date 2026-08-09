#include "vf2/m68k.h"

#include <stdio.h>
#include <string.h>

static int in_range(size_t size, uint32_t address, size_t access_size)
{
    return (size_t)address <= size && access_size <= size - (size_t)address;
}

vf2_status vf2_m68k_read_u16(
    const uint8_t *program,
    size_t program_size,
    uint32_t address,
    uint16_t *value
)
{
    if (program == NULL || value == NULL || !in_range(program_size, address, 2u)) {
        return VF2_ERROR_OUT_OF_BOUNDS;
    }
    *value = (uint16_t)(((uint16_t)program[address] << 8u) |
                        program[address + 1u]);
    return VF2_OK;
}

vf2_status vf2_m68k_read_u32(
    const uint8_t *program,
    size_t program_size,
    uint32_t address,
    uint32_t *value
)
{
    uint16_t high = 0u;
    uint16_t low = 0u;
    vf2_status status = vf2_m68k_read_u16(program, program_size, address, &high);
    if (status == VF2_OK) {
        status = vf2_m68k_read_u16(program, program_size, address + 2u, &low);
    }
    if (status == VF2_OK) {
        *value = ((uint32_t)high << 16u) | low;
    }
    return status;
}

static int32_t sign_extend8(uint8_t value)
{
    return (int32_t)(int8_t)value;
}

static int32_t sign_extend16(uint16_t value)
{
    return (int32_t)(int16_t)value;
}

static void set_text(vf2_m68k_instruction *instruction, const char *text)
{
    (void)snprintf(instruction->text, sizeof(instruction->text), "%s", text);
}

static void set_branch_text(
    vf2_m68k_instruction *instruction,
    const char *mnemonic,
    uint32_t target
)
{
    (void)snprintf(
        instruction->text, sizeof(instruction->text),
        "%s $%08x", mnemonic, (unsigned)target
    );
}

static vf2_status format_bit_ea(
    const uint8_t *program,
    size_t program_size,
    uint32_t extension_address,
    unsigned mode,
    unsigned register_number,
    char *text,
    size_t text_size,
    size_t *extension_bytes
)
{
    uint16_t extension = 0u;

    *extension_bytes = 0u;
    if (mode == 0u) {
        (void)snprintf(text, text_size, "d%u", register_number);
        return VF2_OK;
    }
    if (mode == 2u) {
        (void)snprintf(text, text_size, "(a%u)", register_number);
        return VF2_OK;
    }
    if (mode == 3u) {
        (void)snprintf(text, text_size, "(a%u)+", register_number);
        return VF2_OK;
    }
    if (mode == 4u) {
        (void)snprintf(text, text_size, "-(a%u)", register_number);
        return VF2_OK;
    }
    if (mode == 5u) {
        if (vf2_m68k_read_u16(program, program_size, extension_address,
                              &extension) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        *extension_bytes = 2u;
        (void)snprintf(text, text_size, "$%04x(a%u)",
                       (unsigned)extension, register_number);
        return VF2_OK;
    }
    if (mode == 6u) {
        unsigned index_register;
        char index_size;
        if (vf2_m68k_read_u16(program, program_size, extension_address,
                              &extension) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        *extension_bytes = 2u;
        index_register = (unsigned)((extension >> 12u) & 7u);
        index_size = (extension & UINT16_C(0x0800)) != 0u ? 'l' : 'w';
        (void)snprintf(text, text_size, "$%02x(a%u,d%u.%c)",
                       (unsigned)(extension & 0xffu), register_number,
                       index_register, index_size);
        return VF2_OK;
    }
    if (mode == 7u && register_number == 0u) {
        if (vf2_m68k_read_u16(program, program_size, extension_address,
                              &extension) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        *extension_bytes = 2u;
        (void)snprintf(text, text_size, "$%04x", (unsigned)extension);
        return VF2_OK;
    }
    if (mode == 7u && register_number == 1u) {
        uint32_t address;
        if (vf2_m68k_read_u32(program, program_size, extension_address,
                              &address) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        *extension_bytes = 4u;
        (void)snprintf(text, text_size, "$%08x", (unsigned)address);
        return VF2_OK;
    }
    if (mode == 7u && register_number == 2u) {
        if (vf2_m68k_read_u16(program, program_size, extension_address,
                              &extension) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        *extension_bytes = 2u;
        (void)snprintf(text, text_size, "$%08x(pc)",
                       (unsigned)(extension_address + 2u +
                                  (uint32_t)sign_extend16(extension)));
        return VF2_OK;
    }
    if (mode == 7u && register_number == 3u) {
        if (vf2_m68k_read_u16(program, program_size, extension_address,
                              &extension) != VF2_OK) {
            return VF2_ERROR_OUT_OF_BOUNDS;
        }
        *extension_bytes = 2u;
        (void)snprintf(text, text_size, "$%02x(pc,d%u.%c)",
                       (unsigned)(extension & 0xffu),
                       (unsigned)((extension >> 12u) & 7u),
                       (extension & UINT16_C(0x0800)) != 0u ? 'l' : 'w');
        return VF2_OK;
    }
    return VF2_ERROR_UNSUPPORTED;
}

vf2_status vf2_m68k_decode(
    const uint8_t *program,
    size_t program_size,
    uint32_t address,
    vf2_m68k_instruction *instruction
)
{
    static const char *const conditions[16] = {
        "bra", "bsr", "bhi", "bls", "bcc", "bcs", "bne", "beq",
        "bvc", "bvs", "bpl", "bmi", "bge", "blt", "bgt", "ble"
    };
    static const char *const db_conditions[16] = {
        "dbt", "dbf", "dbhi", "dbls", "dbcc", "dbcs", "dbne", "dbeq",
        "dbvc", "dbvs", "dbpl", "dbmi", "dbge", "dblt", "dbgt", "dble"
    };
    uint16_t opcode = 0u;
    vf2_status status = VF2_OK;

    if (instruction == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(instruction, 0, sizeof(*instruction));
    instruction->address = address;
    status = vf2_m68k_read_u16(program, program_size, address, &opcode);
    if (status != VF2_OK) {
        return status;
    }
    instruction->opcode = opcode;
    instruction->length = 2u;
    instruction->supported = 1;

    if (opcode == UINT16_C(0x4e71)) {
        set_text(instruction, "nop");
    } else if (opcode == UINT16_C(0x46fc)) {
        uint16_t value = 0u;
        status = vf2_m68k_read_u16(program, program_size, address + 2u, &value);
        instruction->length = 4u;
        if (status == VF2_OK) {
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "move.w #$%04x,sr", (unsigned)value);
        }
    } else if (opcode == UINT16_C(0x4e75)) {
        set_text(instruction, "rts");
    } else if (opcode == UINT16_C(0x4e73)) {
        set_text(instruction, "rte");
    } else if (opcode == UINT16_C(0x4e5e)) {
        set_text(instruction, "unlk a6");
    } else if (opcode == UINT16_C(0x4e56)) {
        uint16_t displacement = 0u;
        status = vf2_m68k_read_u16(program, program_size, address + 2u,
                                   &displacement);
        instruction->length = 4u;
        if (status == VF2_OK) {
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "link a6,#$%04x", (unsigned)displacement);
        }
    } else if ((opcode & UINT16_C(0xf000)) == UINT16_C(0x7000)) {
        (void)snprintf(instruction->text, sizeof(instruction->text),
                       "moveq #%d,d%u", (int)(int8_t)(opcode & 0xffu),
                       (unsigned)((opcode >> 9u) & 7u));
    } else if ((opcode & UINT16_C(0xf000)) == UINT16_C(0x6000)) {
        int32_t displacement = sign_extend8((uint8_t)opcode);
        instruction->length = 2u;
        if ((opcode & 0xffu) == 0u) {
            uint16_t extension = 0u;
            status = vf2_m68k_read_u16(program, program_size, address + 2u,
                                       &extension);
            displacement = sign_extend16(extension);
            instruction->length = 4u;
        }
        if (status == VF2_OK) {
            set_branch_text(instruction, conditions[(opcode >> 8u) & 15u],
                            address + (uint32_t)instruction->length +
                            (uint32_t)displacement);
        }
    } else if ((opcode & UINT16_C(0xf0f8)) == UINT16_C(0x50c8)) {
        uint16_t extension = 0u;
        status = vf2_m68k_read_u16(program, program_size, address + 2u,
                                   &extension);
        instruction->length = 4u;
        if (status == VF2_OK) {
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "%s d%u,$%08x", db_conditions[(opcode >> 8u) & 15u],
                           (unsigned)(opcode & 7u),
                           (unsigned)(address + 4u +
                                      (uint32_t)sign_extend16(extension)));
        }
    } else if ((opcode & UINT16_C(0xff00)) == UINT16_C(0x0800) ||
               (opcode & UINT16_C(0xf100)) == UINT16_C(0x0100)) {
        static const char *const bit_operations[4] = {
            "btst", "bchg", "bclr", "bset"
        };
        const int immediate_bit = (opcode & UINT16_C(0xf100)) !=
                                  UINT16_C(0x0100);
        const unsigned operation = (unsigned)((opcode >> 6u) & 3u);
        const unsigned mode = (unsigned)((opcode >> 3u) & 7u);
        const unsigned register_number = (unsigned)(opcode & 7u);
        uint16_t bit_number = 0u;
        size_t extension_bytes = 0u;
        size_t ea_address = address + 2u;
        char ea_text[64];

        if (immediate_bit) {
            status = vf2_m68k_read_u16(program, program_size, address + 2u,
                                       &bit_number);
            ea_address = address + 4u;
        } else {
            bit_number = (uint16_t)((opcode >> 9u) & 7u);
        }
        if (status == VF2_OK) {
            status = format_bit_ea(program, program_size, (uint32_t)ea_address,
                                   mode, register_number, ea_text,
                                   sizeof(ea_text), &extension_bytes);
        }
        instruction->length = (immediate_bit ? 4u : 2u) + extension_bytes;
        if (status == VF2_OK) {
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "%s %s%u,%s", bit_operations[operation],
                           immediate_bit ? "#" : "d",
                           (unsigned)(immediate_bit ? bit_number & 7u :
                                      bit_number), ea_text);
        } else if (status == VF2_ERROR_UNSUPPORTED) {
            instruction->supported = 0;
            status = VF2_OK;
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           ".word $%04x", (unsigned)opcode);
        }
    } else if ((opcode & UINT16_C(0xffc0)) == UINT16_C(0x0200) ||
               (opcode & UINT16_C(0xffc0)) == UINT16_C(0x0240) ||
               (opcode & UINT16_C(0xffc0)) == UINT16_C(0x0280)) {
        const unsigned mode = (unsigned)((opcode >> 3u) & 7u);
        const unsigned register_number = (unsigned)(opcode & 7u);
        const unsigned size_code = (unsigned)((opcode >> 6u) & 3u);
        const char size_name = size_code == 0u ? 'b' :
                               (size_code == 1u ? 'w' : 'l');
        const size_t immediate_bytes = size_code == 2u ? 4u : 2u;
        uint32_t immediate = 0u;
        uint16_t displacement = 0u;
        instruction->length = 2u + immediate_bytes +
                              ((mode == 5u || mode == 6u) ? 2u : 0u);
        if (immediate_bytes == 4u) {
            status = vf2_m68k_read_u32(program, program_size, address + 2u,
                                       &immediate);
        } else {
            uint16_t value = 0u;
            status = vf2_m68k_read_u16(program, program_size, address + 2u,
                                       &value);
            immediate = value;
        }
        if (status == VF2_OK && mode == 0u) {
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "andi.%c #$%x,d%u", size_name,
                           (unsigned)immediate, register_number);
        } else if (status == VF2_OK && mode == 5u) {
            status = vf2_m68k_read_u16(
                program, program_size,
                address + 2u + (uint32_t)immediate_bytes, &displacement
            );
            if (status == VF2_OK) {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "andi.%c #$%x,$%04x(a%u)", size_name,
                               (unsigned)immediate, (unsigned)displacement,
                               register_number);
            }
        } else if (status == VF2_OK && mode == 2u) {
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "andi.%c #$%x,(a%u)", size_name,
                           (unsigned)immediate, register_number);
        }
    } else if ((opcode & UINT16_C(0xffc0)) == UINT16_C(0x0400) ||
               (opcode & UINT16_C(0xffc0)) == UINT16_C(0x0440) ||
               (opcode & UINT16_C(0xffc0)) == UINT16_C(0x0480)) {
        const unsigned mode = (unsigned)((opcode >> 3u) & 7u);
        const unsigned register_number = (unsigned)(opcode & 7u);
        const unsigned size_code = (unsigned)((opcode >> 6u) & 3u);
        const char size_name = size_code == 0u ? 'b' :
                               (size_code == 1u ? 'w' : 'l');
        const size_t immediate_bytes = size_code == 2u ? 4u : 2u;
        uint32_t immediate = 0u;
        uint16_t displacement = 0u;
        instruction->length = 2u + immediate_bytes + (mode == 5u ? 2u : 0u);
        if (immediate_bytes == 4u) {
            status = vf2_m68k_read_u32(program, program_size, address + 2u,
                                       &immediate);
        } else {
            uint16_t value = 0u;
            status = vf2_m68k_read_u16(program, program_size, address + 2u,
                                       &value);
            immediate = value;
        }
        if (status == VF2_OK && mode == 0u) {
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "subi.%c #$%x,d%u", size_name,
                           (unsigned)immediate, register_number);
        } else if (status == VF2_OK && mode == 5u) {
            status = vf2_m68k_read_u16(
                program, program_size,
                address + 2u + (uint32_t)immediate_bytes, &displacement
            );
            if (status == VF2_OK) {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "subi.%c #$%x,$%04x(a%u)", size_name,
                               (unsigned)immediate, (unsigned)displacement,
                               register_number);
            }
        } else if (status == VF2_OK && mode == 2u) {
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "subi.%c #$%x,(a%u)", size_name,
                           (unsigned)immediate, register_number);
        }
    } else if ((opcode & UINT16_C(0xffc0)) == UINT16_C(0x0600) ||
               (opcode & UINT16_C(0xffc0)) == UINT16_C(0x0640) ||
               (opcode & UINT16_C(0xffc0)) == UINT16_C(0x0680)) {
        const unsigned mode = (unsigned)((opcode >> 3u) & 7u);
        const unsigned register_number = (unsigned)(opcode & 7u);
        const unsigned size_code = (unsigned)((opcode >> 6u) & 3u);
        const char size_name = size_code == 0u ? 'b' :
                               (size_code == 1u ? 'w' : 'l');
        const size_t immediate_bytes = size_code == 2u ? 4u : 2u;
        uint32_t immediate = 0u;
        uint16_t displacement = 0u;
        instruction->length = 2u + immediate_bytes +
                              ((mode == 5u || mode == 6u) ? 2u : 0u);
        if (immediate_bytes == 4u) {
            status = vf2_m68k_read_u32(program, program_size, address + 2u,
                                       &immediate);
        } else {
            uint16_t value = 0u;
            status = vf2_m68k_read_u16(program, program_size, address + 2u,
                                       &value);
            immediate = value;
        }
        if (status == VF2_OK && mode == 0u) {
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "addi.%c #$%x,d%u", size_name,
                           (unsigned)immediate, register_number);
        } else if (status == VF2_OK && mode == 5u) {
            status = vf2_m68k_read_u16(
                program, program_size,
                address + 2u + (uint32_t)immediate_bytes, &displacement
            );
            if (status == VF2_OK) {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "addi.%c #$%x,$%04x(a%u)", size_name,
                               (unsigned)immediate, (unsigned)displacement,
                               register_number);
            }
        } else if (status == VF2_OK && mode == 2u) {
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "addi.%c #$%x,(a%u)", size_name,
                           (unsigned)immediate, register_number);
        } else if (status == VF2_OK && mode == 6u) {
            uint16_t extension = 0u;
            status = vf2_m68k_read_u16(
                program, program_size,
                address + 2u + (uint32_t)immediate_bytes, &extension
            );
            if (status == VF2_OK) {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "addi.%c #$%x,$%02x(a%u,d%u.%c)",
                               size_name, (unsigned)immediate,
                               (unsigned)(extension & 0xffu), register_number,
                               (unsigned)((extension >> 12u) & 7u),
                               (extension & UINT16_C(0x0800)) != 0u ? 'l' : 'w');
            }
        }
    } else if ((opcode & UINT16_C(0xf100)) == UINT16_C(0x5100) &&
               (((opcode >> 3u) & 7u) == 0u ||
                ((opcode >> 3u) & 7u) == 1u ||
                ((opcode >> 3u) & 7u) == 2u ||
                ((opcode >> 3u) & 7u) == 3u ||
                ((opcode >> 3u) & 7u) == 5u ||
                ((opcode >> 3u) & 7u) == 6u)) {
        const unsigned mode = (unsigned)((opcode >> 3u) & 7u);
        const unsigned register_number = (unsigned)(opcode & 7u);
        const unsigned count = (unsigned)((opcode >> 9u) & 7u);
        const unsigned size_code = (unsigned)((opcode >> 6u) & 3u);
        const char size_name = size_code == 0u ? 'b' :
                               (size_code == 1u ? 'w' : 'l');
        uint16_t displacement = 0u;
        instruction->length = (mode == 5u || mode == 6u) ? 4u : 2u;
        if (mode == 5u) {
            status = vf2_m68k_read_u16(program, program_size, address + 2u,
                                       &displacement);
        }
        if (status == VF2_OK) {
            if (mode == 0u) {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "subq.%c #%u,d%u", size_name,
                               count == 0u ? 8u : count, register_number);
            } else if (mode == 1u) {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "subq.%c #%u,a%u", size_name,
                               count == 0u ? 8u : count, register_number);
            } else if (mode == 2u) {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "subq.%c #%u,(a%u)", size_name,
                               count == 0u ? 8u : count, register_number);
            } else if (mode == 3u) {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "subq.%c #%u,(a%u)+", size_name,
                               count == 0u ? 8u : count, register_number);
            } else if (mode == 6u) {
                uint16_t extension = 0u;
                status = vf2_m68k_read_u16(program, program_size,
                                           address + 2u, &extension);
                if (status == VF2_OK) {
                    (void)snprintf(instruction->text, sizeof(instruction->text),
                                   "subq.%c #%u,$%02x(a%u,d%u.%c)",
                                   size_name, count == 0u ? 8u : count,
                                   (unsigned)(extension & 0xffu),
                                   register_number,
                                   (unsigned)((extension >> 12u) & 7u),
                                   (extension & UINT16_C(0x0800)) != 0u ? 'l' : 'w');
                }
            } else {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "subq.%c #%u,$%04x(a%u)", size_name,
                               count == 0u ? 8u : count,
                               (unsigned)displacement, register_number);
            }
        }
    } else if (((opcode & UINT16_C(0xf1c0)) == UINT16_C(0x5000) ||
                (opcode & UINT16_C(0xf1c0)) == UINT16_C(0x5040) ||
                (opcode & UINT16_C(0xf1c0)) == UINT16_C(0x5080)) &&
               (((opcode >> 3u) & 7u) == 0u ||
                ((opcode >> 3u) & 7u) == 1u ||
                ((opcode >> 3u) & 7u) == 2u ||
                ((opcode >> 3u) & 7u) == 3u ||
                ((opcode >> 3u) & 7u) == 5u ||
                ((opcode >> 3u) & 7u) == 6u)) {
        const unsigned mode = (unsigned)((opcode >> 3u) & 7u);
        const unsigned register_number = (unsigned)(opcode & 7u);
        const unsigned count = (unsigned)((opcode >> 9u) & 7u);
        const unsigned size_code = (unsigned)((opcode >> 6u) & 3u);
        const char size_name = size_code == 0u ? 'b' :
                               (size_code == 1u ? 'w' : 'l');
        uint16_t displacement = 0u;
        instruction->length = (mode == 5u || mode == 6u) ? 4u : 2u;
        if (mode == 5u) {
            status = vf2_m68k_read_u16(program, program_size, address + 2u,
                                       &displacement);
        }
        if (status == VF2_OK) {
            if (mode == 0u) {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "addq.%c #%u,d%u", size_name,
                               count == 0u ? 8u : count, register_number);
            } else if (mode == 1u) {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "addq.%c #%u,a%u", size_name,
                               count == 0u ? 8u : count, register_number);
            } else if (mode == 2u) {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "addq.%c #%u,(a%u)", size_name,
                               count == 0u ? 8u : count, register_number);
            } else if (mode == 3u) {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "addq.%c #%u,(a%u)+", size_name,
                               count == 0u ? 8u : count, register_number);
            } else if (mode == 6u) {
                uint16_t extension = 0u;
                status = vf2_m68k_read_u16(program, program_size,
                                           address + 2u, &extension);
                if (status == VF2_OK) {
                    (void)snprintf(instruction->text, sizeof(instruction->text),
                                   "addq.%c #%u,$%02x(a%u,d%u.%c)",
                                   size_name, count == 0u ? 8u : count,
                                   (unsigned)(extension & 0xffu),
                                   register_number,
                                   (unsigned)((extension >> 12u) & 7u),
                                   (extension & UINT16_C(0x0800)) != 0u ? 'l' : 'w');
                }
            } else {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "addq.%c #%u,$%04x(a%u)", size_name,
                               count == 0u ? 8u : count,
                               (unsigned)displacement, register_number);
            }
        }
    } else if ((opcode & UINT16_C(0xffc0)) == UINT16_C(0x4400) ||
               (opcode & UINT16_C(0xffc0)) == UINT16_C(0x4440) ||
               (opcode & UINT16_C(0xffc0)) == UINT16_C(0x4480)) {
        const unsigned mode = (unsigned)((opcode >> 3u) & 7u);
        const unsigned size_code = (unsigned)((opcode >> 6u) & 3u);
        const char size_name = size_code == 0u ? 'b' :
                               (size_code == 1u ? 'w' : 'l');
        if (mode == 0u) {
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "neg.%c d%u", size_name,
                           (unsigned)(opcode & 7u));
        } else {
            instruction->supported = 0;
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           ".word $%04x", (unsigned)opcode);
        }
    } else if ((opcode & UINT16_C(0xffc0)) == UINT16_C(0x4600) ||
               (opcode & UINT16_C(0xffc0)) == UINT16_C(0x4640) ||
               (opcode & UINT16_C(0xffc0)) == UINT16_C(0x4680)) {
        const unsigned mode = (unsigned)((opcode >> 3u) & 7u);
        const unsigned size_code = (unsigned)((opcode >> 6u) & 3u);
        const char size_name = size_code == 0u ? 'b' :
                               (size_code == 1u ? 'w' : 'l');
        if (mode == 0u) {
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "not.%c d%u", size_name,
                           (unsigned)(opcode & 7u));
        } else {
            instruction->supported = 0;
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           ".word $%04x", (unsigned)opcode);
        }
    } else if ((opcode & UINT16_C(0xff00)) == UINT16_C(0x4a00)) {
        const unsigned mode = (unsigned)((opcode >> 3u) & 7u);
        const unsigned register_number = (unsigned)(opcode & 7u);
        const unsigned size_code = (unsigned)((opcode >> 6u) & 3u);
        const char size_name = size_code == 0u ? 'b' :
                               (size_code == 1u ? 'w' : 'l');
        if (mode == 0u) {
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "tst.%c d%u", size_name, register_number);
        } else if (mode == 5u) {
            uint16_t displacement = 0u;
            status = vf2_m68k_read_u16(program, program_size, address + 2u,
                                       &displacement);
            instruction->length = 4u;
            if (status == VF2_OK) {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "tst.%c $%04x(a%u)", size_name,
                               (unsigned)displacement, register_number);
            }
        } else {
            instruction->supported = 0;
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           ".word $%04x", (unsigned)opcode);
        }
    } else if ((opcode & UINT16_C(0xf000)) == UINT16_C(0x8000)) {
        const unsigned operation_mode = (unsigned)((opcode >> 6u) & 7u);
        const unsigned source_mode = (unsigned)((opcode >> 3u) & 7u);
        const unsigned source_register = (unsigned)(opcode & 7u);
        const unsigned destination_register =
            (unsigned)((opcode >> 9u) & 7u);
        const char size_name = operation_mode == 0u ? 'b' :
                               (operation_mode == 1u ? 'w' :
                                (operation_mode == 2u ? 'l' : 'b'));
        char source_text[64];
        size_t extension_bytes = 0u;
        if (operation_mode <= 2u && source_mode == 0u) {
            (void)snprintf(source_text, sizeof(source_text), "d%u",
                           source_register);
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "or.%c %s,d%u", size_name, source_text,
                           destination_register);
        } else if (operation_mode <= 2u &&
                   (source_mode == 2u || source_mode == 3u ||
                    source_mode == 5u || source_mode == 6u)) {
            status = format_bit_ea(program, program_size, address + 2u,
                                   source_mode, source_register, source_text,
                                   sizeof(source_text), &extension_bytes);
            instruction->length = 2u + extension_bytes;
            if (status == VF2_OK) {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "or.%c %s,d%u", size_name, source_text,
                               destination_register);
            }
        } else {
            instruction->supported = 0;
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           ".word $%04x", (unsigned)opcode);
        }
    } else if ((opcode & UINT16_C(0xf000)) == UINT16_C(0xe000) &&
               ((opcode >> 6u) & 3u) != 3u) {
        static const char *const shift_types[4] = { "as", "ls", "rox", "ro" };
        const unsigned type = (unsigned)((opcode >> 3u) & 3u);
        const int left = (opcode & UINT16_C(0x0100)) != 0u;
        const unsigned size_code = (unsigned)((opcode >> 6u) & 3u);
        const char size_name = size_code == 0u ? 'b' :
                               (size_code == 1u ? 'w' : 'l');
        const int register_count = (opcode & UINT16_C(0x0020)) != 0u;
        const unsigned count_or_register = (unsigned)((opcode >> 9u) & 7u);
        const unsigned destination_register = (unsigned)(opcode & 7u);
        if (register_count) {
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "%s%c.%c d%u,d%u", shift_types[type],
                           left ? 'l' : 'r', size_name, count_or_register,
                           destination_register);
        } else {
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "%s%c.%c #%u,d%u", shift_types[type],
                           left ? 'l' : 'r', size_name,
                           count_or_register == 0u ? 8u : count_or_register,
                           destination_register);
        }
    } else if ((opcode & UINT16_C(0xf1ff)) == UINT16_C(0xd0fc) ||
               (opcode & UINT16_C(0xf1ff)) == UINT16_C(0xd1fc) ||
               (opcode & UINT16_C(0xf1ff)) == UINT16_C(0x90fc) ||
               (opcode & UINT16_C(0xf1ff)) == UINT16_C(0x91fc)) {
        const int is_long = (opcode & UINT16_C(0x0100)) != 0u;
        const int is_sub = (opcode & UINT16_C(0xf000)) == UINT16_C(0x9000);
        uint32_t immediate = 0u;
        instruction->length = is_long ? 6u : 4u;
        if (is_long) {
            status = vf2_m68k_read_u32(program, program_size, address + 2u,
                                       &immediate);
        } else {
            uint16_t value = 0u;
            status = vf2_m68k_read_u16(program, program_size, address + 2u,
                                       &value);
            immediate = value;
        }
        if (status == VF2_OK) {
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "%sa.%c #$%x,a%u", is_sub ? "sub" : "add",
                           is_long ? 'l' : 'w',
                           (unsigned)immediate,
                           (unsigned)((opcode >> 9u) & 7u));
        }
    } else if ((opcode & UINT16_C(0xff00)) == UINT16_C(0x0c00)) {
        const unsigned mode = (unsigned)((opcode >> 3u) & 7u);
        const unsigned register_number = (unsigned)(opcode & 7u);
        const unsigned size_code = (unsigned)((opcode >> 6u) & 3u);
        const char size_name = size_code == 0u ? 'b' :
                               (size_code == 1u ? 'w' : 'l');
        const size_t immediate_bytes = size_code == 2u ? 4u : 2u;
        uint32_t immediate = 0u;
        instruction->length = 2u + immediate_bytes + (mode == 5u ? 2u : 0u);
        if (immediate_bytes == 4u) {
            status = vf2_m68k_read_u32(program, program_size, address + 2u,
                                       &immediate);
        } else {
            uint16_t value = 0u;
            status = vf2_m68k_read_u16(program, program_size, address + 2u,
                                       &value);
            immediate = value;
        }
        if (status == VF2_OK && mode == 2u) {
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "cmpi.%c #$%x,(a%u)", size_name,
                           (unsigned)immediate, register_number);
        } else if (status == VF2_OK && mode == 0u) {
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "cmpi.%c #$%x,d%u", size_name,
                           (unsigned)immediate, register_number);
        } else if (status == VF2_OK && mode == 5u) {
            uint16_t displacement = 0u;
            status = vf2_m68k_read_u16(program, program_size,
                                       address + 2u + (uint32_t)immediate_bytes,
                                       &displacement);
            if (status == VF2_OK) {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "cmpi.%c #$%x,$%04x(a%u)", size_name,
                               (unsigned)immediate, (unsigned)displacement,
                               register_number);
            }
        }
    } else if ((opcode & UINT16_C(0xf000)) == UINT16_C(0x9000)) {
        const unsigned destination_register = (unsigned)((opcode >> 9u) & 7u);
        const unsigned operation_mode = (unsigned)((opcode >> 6u) & 7u);
        const unsigned source_mode = (unsigned)((opcode >> 3u) & 7u);
        const unsigned source_register = (unsigned)(opcode & 7u);
        const char size_name = operation_mode == 2u || operation_mode == 6u
            ? 'b' : ((operation_mode == 1u || operation_mode == 5u ||
                      operation_mode == 7u) ? 'l' : 'w');
        uint16_t displacement = 0u;
        const int source_has_displacement = source_mode == 5u;
        instruction->length = 2u + (source_has_displacement ? 2u : 0u);
        if (source_has_displacement) {
            status = vf2_m68k_read_u16(program, program_size, address + 2u,
                                       &displacement);
        }
        if (status == VF2_OK && operation_mode <= 2u &&
            (source_mode == 0u || source_mode == 2u || source_mode == 3u ||
             source_mode == 5u)) {
            if (source_mode == 0u) {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "sub.%c d%u,d%u", size_name, source_register,
                               destination_register);
            } else if (source_mode == 2u) {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "sub.%c (a%u),d%u", size_name, source_register,
                               destination_register);
            } else if (source_mode == 3u) {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "sub.%c (a%u)+,d%u", size_name, source_register,
                               destination_register);
            } else {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "sub.%c $%04x(a%u),d%u", size_name,
                               (unsigned)displacement, source_register,
                               destination_register);
            }
        } else {
            instruction->supported = 0;
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           ".word $%04x", (unsigned)opcode);
        }
    } else if ((opcode & UINT16_C(0xf1ff)) == UINT16_C(0xb0fc) ||
               (opcode & UINT16_C(0xf1ff)) == UINT16_C(0xb1fc)) {
        const int is_long = (opcode & UINT16_C(0x0100)) != 0u;
        const unsigned destination_register =
            (unsigned)((opcode >> 9u) & 7u);
        uint32_t immediate = 0u;
        instruction->length = is_long ? 6u : 4u;
        if (is_long) {
            status = vf2_m68k_read_u32(program, program_size, address + 2u,
                                       &immediate);
        } else {
            uint16_t value = 0u;
            status = vf2_m68k_read_u16(program, program_size, address + 2u,
                                       &value);
            immediate = value;
        }
        if (status == VF2_OK) {
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "cmpa.%c #$%x,a%u", is_long ? 'l' : 'w',
                           (unsigned)immediate, destination_register);
        }
    } else if ((opcode & UINT16_C(0xf000)) == UINT16_C(0xb000) &&
               (((opcode >> 3u) & 7u) == 0u ||
                ((opcode >> 3u) & 7u) == 2u ||
                ((opcode >> 3u) & 7u) == 3u ||
                ((opcode >> 3u) & 7u) == 5u)) {
        const unsigned mode = (unsigned)((opcode >> 3u) & 7u);
        const unsigned source_register = (unsigned)(opcode & 7u);
        const unsigned size_code = (unsigned)((opcode >> 6u) & 3u);
        const char size_name = size_code == 0u ? 'w' :
                               (size_code == 1u ? 'l' : 'b');
        uint16_t displacement = 0u;
        instruction->length = mode == 5u ? 4u : 2u;
        if (mode == 5u) {
            status = vf2_m68k_read_u16(program, program_size, address + 2u,
                                       &displacement);
        }
        if (status == VF2_OK) {
            if (mode == 0u) {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "cmp.%c d%u,d%u", size_name, source_register,
                               (unsigned)((opcode >> 9u) & 7u));
            } else if (mode == 2u) {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "cmp.%c (a%u),d%u", size_name, source_register,
                               (unsigned)((opcode >> 9u) & 7u));
            } else if (mode == 3u) {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "cmp.%c (a%u)+,d%u", size_name, source_register,
                               (unsigned)((opcode >> 9u) & 7u));
            } else {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "cmp.%c $%04x(a%u),d%u", size_name,
                               (unsigned)displacement, source_register,
                               (unsigned)((opcode >> 9u) & 7u));
            }
        }
    } else if ((opcode & UINT16_C(0xf000)) == UINT16_C(0xd000)) {
        const unsigned destination_register = (unsigned)((opcode >> 9u) & 7u);
        const unsigned operation_mode = (unsigned)((opcode >> 6u) & 7u);
        const unsigned source_mode = (unsigned)((opcode >> 3u) & 7u);
        const unsigned source_register = (unsigned)(opcode & 7u);
        const char size_name = operation_mode == 2u || operation_mode == 6u
            ? 'b' : ((operation_mode == 1u || operation_mode == 5u ||
                      operation_mode == 7u) ? 'l' : 'w');
        uint16_t displacement = 0u;
        const int source_has_displacement = source_mode == 5u;
        const int destination_has_displacement =
            source_mode == 0u && (operation_mode == 4u ||
                                  operation_mode == 5u || operation_mode == 6u);
        instruction->length = 2u +
            (source_has_displacement || destination_has_displacement ? 2u : 0u);
        if (source_has_displacement || destination_has_displacement) {
            status = vf2_m68k_read_u16(program, program_size, address + 2u,
                                       &displacement);
        }
        if (status == VF2_OK && operation_mode <= 2u &&
            (source_mode == 0u || source_mode == 2u || source_mode == 3u ||
             source_mode == 5u)) {
            if (source_mode == 0u) {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "add.%c d%u,d%u", size_name, source_register,
                               destination_register);
            } else if (source_mode == 2u) {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "add.%c (a%u),d%u", size_name, source_register,
                               destination_register);
            } else if (source_mode == 3u) {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "add.%c (a%u)+,d%u", size_name, source_register,
                               destination_register);
            } else {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "add.%c $%04x(a%u),d%u", size_name,
                               (unsigned)displacement, source_register,
                               destination_register);
            }
        } else if (status == VF2_OK && (operation_mode == 3u ||
                                        operation_mode == 7u) &&
                   (source_mode == 0u || source_mode == 2u ||
                    source_mode == 3u || source_mode == 5u)) {
            if (source_mode == 0u) {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "adda.%c d%u,a%u",
                               operation_mode == 7u ? 'l' : 'w',
                               source_register, destination_register);
            } else if (source_mode == 5u) {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "adda.%c $%04x(a%u),a%u",
                               operation_mode == 7u ? 'l' : 'w',
                               (unsigned)displacement, source_register,
                               destination_register);
            } else {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "adda.%c (a%u)%s,a%u",
                               operation_mode == 7u ? 'l' : 'w',
                               source_register, source_mode == 3u ? "+" : "",
                               destination_register);
            }
        } else if (status == VF2_OK && operation_mode >= 4u &&
                   operation_mode <= 6u &&
                   (source_mode == 0u || source_mode == 2u ||
                    source_mode == 3u || source_mode == 5u)) {
            if (source_mode == 0u) {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "add.%c d%u,d%u", size_name,
                               destination_register, source_register);
            } else if (source_mode == 5u) {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "add.%c d%u,$%04x(a%u)", size_name,
                               destination_register, (unsigned)displacement,
                               source_register);
            } else {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "add.%c d%u,(a%u)%s", size_name,
                               destination_register, source_register,
                               source_mode == 3u ? "+" : "");
            }
        } else {
            instruction->supported = 0;
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           ".word $%04x", (unsigned)opcode);
        }
    } else if (opcode == UINT16_C(0x4eb9) || opcode == UINT16_C(0x4ef9)) {
        uint32_t target = 0u;
        status = vf2_m68k_read_u32(program, program_size, address + 2u, &target);
        instruction->length = 6u;
        if (status == VF2_OK) {
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "%s $%08x", opcode == UINT16_C(0x4eb9) ? "jsr" : "jmp",
                           (unsigned)target);
        }
    } else if (opcode == UINT16_C(0x4eba) || opcode == UINT16_C(0x4efa)) {
        uint16_t extension = 0u;
        status = vf2_m68k_read_u16(program, program_size, address + 2u,
                                   &extension);
        instruction->length = 4u;
        if (status == VF2_OK) {
            set_branch_text(instruction,
                            opcode == UINT16_C(0x4eba) ? "jsr" : "jmp",
                            address + 2u + (uint32_t)sign_extend16(extension));
        }
    } else if ((opcode & UINT16_C(0xfff8)) == UINT16_C(0x4e90)) {
        (void)snprintf(instruction->text, sizeof(instruction->text),
                       "jsr (a%u)", (unsigned)(opcode & 7u));
    } else if ((opcode & UINT16_C(0xfff8)) == UINT16_C(0x4ed0)) {
        (void)snprintf(instruction->text, sizeof(instruction->text),
                       "jmp (a%u)", (unsigned)(opcode & 7u));
    } else if ((opcode & UINT16_C(0xfff8)) == UINT16_C(0x4880) ||
               (opcode & UINT16_C(0xfff8)) == UINT16_C(0x48c0)) {
        (void)snprintf(instruction->text, sizeof(instruction->text),
                       "ext.%c d%u",
                       (opcode & UINT16_C(0x0040)) != 0u ? 'l' : 'w',
                       (unsigned)(opcode & 7u));
    } else if ((opcode & UINT16_C(0xffc0)) == UINT16_C(0x48c0) ||
               (opcode & UINT16_C(0xffc0)) == UINT16_C(0x4cc0)) {
        uint16_t mask = 0u;
        status = vf2_m68k_read_u16(program, program_size, address + 2u,
                                   &mask);
        instruction->length = 4u;
        if (status == VF2_OK) {
            const int to_memory =
                (opcode & UINT16_C(0xffc0)) == UINT16_C(0x48c0);
            const unsigned address_register = (unsigned)(opcode & 7u);
            if (to_memory) {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "movem.%c $%04x,-(a%u)",
                               (opcode & UINT16_C(0x0040)) != 0u ? 'l' : 'w',
                               (unsigned)mask, address_register);
            } else {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "movem.%c (a%u)+,$%04x",
                               (opcode & UINT16_C(0x0040)) != 0u ? 'l' : 'w',
                               address_register, (unsigned)mask);
            }
        }
    } else if ((opcode & UINT16_C(0xf1f8)) == UINT16_C(0x2040) ||
               (opcode & UINT16_C(0xf1f8)) == UINT16_C(0x3040) ||
               (opcode & UINT16_C(0xf1f8)) == UINT16_C(0x1040)) {
        const char size_name = (opcode & UINT16_C(0xf000)) == UINT16_C(0x2000)
            ? 'l'
            : ((opcode & UINT16_C(0xf000)) == UINT16_C(0x3000) ? 'w' : 'b');
        (void)snprintf(instruction->text, sizeof(instruction->text),
                       "move.%c d%u,a%u", size_name,
                       (unsigned)(opcode & 7u),
                       (unsigned)((opcode >> 9u) & 7u));
    } else if ((opcode & UINT16_C(0xf1f8)) == UINT16_C(0x2000) ||
               (opcode & UINT16_C(0xf1f8)) == UINT16_C(0x3000) ||
               (opcode & UINT16_C(0xf1f8)) == UINT16_C(0x1000)) {
        const char size_name = (opcode & UINT16_C(0xf000)) == UINT16_C(0x2000)
            ? 'l'
            : ((opcode & UINT16_C(0xf000)) == UINT16_C(0x3000) ? 'w' : 'b');
        (void)snprintf(instruction->text, sizeof(instruction->text),
                       "move.%c d%u,d%u", size_name,
                       (unsigned)(opcode & 7u),
                       (unsigned)((opcode >> 9u) & 7u));
    } else if (((opcode & UINT16_C(0xf000)) == UINT16_C(0x1000) ||
                (opcode & UINT16_C(0xf000)) == UINT16_C(0x2000) ||
                (opcode & UINT16_C(0xf000)) == UINT16_C(0x3000)) &&
               ((opcode >> 3u) & 7u) == 1u &&
               ((opcode >> 6u) & 7u) == 1u) {
        const char size_name = (opcode & UINT16_C(0xf000)) == UINT16_C(0x2000)
            ? 'l' : ((opcode & UINT16_C(0xf000)) == UINT16_C(0x3000) ? 'w' : 'b');
        (void)snprintf(instruction->text, sizeof(instruction->text),
                       "movea.%c a%u,a%u", size_name,
                       (unsigned)(opcode & 7u),
                       (unsigned)((opcode >> 9u) & 7u));
    } else if (((opcode & UINT16_C(0xf000)) == UINT16_C(0x1000) ||
                (opcode & UINT16_C(0xf000)) == UINT16_C(0x2000) ||
                (opcode & UINT16_C(0xf000)) == UINT16_C(0x3000)) &&
               ((opcode >> 3u) & 7u) == 1u &&
               (((opcode >> 6u) & 7u) == 5u ||
                ((opcode >> 6u) & 7u) == 6u)) {
        const unsigned destination_mode = (unsigned)((opcode >> 6u) & 7u);
        const unsigned destination_register =
            (unsigned)((opcode >> 9u) & 7u);
        const char size_name = (opcode & UINT16_C(0xf000)) == UINT16_C(0x2000)
            ? 'l' : ((opcode & UINT16_C(0xf000)) == UINT16_C(0x3000) ? 'w' : 'b');
        char destination_text[64];
        size_t extension_bytes = 0u;
        status = format_bit_ea(program, program_size, address + 2u,
                               destination_mode, destination_register,
                               destination_text, sizeof(destination_text),
                               &extension_bytes);
        instruction->length = 2u + extension_bytes;
        if (status == VF2_OK) {
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "move.%c a%u,%s", size_name,
                           (unsigned)(opcode & 7u), destination_text);
        }
    } else if (((opcode & UINT16_C(0xf000)) == UINT16_C(0x1000) ||
                (opcode & UINT16_C(0xf000)) == UINT16_C(0x2000) ||
                (opcode & UINT16_C(0xf000)) == UINT16_C(0x3000)) &&
               (opcode & UINT16_C(0x003f)) == UINT16_C(0x003c)) {
        const unsigned destination_register = (unsigned)((opcode >> 9u) & 7u);
        const unsigned destination_mode = (unsigned)((opcode >> 6u) & 7u);
        const unsigned size_code = (unsigned)((opcode >> 12u) & 3u);
        const char size_name = size_code == 2u ? 'l' :
                               (size_code == 3u ? 'w' : 'b');
        const size_t immediate_bytes = size_code == 2u ? 4u : 2u;
        uint32_t immediate = 0u;
        size_t extension_address = address + 2u + immediate_bytes;
        size_t extension_bytes = 0u;

        if (immediate_bytes == 4u) {
            status = vf2_m68k_read_u32(program, program_size, address + 2u,
                                       &immediate);
        } else {
            uint16_t value = 0u;
            status = vf2_m68k_read_u16(program, program_size, address + 2u,
                                       &value);
            immediate = value;
        }
        if (destination_mode == 5u || destination_mode == 6u ||
            (destination_mode == 7u && destination_register == 0u)) {
            extension_bytes = 2u;
        } else if (destination_mode == 7u && destination_register == 1u) {
            extension_bytes = 4u;
        }
        instruction->length = 2u + immediate_bytes + extension_bytes;
        if (status == VF2_OK && extension_bytes != 0u) {
            uint32_t extension = 0u;
            if (extension_bytes == 2u) {
                uint16_t value = 0u;
                status = vf2_m68k_read_u16(program, program_size,
                                           (uint32_t)extension_address, &value);
                extension = value;
            } else {
                status = vf2_m68k_read_u32(program, program_size,
                                           (uint32_t)extension_address,
                                           &extension);
            }
            if (status == VF2_OK) {
                if (destination_mode == 5u || destination_mode == 6u) {
                    (void)snprintf(instruction->text, sizeof(instruction->text),
                                   "move.%c #$%x,$%04x(a%u)", size_name,
                                   (unsigned)immediate, (unsigned)extension,
                                   destination_register);
                } else if (destination_mode == 7u &&
                           destination_register == 0u) {
                    (void)snprintf(instruction->text, sizeof(instruction->text),
                                   "move.%c #$%x,$%04x", size_name,
                                   (unsigned)immediate, (unsigned)extension);
                } else {
                    (void)snprintf(instruction->text, sizeof(instruction->text),
                                   "move.%c #$%x,$%08x", size_name,
                                   (unsigned)immediate, (unsigned)extension);
                }
            }
        } else if (status == VF2_OK && destination_mode == 0u) {
        (void)snprintf(instruction->text, sizeof(instruction->text),
                       "move.%c #$%x,d%u", size_name,
                       (unsigned)immediate, destination_register);
        } else if (status == VF2_OK && destination_mode == 2u) {
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "move.%c #$%x,(a%u)", size_name,
                           (unsigned)immediate, destination_register);
        } else if (status == VF2_OK && destination_mode == 3u) {
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "move.%c #$%x,(a%u)+", size_name,
                           (unsigned)immediate, destination_register);
        } else if (status == VF2_OK && destination_mode == 4u) {
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "move.%c #$%x,-(a%u)", size_name,
                           (unsigned)immediate, destination_register);
        } else if (status == VF2_OK && destination_mode == 1u) {
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "movea.%c #$%x,a%u", size_name,
                           (unsigned)immediate, destination_register);
        } else {
            instruction->supported = 0;
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           ".word $%04x", (unsigned)opcode);
        }
    } else if (((opcode & UINT16_C(0xf000)) == UINT16_C(0x1000) ||
                (opcode & UINT16_C(0xf000)) == UINT16_C(0x2000) ||
                (opcode & UINT16_C(0xf000)) == UINT16_C(0x3000)) &&
               ((opcode >> 3u) & 7u) >= 2u &&
                ((((opcode >> 3u) & 7u) <= 6u) ||
                 (((opcode >> 3u) & 7u) == 7u &&
                  (opcode & 7u) == 3u)) &&
               (((opcode >> 6u) & 7u) == 0u ||
                ((opcode >> 6u) & 7u) == 1u)) {
        const unsigned destination_register = (unsigned)((opcode >> 9u) & 7u);
        const unsigned source_mode = (unsigned)((opcode >> 3u) & 7u);
        const unsigned source_register = (unsigned)(opcode & 7u);
        const char size_name = (opcode & UINT16_C(0xf000)) == UINT16_C(0x2000)
            ? 'l' : ((opcode & UINT16_C(0xf000)) == UINT16_C(0x3000) ? 'w' : 'b');
        uint16_t displacement = 0u;
        instruction->length = source_mode == 5u || source_mode == 6u ||
                              (source_mode == 7u && source_register == 3u)
            ? 4u : 2u;
        if (source_mode == 5u || source_mode == 6u ||
            (source_mode == 7u && source_register == 3u)) {
            status = vf2_m68k_read_u16(program, program_size, address + 2u,
                                       &displacement);
        }
        if (status == VF2_OK) {
            if (source_mode == 2u) {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "move.%c (a%u),%c%u", size_name,
                               source_register,
                               ((opcode >> 6u) & 7u) == 1u ? 'a' : 'd',
                               destination_register);
            } else if (source_mode == 3u) {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "move.%c (a%u)+,%c%u", size_name,
                               source_register,
                               ((opcode >> 6u) & 7u) == 1u ? 'a' : 'd',
                               destination_register);
            } else if (source_mode == 6u) {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "move.%c $%02x(a%u,d%u.%c),%c%u", size_name,
                               (unsigned)(displacement & 0xffu), source_register,
                               (unsigned)((displacement >> 12u) & 7u),
                               (displacement & UINT16_C(0x0800)) != 0u ? 'l' : 'w',
                               ((opcode >> 6u) & 7u) == 1u ? 'a' : 'd',
                               destination_register);
            } else if (source_mode == 7u && source_register == 3u) {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "move.%c $%02x(pc,d%u.%c),%c%u", size_name,
                               (unsigned)(displacement & 0xffu),
                               (unsigned)((displacement >> 12u) & 7u),
                               (displacement & UINT16_C(0x0800)) != 0u ? 'l' : 'w',
                               ((opcode >> 6u) & 7u) == 1u ? 'a' : 'd',
                               destination_register);
            } else {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "move.%c $%04x(a%u),%c%u", size_name,
                               (unsigned)displacement, source_register,
                               ((opcode >> 6u) & 7u) == 1u ? 'a' : 'd',
                               destination_register);
            }
        }
    } else if (((opcode & UINT16_C(0xf000)) == UINT16_C(0x1000) ||
                (opcode & UINT16_C(0xf000)) == UINT16_C(0x2000) ||
                (opcode & UINT16_C(0xf000)) == UINT16_C(0x3000)) &&
               ((opcode >> 3u) & 7u) == 0u &&
               (((opcode >> 6u) & 7u) == 2u ||
                ((opcode >> 6u) & 7u) == 3u)) {
        const unsigned destination_register = (unsigned)((opcode >> 9u) & 7u);
        const unsigned destination_mode = (unsigned)((opcode >> 6u) & 7u);
        const unsigned source_register = (unsigned)(opcode & 7u);
        const char size_name = (opcode & UINT16_C(0xf000)) == UINT16_C(0x2000)
            ? 'l' : ((opcode & UINT16_C(0xf000)) == UINT16_C(0x3000) ? 'w' : 'b');
        if (destination_mode == 2u) {
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "move.%c d%u,(a%u)", size_name, source_register,
                           destination_register);
        } else {
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "move.%c d%u,(a%u)+", size_name, source_register,
                           destination_register);
        }
    } else if (((opcode & UINT16_C(0xf000)) == UINT16_C(0x1000) ||
                (opcode & UINT16_C(0xf000)) == UINT16_C(0x2000) ||
                (opcode & UINT16_C(0xf000)) == UINT16_C(0x3000)) &&
               ((opcode >> 3u) & 7u) == 0u &&
               (((opcode >> 6u) & 7u) == 5u ||
                ((opcode >> 6u) & 7u) == 6u)) {
        const unsigned destination_register = (unsigned)((opcode >> 9u) & 7u);
        const unsigned source_register = (unsigned)(opcode & 7u);
        const char size_name = (opcode & UINT16_C(0xf000)) == UINT16_C(0x2000)
            ? 'l' : ((opcode & UINT16_C(0xf000)) == UINT16_C(0x3000) ? 'w' : 'b');
        uint16_t displacement = 0u;
        status = vf2_m68k_read_u16(program, program_size, address + 2u,
                                   &displacement);
        instruction->length = 4u;
        if (status == VF2_OK) {
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "move.%c d%u,$%04x(a%u)", size_name,
                           source_register, (unsigned)displacement,
                           destination_register);
        }
    } else if (((opcode & UINT16_C(0xf000)) == UINT16_C(0x1000) ||
                (opcode & UINT16_C(0xf000)) == UINT16_C(0x2000) ||
                (opcode & UINT16_C(0xf000)) == UINT16_C(0x3000)) &&
               ((opcode >> 3u) & 7u) == 1u &&
               ((opcode >> 6u) & 7u) == 5u) {
        const unsigned destination_register =
            (unsigned)((opcode >> 9u) & 7u);
        const unsigned source_register = (unsigned)(opcode & 7u);
        const char size_name = (opcode & UINT16_C(0xf000)) == UINT16_C(0x2000)
            ? 'l' : ((opcode & UINT16_C(0xf000)) == UINT16_C(0x3000) ? 'w' : 'b');
        uint16_t displacement = 0u;
        status = vf2_m68k_read_u16(program, program_size, address + 2u,
                                   &displacement);
        instruction->length = 4u;
        if (status == VF2_OK) {
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "move.%c a%u,$%04x(a%u)", size_name,
                           source_register, (unsigned)displacement,
                           destination_register);
        }
    } else if (((opcode & UINT16_C(0xf000)) == UINT16_C(0x1000) ||
                (opcode & UINT16_C(0xf000)) == UINT16_C(0x2000) ||
                (opcode & UINT16_C(0xf000)) == UINT16_C(0x3000)) &&
               (((opcode >> 3u) & 7u) == 2u ||
                ((opcode >> 3u) & 7u) == 3u) &&
               (((opcode >> 6u) & 7u) == 2u ||
                ((opcode >> 6u) & 7u) == 3u)) {
        const unsigned destination_register = (unsigned)((opcode >> 9u) & 7u);
        const unsigned destination_mode = (unsigned)((opcode >> 6u) & 7u);
        const unsigned source_mode = (unsigned)((opcode >> 3u) & 7u);
        const unsigned source_register = (unsigned)(opcode & 7u);
        const char size_name = (opcode & UINT16_C(0xf000)) == UINT16_C(0x2000)
            ? 'l' : ((opcode & UINT16_C(0xf000)) == UINT16_C(0x3000) ? 'w' : 'b');
        (void)snprintf(instruction->text, sizeof(instruction->text),
                       "move.%c (a%u)%s,(a%u)%s", size_name, source_register,
                       source_mode == 3u ? "+" : "", destination_register,
                       destination_mode == 3u ? "+" : "");
    } else if ((opcode & UINT16_C(0xf1ff)) == UINT16_C(0x203c) ||
               (opcode & UINT16_C(0xf1ff)) == UINT16_C(0x303c) ||
               (opcode & UINT16_C(0xf1ff)) == UINT16_C(0x103c)) {
        uint32_t immediate = 0u;
        const size_t immediate_bytes =
            (opcode & UINT16_C(0xf000)) == UINT16_C(0x2000) ? 4u : 2u;
        if (immediate_bytes == 4u) {
            status = vf2_m68k_read_u32(program, program_size, address + 2u,
                                       &immediate);
        } else {
            uint16_t value = 0u;
            status = vf2_m68k_read_u16(program, program_size, address + 2u,
                                       &value);
            immediate = value;
        }
        instruction->length = immediate_bytes + 2u;
        if (status == VF2_OK) {
            const char size_name = (opcode & UINT16_C(0xf000)) == UINT16_C(0x2000)
                ? 'l'
                : ((opcode & UINT16_C(0xf000)) == UINT16_C(0x3000) ? 'w' : 'b');
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "move.%c #$%x,d%u", size_name, (unsigned)immediate,
                           (unsigned)((opcode >> 9u) & 7u));
        }
    } else if ((opcode & UINT16_C(0xf1c0)) == UINT16_C(0xc0c0) ||
               (opcode & UINT16_C(0xf1c0)) == UINT16_C(0xc1c0)) {
        const int is_signed = (opcode & UINT16_C(0x0100)) != 0u;
        const unsigned destination_register =
            (unsigned)((opcode >> 9u) & 7u);
        uint16_t immediate = 0u;
        if ((opcode & UINT16_C(0x003f)) == UINT16_C(0x003c)) {
            status = vf2_m68k_read_u16(program, program_size, address + 2u,
                                       &immediate);
            instruction->length = 4u;
            if (status == VF2_OK) {
                (void)snprintf(instruction->text, sizeof(instruction->text),
                               "%s #$%04x,d%u", is_signed ? "muls" : "mulu",
                               (unsigned)immediate, destination_register);
            }
        } else if ((opcode & UINT16_C(0x0038)) == 0u) {
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "%s.w d%u,d%u", is_signed ? "muls" : "mulu",
                           (unsigned)(opcode & 7u), destination_register);
        } else {
            instruction->supported = 0;
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           ".word $%04x", (unsigned)opcode);
        }
    } else if (((opcode & UINT16_C(0xf000)) == UINT16_C(0x1000) ||
                (opcode & UINT16_C(0xf000)) == UINT16_C(0x2000) ||
                (opcode & UINT16_C(0xf000)) == UINT16_C(0x3000)) &&
               (((opcode >> 3u) & 7u) >= 2u) &&
               (((opcode >> 6u) & 7u) >= 2u) &&
               !((((opcode >> 3u) & 7u) == 7u) &&
                 ((opcode & 7u) == 4u))) {
        const unsigned destination_register =
            (unsigned)((opcode >> 9u) & 7u);
        const unsigned destination_mode = (unsigned)((opcode >> 6u) & 7u);
        const unsigned source_mode = (unsigned)((opcode >> 3u) & 7u);
        const unsigned source_register = (unsigned)(opcode & 7u);
        const char size_name = (opcode & UINT16_C(0xf000)) == UINT16_C(0x2000)
            ? 'l' : ((opcode & UINT16_C(0xf000)) == UINT16_C(0x3000) ? 'w' : 'b');
        size_t source_extension_bytes = 0u;
        size_t destination_extension_bytes = 0u;
        char source_text[64];
        char destination_text[64];

        status = format_bit_ea(program, program_size, address + 2u,
                               source_mode, source_register, source_text,
                               sizeof(source_text), &source_extension_bytes);
        if (status == VF2_OK) {
            status = format_bit_ea(
                program, program_size,
                address + 2u + (uint32_t)source_extension_bytes,
                destination_mode, destination_register, destination_text,
                sizeof(destination_text), &destination_extension_bytes
            );
        }
        instruction->length = 2u + source_extension_bytes +
                              destination_extension_bytes;
        if (status == VF2_OK) {
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "move.%c %.55s,%.55s", size_name, source_text,
                           destination_text);
        } else if (status == VF2_ERROR_UNSUPPORTED) {
            instruction->supported = 0;
            status = VF2_OK;
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           ".word $%04x", (unsigned)opcode);
        }
    } else if (opcode == UINT16_C(0x13fc) || opcode == UINT16_C(0x33fc) ||
               opcode == UINT16_C(0x23fc)) {
        uint32_t target = 0u;
        uint32_t immediate = 0u;
        size_t immediate_bytes = opcode == UINT16_C(0x13fc) ? 2u :
                                 (opcode == UINT16_C(0x33fc) ? 2u : 4u);
        status = vf2_m68k_read_u32(
            program, program_size, address + 4u, &target
        );
        if (immediate_bytes == 2u) {
            uint16_t value = 0u;
            if (status == VF2_OK) {
                status = vf2_m68k_read_u16(program, program_size,
                                           address + 2u, &value);
            }
            immediate = value;
        } else if (status == VF2_OK) {
            status = vf2_m68k_read_u32(program, program_size,
                                       address + 2u, &immediate);
        }
        instruction->length = immediate_bytes + 6u;
        if (status == VF2_OK) {
            const char size_name = opcode == UINT16_C(0x13fc) ? 'b' :
                                   (opcode == UINT16_C(0x33fc) ? 'w' : 'l');
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "move.%c #$%x,$%08x", size_name,
                           (unsigned)immediate, (unsigned)target);
        }
    } else if (((opcode & UINT16_C(0xf1c0)) == UINT16_C(0x11c0) ||
                (opcode & UINT16_C(0xf1c0)) == UINT16_C(0x31c0) ||
                (opcode & UINT16_C(0xf1c0)) == UINT16_C(0x21c0)) &&
               (opcode & UINT16_C(0x0038)) == 0u) {
        uint32_t target = 0u;
        const char size_name = (opcode & UINT16_C(0xf000)) == UINT16_C(0x2000)
            ? 'l' : ((opcode & UINT16_C(0xf000)) == UINT16_C(0x3000)
                ? 'w' : 'b');
        status = vf2_m68k_read_u32(program, program_size, address + 2u,
                                   &target);
        instruction->length = 6u;
        if (status == VF2_OK) {
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "move.%c d%u,$%08x", size_name,
                           (unsigned)(opcode & 7u), (unsigned)target);
        }
    } else if ((opcode & UINT16_C(0xf1c0)) == UINT16_C(0x41c0) &&
               (opcode & UINT16_C(0x003f)) == UINT16_C(0x0038)) {
        uint16_t displacement = 0u;
        status = vf2_m68k_read_u16(program, program_size, address + 2u,
                                   &displacement);
        instruction->length = 4u;
        if (status == VF2_OK) {
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "lea $%04x,a%u", (unsigned)displacement,
                           (unsigned)((opcode >> 9u) & 7u));
        }
    } else if ((opcode & UINT16_C(0xf1c0)) == UINT16_C(0x41c0) &&
               (opcode & UINT16_C(0x003f)) == UINT16_C(0x0039)) {
        uint32_t target = 0u;
        status = vf2_m68k_read_u32(program, program_size, address + 2u,
                                   &target);
        instruction->length = 6u;
        if (status == VF2_OK) {
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "lea $%08x,a%u", (unsigned)target,
                           (unsigned)((opcode >> 9u) & 7u));
        }
    } else if ((opcode & UINT16_C(0xf1c0)) == UINT16_C(0x41c0) &&
               (opcode & UINT16_C(0x003f)) == UINT16_C(0x003a)) {
        uint16_t displacement = 0u;
        status = vf2_m68k_read_u16(program, program_size, address + 2u,
                                   &displacement);
        instruction->length = 4u;
        if (status == VF2_OK) {
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "lea $%08x(pc),a%u",
                           (unsigned)(address + 2u +
                                      (uint32_t)sign_extend16(displacement)),
                           (unsigned)((opcode >> 9u) & 7u));
        }
    } else if ((opcode & UINT16_C(0xf1c0)) == UINT16_C(0x41c0) &&
               (opcode & UINT16_C(0x003f)) == UINT16_C(0x003b)) {
        uint16_t extension = 0u;
        status = vf2_m68k_read_u16(program, program_size, address + 2u,
                                   &extension);
        instruction->length = 4u;
        if (status == VF2_OK) {
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "lea $%02x(pc,d%u.%c),a%u",
                           (unsigned)(extension & 0xffu),
                           (unsigned)((extension >> 12u) & 7u),
                           (extension & UINT16_C(0x0800)) != 0u ? 'l' : 'w',
                           (unsigned)((opcode >> 9u) & 7u));
        }
    } else if ((opcode & UINT16_C(0xf1c0)) == UINT16_C(0x41c0) &&
               (opcode & UINT16_C(0x0038)) == UINT16_C(0x0010)) {
        (void)snprintf(instruction->text, sizeof(instruction->text),
                       "lea (a%u),a%u", (unsigned)(opcode & 7u),
                       (unsigned)((opcode >> 9u) & 7u));
    } else if ((opcode & UINT16_C(0xf1c0)) == UINT16_C(0x41c0) &&
               (opcode & UINT16_C(0x0038)) == UINT16_C(0x0028)) {
        uint16_t displacement = 0u;
        status = vf2_m68k_read_u16(program, program_size, address + 2u,
                                   &displacement);
        instruction->length = 4u;
        if (status == VF2_OK) {
            (void)snprintf(instruction->text, sizeof(instruction->text),
                           "lea $%04x(a%u),a%u", (unsigned)displacement,
                           (unsigned)(opcode & 7u),
                           (unsigned)((opcode >> 9u) & 7u));
        }
    } else if ((opcode & UINT16_C(0xff00)) == UINT16_C(0x4200)) {
        (void)snprintf(instruction->text, sizeof(instruction->text),
                       "clr.%c d%u", ((opcode >> 6u) & 3u) == 1u ? 'w' : 'l',
                       (unsigned)(opcode & 7u));
    } else {
        instruction->supported = 0;
        (void)snprintf(instruction->text, sizeof(instruction->text),
                       ".word $%04x", (unsigned)opcode);
    }
    return status;
}
