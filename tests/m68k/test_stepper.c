#include "vf2/m68k_step.h"

#include <stdint.h>
#include <stdio.h>

static int failures;

#define EXPECT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            ++failures; \
        } \
    } while (0)

static void make_bus(
    vf2_m68k_bus *bus,
    vf2_scsp *scsp,
    uint8_t *ram,
    const uint8_t *rom,
    size_t rom_size
)
{
    size_t index = 0u;
    bus->ram = ram;
    bus->ram_size = 32768u;
    bus->rom = rom;
    bus->rom_size = rom_size;
    bus->sample_rom = NULL;
    bus->sample_rom_size = 0u;
    bus->scsp = scsp;
    bus->event_count = 0u;
    for (index = 0u; index < 32768u; ++index) {
        ram[index] = 0u;
    }
    vf2_scsp_initialize(scsp);
}

int main(void)
{
    static const uint8_t program[] = {
        0x70, 0x0a,             /* 0x00 moveq #10,d0 */
        0x56, 0x00,             /* 0x02 addq.b #3,d0 -> 13 */
        0x0c, 0x00, 0x00, 0x0d, /* 0x04 cmpi.b #$0d,d0 (Z=1) */
        0x66, 0x04,             /* 0x08 bne +4 (not taken) */
        0x72, 0x01,             /* 0x0a moveq #1,d1 */
        0x60, 0x02,             /* 0x0c bra +2 -> 0x10 */
        0x72, 0x02,             /* 0x0e moveq #2,d1 (skipped) */
        0x74, 0x03,             /* 0x10 moveq #3,d2 */
        0x53, 0x02,             /* 0x12 subq.b #1,d2 */
        0x51, 0xca, 0xff, 0xfc, /* 0x14 dbf d2,-4 -> 0x14+2-4 = 0x12 (4 iterations) */
        0x41, 0xf8, 0x20, 0x00, /* 0x18 lea $2000,a0 */
        0x28, 0x3c, 0x12, 0x34, 0x56, 0x78, /* 0x1c move.l #$12345678,d4 */
        0x20, 0x84,             /* 0x22 move.l d4,(a0) */
        0x4e, 0xba, 0x00, 0x04, /* 0x24 jsr +4 -> 0x24+2+4 = 0x2a */
        0x60, 0x06,             /* 0x28 bra +6 -> 0x30 (stop) */
        0x76, 0x07,             /* 0x2a moveq #7,d3 */
        0x4e, 0x75              /* 0x2c rts (halts with stop_on_return) */
    };
    static const uint8_t bad_program[] = { 0x4a, 0xfc };
    static uint8_t ram[32768];
    vf2_scsp scsp;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;

    make_bus(&bus, &scsp, ram, program, sizeof(program));
    vf2_m68k_regs_reset(&regs);
    regs.a[7] = 32512u;
    status = vf2_m68k_run(&bus, &regs, 0x30u, 1, 100u, &done);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.d[0] == 13u);
    EXPECT_TRUE(regs.d[1] == 1u);
    EXPECT_TRUE(regs.d[2] == 65535u);
    EXPECT_TRUE(regs.d[3] == 7u);
    EXPECT_TRUE(regs.d[4] == UINT32_C(0x12345678));
    EXPECT_TRUE(regs.a[0] == 0x2000u);
    EXPECT_TRUE(regs.z == 0u);
    EXPECT_TRUE(ram[0x2000u] == 0x12u);
    EXPECT_TRUE(ram[0x2001u] == 0x34u);
    EXPECT_TRUE(ram[0x2002u] == 0x56u);
    EXPECT_TRUE(ram[0x2003u] == 0x78u);
    EXPECT_TRUE(regs.a[7] == 32508u);
    EXPECT_TRUE(bus.event_count == 0u);

    /* Fail closed on an unimplemented opcode. */
    make_bus(&bus, &scsp, ram, bad_program, sizeof(bad_program));
    vf2_m68k_regs_reset(&regs);
    status = vf2_m68k_run(&bus, &regs, 0x10u, 0, 100u, &done);
    EXPECT_TRUE(status == VF2_ERROR_UNSUPPORTED);

    /* Fail closed on step exhaustion. */
    make_bus(&bus, &scsp, ram, program, sizeof(program));
    vf2_m68k_regs_reset(&regs);
    regs.a[7] = 32512u;
    status = vf2_m68k_run(&bus, &regs, 0x30u, 1, 2u, &done);
    EXPECT_TRUE(status == VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(done == 2u);

    return failures == 0 ? 0 : 1;
}
