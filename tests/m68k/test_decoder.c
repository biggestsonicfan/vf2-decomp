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
        0xd0, 0x28, 0x00, 0x04,             /* add.b $4(a0),d0 */
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
    static const uint8_t ori_eori[] = {
        0x00, 0x07, 0x00, 0x10,             /* ori.b #$10,d7 (ROM 0x49e) */
        0x00, 0x3c, 0x00, 0x10,             /* ori.b #$10,ccr */
        0x0a, 0x00, 0x00, 0xff,             /* eori.b #$ff,d0 */
        0x00, 0x35, 0x00, 0x0f, 0x10, 0x01, /* ori.b #$f,$1(a5,d1.w) */
        0x00, 0x20, 0x00, 0xff              /* ori.b #$ff,-(a0) */
    };
    static const uint8_t move_data_indexed[] = {
        0x1d, 0x41, 0x15, 0x0d,             /* move.b d1,$150e(a6) */
        0x27, 0x85, 0x40, 0x0a,             /* move.l d5,$a(a3,d4.w) */
        0x2c, 0x0a                          /* move.l a2,d6 (ROM 0xc86) */
    };
    static const uint8_t imm_indexed[] = {
        0x02, 0x72, 0x00, 0xff, 0x30, 0x07, /* andi.w #$ff,$7(a2,d3.w) */
        0x04, 0x35, 0x00, 0x0f, 0x10, 0x01, /* subi.b #$f,$1(a5,d1.w) */
        0x0c, 0xb2, 0x00, 0x00, 0x00, 0x09, /* cmpi.l #$9,$4(a2,d2.w) */
        0x20, 0x04,
        0x02, 0x5a, 0x00, 0xff,             /* andi.w #$ff,(a2)+ */
        0x02, 0x78, 0x00, 0xff, 0x12, 0x34, /* andi.w #$ff,$1234 */
        0x04, 0x25, 0x00, 0x0f,             /* subi.b #$f,-(a5) */
        0x04, 0x3b, 0x00, 0x0f, 0x10, 0x01, /* subi.b #$f,$1(pc,d1.w) */
        0x06, 0x1d, 0x00, 0x50,             /* addi.b #$50,(a5)+ */
        0x06, 0x3a, 0x00, 0x50, 0x00, 0x10, /* addi.b #$50,$10(pc) */
        0x0c, 0x1b, 0x00, 0xf7,             /* cmpi.b #$f7,(a3)+ (ROM) */
        0x0c, 0x25, 0x00, 0x09,             /* cmpi.b #$9,-(a5) */
        0x0c, 0x39, 0x00, 0x09, 0x00, 0x12, /* cmpi.b #$9,$00120034 */
        0x00, 0x34,
        0x00, 0x38, 0x00, 0xff, 0x12, 0x34  /* ori.b #$ff,$1234 */
    };
    static const uint8_t indexed_quick[] = {
        0x06, 0x31, 0x00, 0x50, 0x10, 0x0b, /* addi.b #$50,$b(a1,d1.w) */
        0x56, 0x31, 0x10, 0x0b              /* addq.b #3,$b(a1,d1.w) */
    };
    static const uint8_t address_immediate[] = {
        0x99, 0xfc, 0x00, 0x00, 0x00, 0x20  /* suba.l #$20,a4 */
    };
    static const uint8_t move_indexed_immediate[] = {
        0x1b, 0xbc, 0x00, 0x1f, 0x40, 0x0b, /* move.b #$1f,$b(a5,d4.w) */
        0x1b, 0xbc, 0x00, 0x10, 0x40, 0x00, /* move.b #$10,$0(a5,d4.w) */
        0x1b, 0xbc, 0x00, 0x1f, 0x10, 0x0b  /* move.b #$1f,$b(a5,d1.w) */
    };

    EXPECT_TRUE(vf2_m68k_decode(program, sizeof(program), 0u, &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 2u);
    EXPECT_TRUE(strcmp(instruction.text, "moveq #-2,d0") == 0);
    EXPECT_TRUE(vf2_m68k_decode(program, sizeof(program), 2u, &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 2u);
    EXPECT_TRUE(strcmp(instruction.text, "bra $00000006") == 0);
    EXPECT_TRUE(vf2_m68k_decode(program, sizeof(program), 6u, &instruction) == VF2_OK);
    /* jsr (d16,PC): the base is the address of the ext word (opcode
       address + 2), so -8 from 6 lands at 0 (see test_pcrel_base.c for
       the ROM measurement behind this). */
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
    EXPECT_TRUE(strcmp(instruction.text, "add.b $0004(a0),d0") == 0);
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

    EXPECT_TRUE(vf2_m68k_decode(ori_eori, sizeof(ori_eori), 0u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 4u);
    EXPECT_TRUE(strcmp(instruction.text, "ori.b #$10,d7") == 0);
    EXPECT_TRUE(vf2_m68k_decode(ori_eori, sizeof(ori_eori), 4u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 4u);
    EXPECT_TRUE(strcmp(instruction.text, "ori.b #$10,ccr") == 0);
    EXPECT_TRUE(vf2_m68k_decode(ori_eori, sizeof(ori_eori), 8u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 4u);
    EXPECT_TRUE(strcmp(instruction.text, "eori.b #$ff,d0") == 0);
    EXPECT_TRUE(vf2_m68k_decode(ori_eori, sizeof(ori_eori), 12u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 6u);
    EXPECT_TRUE(strcmp(instruction.text,
                       "ori.b #$f,$01(a5,d1.w)") == 0);
    EXPECT_TRUE(vf2_m68k_decode(ori_eori, sizeof(ori_eori), 18u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 4u);
    EXPECT_TRUE(strcmp(instruction.text, "ori.b #$ff,-(a0)") == 0);
    EXPECT_TRUE(vf2_m68k_decode(move_data_indexed,
                                sizeof(move_data_indexed), 0u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 4u);
    EXPECT_TRUE(strcmp(instruction.text,
                       "move.b d1,$150d(a6)") == 0);
    EXPECT_TRUE(vf2_m68k_decode(move_data_indexed,
                                sizeof(move_data_indexed), 4u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 4u);
    EXPECT_TRUE(strcmp(instruction.text,
                       "move.l d5,$0a(a3,d4.w)") == 0);
    EXPECT_TRUE(vf2_m68k_decode(move_data_indexed,
                                sizeof(move_data_indexed), 8u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 2u);
    EXPECT_TRUE(strcmp(instruction.text, "move.l a2,d6") == 0);
    EXPECT_TRUE(vf2_m68k_decode(imm_indexed, sizeof(imm_indexed), 0u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 6u);
    EXPECT_TRUE(strcmp(instruction.text,
                       "andi.w #$ff,$07(a2,d3.w)") == 0);
    EXPECT_TRUE(vf2_m68k_decode(imm_indexed, sizeof(imm_indexed), 6u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 6u);
    EXPECT_TRUE(strcmp(instruction.text,
                       "subi.b #$f,$01(a5,d1.w)") == 0);
    EXPECT_TRUE(vf2_m68k_decode(imm_indexed, sizeof(imm_indexed), 12u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 8u);
    EXPECT_TRUE(strcmp(instruction.text,
                       "cmpi.l #$9,$04(a2,d2.w)") == 0);
    EXPECT_TRUE(vf2_m68k_decode(imm_indexed, sizeof(imm_indexed), 20u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 4u);
    EXPECT_TRUE(strcmp(instruction.text, "andi.w #$ff,(a2)+") == 0);
    EXPECT_TRUE(vf2_m68k_decode(imm_indexed, sizeof(imm_indexed), 24u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 6u);
    EXPECT_TRUE(strcmp(instruction.text, "andi.w #$ff,$1234") == 0);
    EXPECT_TRUE(vf2_m68k_decode(imm_indexed, sizeof(imm_indexed), 30u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 4u);
    EXPECT_TRUE(strcmp(instruction.text, "subi.b #$f,-(a5)") == 0);
    EXPECT_TRUE(vf2_m68k_decode(imm_indexed, sizeof(imm_indexed), 34u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 6u);
    EXPECT_TRUE(strcmp(instruction.text,
                       "subi.b #$f,$01(pc,d1.w)") == 0);
    EXPECT_TRUE(vf2_m68k_decode(imm_indexed, sizeof(imm_indexed), 40u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 4u);
    EXPECT_TRUE(strcmp(instruction.text, "addi.b #$50,(a5)+") == 0);
    EXPECT_TRUE(vf2_m68k_decode(imm_indexed, sizeof(imm_indexed), 44u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 6u);
    EXPECT_TRUE(strcmp(instruction.text, "addi.b #$50,$0010(pc)") == 0);
    EXPECT_TRUE(vf2_m68k_decode(imm_indexed, sizeof(imm_indexed), 50u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 4u);
    EXPECT_TRUE(strcmp(instruction.text, "cmpi.b #$f7,(a3)+") == 0);
    EXPECT_TRUE(vf2_m68k_decode(imm_indexed, sizeof(imm_indexed), 54u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 4u);
    EXPECT_TRUE(strcmp(instruction.text, "cmpi.b #$9,-(a5)") == 0);
    EXPECT_TRUE(vf2_m68k_decode(imm_indexed, sizeof(imm_indexed), 58u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 8u);
    EXPECT_TRUE(strcmp(instruction.text, "cmpi.b #$9,$00120034") == 0);
    EXPECT_TRUE(vf2_m68k_decode(imm_indexed, sizeof(imm_indexed), 66u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 6u);
    EXPECT_TRUE(strcmp(instruction.text, "ori.b #$ff,$1234") == 0);
    static const uint8_t move_src_abs[] = {
        0x16, 0x20,                         /* move.b -(a0),d3 (ROM 0x854) */
        0x38, 0x38, 0x12, 0x34,             /* move.w $1234,d4 */
        0x26, 0x39, 0x00, 0x12, 0x00, 0x34, /* move.l $00120034,d3 */
        0x12, 0x3a, 0x00, 0x10              /* move.b $10(pc),d1 */
    };
    EXPECT_TRUE(vf2_m68k_decode(move_src_abs, sizeof(move_src_abs), 0u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 2u);
    EXPECT_TRUE(strcmp(instruction.text, "move.b -(a0),d3") == 0);
    EXPECT_TRUE(vf2_m68k_decode(move_src_abs, sizeof(move_src_abs), 2u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 4u);
    EXPECT_TRUE(strcmp(instruction.text, "move.w $1234,d4") == 0);
    EXPECT_TRUE(vf2_m68k_decode(move_src_abs, sizeof(move_src_abs), 6u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 6u);
    EXPECT_TRUE(strcmp(instruction.text, "move.l $00120034,d3") == 0);
    EXPECT_TRUE(vf2_m68k_decode(move_src_abs, sizeof(move_src_abs), 12u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 4u);
    EXPECT_TRUE(strcmp(instruction.text, "move.b $0010(pc),d1") == 0);
    /* Byte immediates use the extension word's low byte; ROM carries
       nonzero high bytes (0x25f8 ori.b #$e8,(a4), 0x265e ori.b
       #$8,(a6)). Every handler with a byte #imm form is covered. */
    static const uint8_t imm_byte_high[] = {
        0x00, 0x14, 0x08, 0xe8,                   /* ori.b #$e8,(a4) */
        0x0a, 0x14, 0x08, 0xe8,                   /* eori.b #$e8,(a4) */
        0x02, 0x14, 0x08, 0xe8,                   /* andi.b #$e8,(a4) */
        0x04, 0x14, 0x08, 0xe8,                   /* subi.b #$e8,(a4) */
        0x06, 0x14, 0x08, 0xe8,                   /* addi.b #$e8,(a4) */
        0x0c, 0x14, 0x08, 0xe8,                   /* cmpi.b #$e8,(a4) */
        0x18, 0x3c, 0x08, 0xe8,                   /* move.b #$e8,d4 */
        0x18, 0xbc, 0x08, 0xe8,                   /* move.b #$e8,(a4) */
        0x13, 0xfc, 0x08, 0xe8, 0x00, 0x00, 0x12, 0x34, /* move.b #$e8,abs */
        0x00, 0x16, 0x04, 0x08                    /* ori.b #$8,(a6) ROM 0x265e */
    };
    EXPECT_TRUE(vf2_m68k_decode(imm_byte_high, sizeof(imm_byte_high), 0u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 4u);
    EXPECT_TRUE(strcmp(instruction.text, "ori.b #$e8,(a4)") == 0);
    EXPECT_TRUE(vf2_m68k_decode(imm_byte_high, sizeof(imm_byte_high), 4u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 4u);
    EXPECT_TRUE(strcmp(instruction.text, "eori.b #$e8,(a4)") == 0);
    EXPECT_TRUE(vf2_m68k_decode(imm_byte_high, sizeof(imm_byte_high), 8u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 4u);
    EXPECT_TRUE(strcmp(instruction.text, "andi.b #$e8,(a4)") == 0);
    EXPECT_TRUE(vf2_m68k_decode(imm_byte_high, sizeof(imm_byte_high), 12u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 4u);
    EXPECT_TRUE(strcmp(instruction.text, "subi.b #$e8,(a4)") == 0);
    EXPECT_TRUE(vf2_m68k_decode(imm_byte_high, sizeof(imm_byte_high), 16u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 4u);
    EXPECT_TRUE(strcmp(instruction.text, "addi.b #$e8,(a4)") == 0);
    EXPECT_TRUE(vf2_m68k_decode(imm_byte_high, sizeof(imm_byte_high), 20u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 4u);
    EXPECT_TRUE(strcmp(instruction.text, "cmpi.b #$e8,(a4)") == 0);
    EXPECT_TRUE(vf2_m68k_decode(imm_byte_high, sizeof(imm_byte_high), 24u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 4u);
    EXPECT_TRUE(strcmp(instruction.text, "move.b #$e8,d4") == 0);
    EXPECT_TRUE(vf2_m68k_decode(imm_byte_high, sizeof(imm_byte_high), 28u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 4u);
    EXPECT_TRUE(strcmp(instruction.text, "move.b #$e8,(a4)") == 0);
    EXPECT_TRUE(vf2_m68k_decode(imm_byte_high, sizeof(imm_byte_high), 32u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 8u);
    EXPECT_TRUE(strcmp(instruction.text, "move.b #$e8,$00001234") == 0);
    EXPECT_TRUE(vf2_m68k_decode(imm_byte_high, sizeof(imm_byte_high), 40u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 4u);
    EXPECT_TRUE(strcmp(instruction.text, "ori.b #$8,(a6)") == 0);

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

    EXPECT_TRUE(vf2_m68k_decode(move_indexed_immediate,
                                sizeof(move_indexed_immediate), 0u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 6u);
    EXPECT_TRUE(strcmp(instruction.text,
                       "move.b #$1f,$0b(a5,d4.w)") == 0);
    EXPECT_TRUE(vf2_m68k_decode(move_indexed_immediate,
                                sizeof(move_indexed_immediate), 6u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 6u);
    EXPECT_TRUE(strcmp(instruction.text,
                       "move.b #$10,$00(a5,d4.w)") == 0);
    EXPECT_TRUE(vf2_m68k_decode(move_indexed_immediate,
                                sizeof(move_indexed_immediate), 12u,
                                &instruction) == VF2_OK);
    EXPECT_TRUE(instruction.length == 6u);
    EXPECT_TRUE(strcmp(instruction.text,
                       "move.b #$1f,$0b(a5,d1.w)") == 0);

    return failures == 0 ? 0 : 1;
}
