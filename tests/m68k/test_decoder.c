#include "vf2/m68k.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define EXPECT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            ++failures; \
        } \
    } while (0)

int main(void)
{
    static const uint8_t program[] = {
        0x70, 0xfe,             /* moveq #-2,d0 */
        0x60, 0x02,             /* bra +2 */
        0x4e, 0x75,             /* rts */
        0x4e, 0xba, 0xff, 0xf8,/* jsr pc,-8 */
        0x4e, 0x71              /* nop */
    };
    vf2_m68k_instruction instruction;
    static const uint8_t handler[] = {
        0x74, 0x0f,                         /* moveq #15,d2 */
        0x13, 0xc0, 0x00, 0x40, 0x00, 0x01, /* move.b d0,$00400001 */
        0x48, 0xe7, 0xff, 0xf8,             /* movem.l $fff8,-(a7) */
        0x4c, 0xdf, 0x1f, 0xff,             /* movem.l (a7)+,$1fff */
        0x4e, 0x73                            /* rte */
    };
    static const uint8_t operations[] = {
        0x08, 0x28, 0x00, 0x00, 0x00, 0x02, /* btst #0,$2(a0) */
        0x53, 0x2e, 0x15, 0x1e,             /* subq.b #1,$151e(a6) */
        0x0c, 0x2a, 0x00, 0xff, 0x00, 0x09, /* cmpi.b #$ff,$9(a2) */
        0xb4, 0x12,                         /* cmp.w (a2),d2 */
        0xd5, 0xfc, 0x00, 0x00, 0x00, 0x09, /* adda.l #9,a2 */
        0x41, 0xfa, 0xfd, 0xd2              /* lea $ffffffd6(pc),a0 */
    };
    static const uint8_t dispatcher[] = {
        0x02, 0x80, 0x00, 0x00, 0x0f, 0xff, /* andi.l #$fff,d0 */
        0x1d, 0x40, 0x15, 0x0e,             /* move.b d0,$150e(a6) */
        0x02, 0x00, 0x00, 0x0f,             /* andi.b #$f,d0 */
        0x58, 0x6e, 0x15, 0x08,             /* addq.w #4,$1508(a6) */
        0x02, 0x6e, 0x0f, 0xff, 0x15, 0x08, /* andi.w #$fff,$1508(a6) */
        0xd0, 0x28, 0x00, 0x04,             /* add.w $4(a0),d0 */
        0xd5, 0xc0                          /* adda.l d0,a2 */
    };
    static const uint8_t bit_operations[] = {
        0x08, 0xe9, 0x00, 0x01, 0x00, 0x02, /* bset #1,$2(a1) */
        0x09, 0x72, 0x00, 0x00              /* bchg d4,(a2,d0.w) */
    };
    static const uint8_t handler_memory[] = {
        0xc0, 0xfc, 0x00, 0x06,             /* mulu #6,d0 */
        0x3d, 0x6a, 0x00, 0x00, 0x15, 0x16,  /* move.w $0(a2),$1516(a6) */
        0x04, 0x03, 0x00, 0xf0,             /* subi.b #$f0,d3 */
        0x24, 0x71, 0x00, 0x38,             /* move.l $38(a1,d0.w),a2 */
        0x29, 0x4a, 0x00, 0x08              /* move.l a2,$8(a4) */
    };
    static const uint8_t shifts[] = {
        0xe5, 0x83                         /* asl.l #2,d3 */
    };
    static const uint8_t handler_arithmetic[] = {
        0x84, 0x40,                         /* or.w d0,d2 */
        0x44, 0x02,                         /* neg.b d2 */
        0x53, 0x02,                         /* subq.b #1,d2 */
        0x08, 0x02, 0x00, 0x00,             /* btst #0,d2 */
        0x56, 0x8a,                         /* addq.l #3,a2 */
        0xb0, 0x42                          /* cmp.w d2,d0 */
    };
    static const uint8_t extension[] = {
        0x48, 0x83                          /* ext.w d3 */
    };
    static const uint8_t jump_table[] = {
        0x43, 0xfb, 0x20, 0x06              /* lea $6(pc,d2.w),a1 */
    };
    static const uint8_t voice_helpers[] = {
        0x52, 0x00,                         /* addq.b #1,d0 */
        0xc0, 0xc2,                         /* mulu.w d2,d0 */
        0x46, 0x06,                         /* not.b d6 */
        0x12, 0x3b, 0x00, 0x4a,             /* move.b $4a(pc,d0.w),d1 */
        0x4a, 0x02,                         /* tst.b d2 */
        0x06, 0x86, 0x00, 0x00, 0x01, 0xff, /* addi.l #$1ff,d6 */
        0x96, 0x82,                         /* sub.l d2,d3 */
        0x20, 0x4d,                         /* movea.l a5,a0 */
        0x25, 0x8b, 0x00, 0x00,             /* move.l a3,$0(a2,d0.w) */
        0xb7, 0xfc, 0x00, 0x06, 0xff, 0xff /* cmpa.l #$6ffff,a3 */
    };
    static const uint8_t stream_helpers[] = {
        0x53, 0x6a, 0x00, 0x02,             /* subq.w #1,$2(a2) */
        0x0c, 0x82, 0xff, 0xff, 0xff, 0xff, /* cmpi.l #$ffffffff,d2 */
        0x4a, 0x2a, 0x00, 0x0c              /* tst.b $c(a2) */
    };
    static const uint8_t indexed_quick[] = {
        0x06, 0x31, 0x00, 0x50, 0x10, 0x0b, /* addi.b #$50,$b(a1,d1.w) */
        0x56, 0x31, 0x10, 0x0b              /* addq.b #3,$b(a1,d1.w) */
    };
    static const uint8_t address_immediate[] = {
        0x99, 0xfc, 0x00, 0x00, 0x00, 0x20  /* suba.l #$20,a4 */
    };

    EXPECT_TRUE(vf2_m68k_decode(program, sizeof(program), 0u, &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 2u);
    EXPECT_TRUE(strcmp(instruction.text, "moveq #-2,d0") == 0);
    EXPECT_TRUE(vf2_m68k_decode(program, sizeof(program), 2u, &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 2u);
    EXPECT_TRUE(strcmp(instruction.text, "bra $00000006") == 0);
    EXPECT_TRUE(vf2_m68k_decode(program, sizeof(program), 6u, &instruction) == VF2_OK);
    EXPECT_TRUE(strcmp(instruction.text, "jsr $00000000") == 0);
    EXPECT_TRUE(vf2_m68k_decode(program, sizeof(program), 10u, &instruction) == VF2_OK);
    EXPECT_TRUE(strcmp(instruction.text, "nop") == 0);
    EXPECT_TRUE(vf2_m68k_decode(program, sizeof(program) - 1u, 10u,
                                &instruction) == VF2_ERROR_OUT_OF_BOUNDS);

    EXPECT_TRUE(vf2_m68k_decode(handler, sizeof(handler), 0u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(strcmp(instruction.text, "moveq #15,d2") == 0);
    EXPECT_TRUE(vf2_m68k_decode(handler, sizeof(handler), 2u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 6u);
    EXPECT_TRUE(strcmp(instruction.text, "move.b d0,$00400001") == 0);
    EXPECT_TRUE(vf2_m68k_decode(handler, sizeof(handler), 8u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(strcmp(instruction.text, "movem.l $fff8,-(a7)") == 0);
    EXPECT_TRUE(vf2_m68k_decode(handler, sizeof(handler), 12u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(strcmp(instruction.text, "movem.l (a7)+,$1fff") == 0);
    EXPECT_TRUE(vf2_m68k_decode(handler, sizeof(handler), 16u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(strcmp(instruction.text, "rte") == 0);

    EXPECT_TRUE(vf2_m68k_decode(operations, sizeof(operations), 0u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 6u);
    EXPECT_TRUE(strcmp(instruction.text, "btst #0,$0002(a0)") == 0);
    EXPECT_TRUE(vf2_m68k_decode(operations, sizeof(operations), 6u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(strcmp(instruction.text, "subq.b #1,$151e(a6)") == 0);
    EXPECT_TRUE(vf2_m68k_decode(operations, sizeof(operations), 10u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 6u);
    EXPECT_TRUE(strcmp(instruction.text, "cmpi.b #$ff,$0009(a2)") == 0);
    EXPECT_TRUE(vf2_m68k_decode(operations, sizeof(operations), 16u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(strcmp(instruction.text, "cmp.w (a2),d2") == 0);
    EXPECT_TRUE(vf2_m68k_decode(operations, sizeof(operations), 18u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(strcmp(instruction.text, "adda.l #$9,a2") == 0);
    EXPECT_TRUE(vf2_m68k_decode(operations, sizeof(operations), 24u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(strcmp(instruction.text, "lea $fffffdec(pc),a0") == 0);

    EXPECT_TRUE(vf2_m68k_decode(dispatcher, sizeof(dispatcher), 0u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 6u);
    EXPECT_TRUE(strcmp(instruction.text, "andi.l #$fff,d0") == 0);
    EXPECT_TRUE(vf2_m68k_decode(dispatcher, sizeof(dispatcher), 6u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(strcmp(instruction.text, "move.b d0,$150e(a6)") == 0);
    EXPECT_TRUE(vf2_m68k_decode(dispatcher, sizeof(dispatcher), 10u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(strcmp(instruction.text, "andi.b #$f,d0") == 0);
    EXPECT_TRUE(vf2_m68k_decode(dispatcher, sizeof(dispatcher), 14u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(strcmp(instruction.text, "addq.w #4,$1508(a6)") == 0);
    EXPECT_TRUE(vf2_m68k_decode(dispatcher, sizeof(dispatcher), 18u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(strcmp(instruction.text, "andi.w #$fff,$1508(a6)") == 0);
    EXPECT_TRUE(vf2_m68k_decode(dispatcher, sizeof(dispatcher), 24u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(strcmp(instruction.text, "add.w $0004(a0),d0") == 0);
    EXPECT_TRUE(vf2_m68k_decode(dispatcher, sizeof(dispatcher), 28u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(strcmp(instruction.text, "adda.l d0,a2") == 0);

    EXPECT_TRUE(vf2_m68k_decode(bit_operations, sizeof(bit_operations), 0u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 6u);
    EXPECT_TRUE(strcmp(instruction.text, "bset #1,$0002(a1)") == 0);
    EXPECT_TRUE(vf2_m68k_decode(bit_operations, sizeof(bit_operations), 6u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 4u);
    EXPECT_TRUE(strcmp(instruction.text, "bchg d4,$00(a2,d0.w)") == 0);

    EXPECT_TRUE(vf2_m68k_decode(handler_memory, sizeof(handler_memory), 0u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 4u);
    EXPECT_TRUE(strcmp(instruction.text, "mulu #$0006,d0") == 0);
    EXPECT_TRUE(vf2_m68k_decode(handler_memory, sizeof(handler_memory), 4u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 6u);
    EXPECT_TRUE(strcmp(instruction.text, "move.w $0000(a2),$1516(a6)") == 0);
    EXPECT_TRUE(vf2_m68k_decode(handler_memory, sizeof(handler_memory), 10u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 4u);
    EXPECT_TRUE(strcmp(instruction.text, "subi.b #$f0,d3") == 0);
    EXPECT_TRUE(vf2_m68k_decode(handler_memory, sizeof(handler_memory), 14u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 4u);
    EXPECT_TRUE(strcmp(instruction.text, "move.l $38(a1,d0.w),a2") == 0);
    EXPECT_TRUE(vf2_m68k_decode(handler_memory, sizeof(handler_memory), 18u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 4u);
    EXPECT_TRUE(strcmp(instruction.text, "move.l a2,$0008(a4)") == 0);

    EXPECT_TRUE(vf2_m68k_decode(shifts, sizeof(shifts), 0u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(strcmp(instruction.text, "asl.l #2,d3") == 0);

    EXPECT_TRUE(vf2_m68k_decode(handler_arithmetic,
                                sizeof(handler_arithmetic), 0u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(strcmp(instruction.text, "or.w d0,d2") == 0);
    EXPECT_TRUE(vf2_m68k_decode(handler_arithmetic,
                                sizeof(handler_arithmetic), 2u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(strcmp(instruction.text, "neg.b d2") == 0);
    EXPECT_TRUE(vf2_m68k_decode(handler_arithmetic,
                                sizeof(handler_arithmetic), 4u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(strcmp(instruction.text, "subq.b #1,d2") == 0);
    EXPECT_TRUE(vf2_m68k_decode(handler_arithmetic,
                                sizeof(handler_arithmetic), 6u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 4u);
    EXPECT_TRUE(strcmp(instruction.text, "btst #0,d2") == 0);
    EXPECT_TRUE(vf2_m68k_decode(handler_arithmetic,
                                sizeof(handler_arithmetic), 10u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(strcmp(instruction.text, "addq.l #3,a2") == 0);
    EXPECT_TRUE(vf2_m68k_decode(handler_arithmetic,
                                sizeof(handler_arithmetic), 12u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(strcmp(instruction.text, "cmp.l d2,d0") == 0);

    EXPECT_TRUE(vf2_m68k_decode(extension, sizeof(extension), 0u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(strcmp(instruction.text, "ext.w d3") == 0);

    EXPECT_TRUE(vf2_m68k_decode(jump_table, sizeof(jump_table), 0u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 4u);
    EXPECT_TRUE(strcmp(instruction.text, "lea $06(pc,d2.w),a1") == 0);

    EXPECT_TRUE(vf2_m68k_decode(voice_helpers, sizeof(voice_helpers), 0u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(strcmp(instruction.text, "addq.b #1,d0") == 0);
    EXPECT_TRUE(vf2_m68k_decode(voice_helpers, sizeof(voice_helpers), 2u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(strcmp(instruction.text, "mulu.w d2,d0") == 0);
    EXPECT_TRUE(vf2_m68k_decode(voice_helpers, sizeof(voice_helpers), 4u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(strcmp(instruction.text, "not.b d6") == 0);
    EXPECT_TRUE(vf2_m68k_decode(voice_helpers, sizeof(voice_helpers), 6u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 4u);
    EXPECT_TRUE(strcmp(instruction.text, "move.b $4a(pc,d0.w),d1") == 0);
    EXPECT_TRUE(vf2_m68k_decode(voice_helpers, sizeof(voice_helpers), 10u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(strcmp(instruction.text, "tst.b d2") == 0);
    EXPECT_TRUE(vf2_m68k_decode(voice_helpers, sizeof(voice_helpers), 12u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 6u);
    EXPECT_TRUE(strcmp(instruction.text, "addi.l #$1ff,d6") == 0);
    EXPECT_TRUE(vf2_m68k_decode(voice_helpers, sizeof(voice_helpers), 18u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(strcmp(instruction.text, "sub.b d2,d3") == 0);
    EXPECT_TRUE(vf2_m68k_decode(voice_helpers, sizeof(voice_helpers), 20u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(strcmp(instruction.text, "movea.l a5,a0") == 0);
    EXPECT_TRUE(vf2_m68k_decode(voice_helpers, sizeof(voice_helpers), 22u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 4u);
    EXPECT_TRUE(strcmp(instruction.text, "move.l a3,$00(a2,d0.w)") == 0);
    EXPECT_TRUE(vf2_m68k_decode(voice_helpers, sizeof(voice_helpers), 26u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 6u);
    EXPECT_TRUE(strcmp(instruction.text, "cmpa.l #$6ffff,a3") == 0);
    EXPECT_TRUE(vf2_m68k_decode(stream_helpers, sizeof(stream_helpers), 0u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 4u);
    EXPECT_TRUE(strcmp(instruction.text, "subq.w #1,$0002(a2)") == 0);
    EXPECT_TRUE(vf2_m68k_decode(stream_helpers, sizeof(stream_helpers), 4u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 6u);
    EXPECT_TRUE(strcmp(instruction.text, "cmpi.l #$ffffffff,d2") == 0);
    EXPECT_TRUE(vf2_m68k_decode(stream_helpers, sizeof(stream_helpers), 10u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 4u);
    EXPECT_TRUE(strcmp(instruction.text, "tst.b $000c(a2)") == 0);

    EXPECT_TRUE(vf2_m68k_decode(indexed_quick, sizeof(indexed_quick), 0u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 6u);
    EXPECT_TRUE(strcmp(instruction.text,
                       "addi.b #$50,$0b(a1,d1.w)") == 0);
    EXPECT_TRUE(vf2_m68k_decode(indexed_quick, sizeof(indexed_quick), 6u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 4u);
    EXPECT_TRUE(strcmp(instruction.text,
                       "addq.b #3,$0b(a1,d1.w)") == 0);

    EXPECT_TRUE(vf2_m68k_decode(address_immediate, sizeof(address_immediate),
                                0u, &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 6u);
    EXPECT_TRUE(strcmp(instruction.text, "suba.l #$20,a4") == 0);

    return failures == 0 ? 0 : 1;
}
