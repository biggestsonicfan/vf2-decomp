#include "vf2/m68k.h"
#include "vf2/m68k_step.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ROM-independent pin for the 68000 PC-relative base.
 *
 * On the 68000 every PC-relative form (Bcc.w, BRA/BSR.w, DBcc,
 * JSR/JMP (d16,PC), and the (d16,PC) / (d8,PC,Xn) operands) is relative
 * to the address of the extension word, i.e. opcode address + 2.
 * A base of opcode address + 4 lands branch targets in the middle of
 * instructions. Measured on epr-17574.30 (0x180..0x2700, decoder
 * instruction boundaries): with +2 all 176 Bcc.w, 42 BRA/BSR.w, 67 DBcc
 * and 183 JSR/JMP (d16,PC) targets land on an instruction start; with
 * +4 only 31, 4, 19 and 125 do. */

static int failures;

#define EXPECT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            ++failures; \
        } \
    } while (0)

static void expect_text(const uint8_t *program, size_t size, uint32_t address,
                        const char *text)
{
    vf2_m68k_instruction instruction;
    EXPECT_TRUE(vf2_m68k_decode(program, size, address, &instruction) == VF2_OK);
    if (strcmp(instruction.text, text) != 0) {
        fprintf(stderr, "decode at 0x%x: got '%s', want '%s'\n",
                (unsigned)address, instruction.text, text);
        ++failures;
    }
}

int main(void)
{
    /* Decoder: printed targets use the extension-word base. */
    static const uint8_t d_program[] = {
        0x66, 0x00, 0x00, 0x04, /* 0x00 bne.w +4         -> 0x00+2+4 = 0x06 */
        0x60, 0x00, 0xff, 0xfc, /* 0x04 bra.w -4         -> 0x04+2-4 = 0x02 */
        0x51, 0xc8, 0xff, 0xf8, /* 0x08 dbf d0,-8        -> 0x08+2-8 = 0x02 */
        0x4e, 0xfa, 0x00, 0x06, /* 0x0c jmp (d16,PC)     -> 0x0c+2+6 = 0x14 */
        0x4e, 0xba, 0x00, 0x06, /* 0x10 jsr (d16,PC)     -> 0x10+2+6 = 0x18 */
        0x41, 0xfa, 0x00, 0x06  /* 0x14 lea (d16,PC),a0  -> 0x14+2+6 = 0x1c */
    };
    /* Stepper: the same forms, executed. */
    static const uint8_t s_program[] = {
        0x70, 0x01,             /* 0x00 moveq #1,d0 (Z=0) */
        0x66, 0x00, 0x00, 0x04, /* 0x02 bne.w +4 -> 0x08 (taken) */
        0x70, 0x63,             /* 0x06 moveq #99,d0 (must be skipped) */
        0x74, 0x03,             /* 0x08 moveq #3,d2 */
        0x53, 0x02,             /* 0x0a subq.b #1,d2 */
        0x51, 0xca, 0xff, 0xfc, /* 0x0c dbf d2,-4 -> 0x0c+2-4 = 0x0a */
        0x41, 0xfa, 0x00, 0x06, /* 0x10 lea (d16,PC),a0 -> 0x10+2+6 = 0x18 */
        0x36, 0x3a, 0x00, 0x06, /* 0x14 move.w (d16,PC),d3 -> word at 0x1c */
        0x4e, 0xfa, 0x00, 0x04, /* 0x18 jmp (d16,PC) -> 0x18+2+4 = 0x1e */
        0xbe, 0xef,             /* 0x1c data word */
        0x4e, 0xba, 0x00, 0x04, /* 0x1e jsr (d16,PC) -> 0x1e+2+4 = 0x24 */
        0x4e, 0x71,             /* 0x22 nop (return point) */
        0x78, 0x09,             /* 0x24 moveq #9,d4 */
        0x4e, 0x75              /* 0x26 rts (halts with stop_on_return) */
    };
    static uint8_t ram[32768];
    vf2_scsp scsp;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;

    expect_text(d_program, sizeof(d_program), 0x00u, "bne $00000006");
    expect_text(d_program, sizeof(d_program), 0x04u, "bra $00000002");
    expect_text(d_program, sizeof(d_program), 0x08u, "dbf d0,$00000002");
    expect_text(d_program, sizeof(d_program), 0x0cu, "jmp $00000014");
    expect_text(d_program, sizeof(d_program), 0x10u, "jsr $00000018");
    expect_text(d_program, sizeof(d_program), 0x14u, "lea $0000001c(pc),a0");

    memset(ram, 0, sizeof(ram));
    vf2_scsp_initialize(&scsp);
    bus.ram = ram;
    bus.ram_size = sizeof(ram);
    bus.rom = s_program;
    bus.rom_size = sizeof(s_program);
    bus.sample_rom = NULL;
    bus.sample_rom_size = 0u;
    bus.scsp = &scsp;
    bus.event_count = 0u;
    vf2_m68k_regs_reset(&regs);
    regs.a[7] = 32512u;
    status = vf2_m68k_run(&bus, &regs, 0xffffffu, 1, 100u, &done);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.d[0] == 1u);           /* bne.w skipped moveq #99 */
    EXPECT_TRUE(regs.d[2] == 65535u);       /* dbf looped to the subq */
    EXPECT_TRUE(regs.a[0] == 0x18u);        /* lea (d16,PC) */
    EXPECT_TRUE(regs.d[3] == 0xbeefu);      /* move.w (d16,PC) */
    EXPECT_TRUE(regs.d[4] == 9u);           /* jmp then jsr (d16,PC) */
    EXPECT_TRUE(bus.event_count == 0u);
    return failures == 0 ? 0 : 1;
}
