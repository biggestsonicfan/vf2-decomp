#include "vf2/sound_board.h"

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

/* ROM-measured boot image (v0794a): the structural tables consumed by
   boot inits 0x28e/0x320/0x336/0x3a8/0x3cc/0x402/0x422. All payload
   regions are zero; the nonzero bytes are init-structural (voice slot
   words, stream records, table pointers, the d1 < 0 upload skip). */
static void test_boot_rom(uint8_t *rom)
{
    size_t voice = 0u;

    memset(rom, 0, 0x10010u);
    /* Historical fixture bytes (vector area, init doesn't read them). */
    rom[0x0000u] = 0x12u; rom[0x0001u] = 0x34u;
    /* 0x28e wave-blob source -> 0x607000 (0x5a probe + zero tail). */
    rom[0x4940u] = 0x00u; rom[0x4941u] = 0x60u;
    rom[0x4942u] = 0x70u; rom[0x4943u] = 0x00u;
    rom[0x7000u] = 0x5au;
    /* 0x336 descriptor images (zero) at 0x607100/0x607200. */
    rom[0x3066u] = 0x00u; rom[0x3067u] = 0x60u;
    rom[0x3068u] = 0x71u; rom[0x3069u] = 0x00u;
    rom[0x30a6u] = 0x00u; rom[0x30a7u] = 0x60u;
    rom[0x30a8u] = 0x72u; rom[0x30a9u] = 0x00u;
    /* 0x3a8 voice slot words (index*0x20, big-endian). */
    for (voice = 0u; voice < 32u; ++voice) {
        rom[0x6e6u + voice * 2u] = (uint8_t)(((uint16_t)voice * 0x20u) >> 8u);
        rom[0x6e6u + voice * 2u + 1u] =
            (uint8_t)(((uint16_t)voice * 0x20u) & 0xffu);
    }
    /* 0x402 volume table pointer -> 0x607300 (16 zero levels). */
    rom[0x536au] = 0x00u; rom[0x536bu] = 0x60u;
    rom[0x536cu] = 0x73u; rom[0x536du] = 0x00u;
    rom[0x7300u] = 0x5au;
    /* 0x422: no first-copy bytes, d1 < 0 skips the upload. */
    rom[0x8df0u] = 0xffu; rom[0x8df1u] = 0xffu;
    rom[0x8df2u] = 0xffu; rom[0x8df3u] = 0xffu;
}

int main(void)
{
    static uint8_t audio_rom[0x10010u];
    static const uint8_t sample_rom[] = {0xa5u, 0x5au};
    vf2_sound_board board;
    int16_t left[1] = {0};
    int16_t right[1] = {0};
    uint16_t value = 0u;
    uint8_t byte = 0u;
    vf2_sound_voice_maintenance_report maintenance;
    vf2_sound_stream_maintenance_report stream_maintenance;
    uint8_t *voices;

    test_boot_rom(audio_rom);
    EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                               sample_rom, sizeof(sample_rom)) == VF2_OK);
    EXPECT_TRUE(vf2_sound_board_write_u16(&board, 0x0000u, 0x1234u) == VF2_OK);
    EXPECT_TRUE(vf2_sound_board_read_u16(&board, 0x0000u, &value) == VF2_OK);
    EXPECT_TRUE(value == 0x1234u);
    EXPECT_TRUE(vf2_sound_board_write_u8(&board, 0x10040bu, 0x1fu) == VF2_OK);
    EXPECT_TRUE(vf2_sound_board_read_u8(&board, 0x10040bu, &byte) == VF2_OK);
    EXPECT_TRUE(byte == 0x1fu);
    EXPECT_TRUE(vf2_sound_board_write_u16(&board, 0x400000u, 0x0020u) == VF2_OK);
    EXPECT_TRUE(vf2_sound_board_read_u16(&board, 0x400000u, &value) == VF2_OK);
    EXPECT_TRUE(value == 0x0020u);
    EXPECT_TRUE(vf2_sound_board_read_u16(&board, 0x600000u, &value) == VF2_OK);
    EXPECT_TRUE(value == 0x1234u);
    EXPECT_TRUE(vf2_sound_board_read_u16(&board, 0x800000u, &value) == VF2_OK);
    EXPECT_TRUE(value == 0xa55au);
    EXPECT_TRUE(vf2_scsp_read_sample(&board.scsp, 0u, &byte, 1u) == VF2_OK);
    EXPECT_TRUE(byte == 0xa5u);
    EXPECT_TRUE(vf2_sound_board_render(&board, left, right, 1u) == VF2_OK);
    EXPECT_TRUE(vf2_sound_board_write_u8(&board, 0x600000u, 0u) ==
                VF2_ERROR_OUT_OF_BOUNDS);

    voices = board.sound_ram + VF2_SOUND_VOICE_TABLE_BASE;
    /* Normal expiration: age field +9 and shared counter 0x151e. */
    voices[0] = 0u;
    voices[1] = 3u;
    voices[2] = 1u;
    voices[9] = 3u;
    voices[0x0b] = 1u;
    voices[VF2_SOUND_VOICE_BYTES + 9u] = 8u;
    /* Release expiration: alternate age field +5 and 0x152e. */
    voices[VF2_SOUND_VOICE_BYTES] = 0u;
    voices[VF2_SOUND_VOICE_BYTES + 1u] = 4u;
    voices[VF2_SOUND_VOICE_BYTES + 2u] = 9u;
    voices[VF2_SOUND_VOICE_BYTES + 5u] = 2u;
    voices[VF2_SOUND_VOICE_BYTES + 0x0bu] = 1u;
    voices[2u * VF2_SOUND_VOICE_BYTES + 5u] = 8u;
    voices[2u * VF2_SOUND_VOICE_BYTES + 9u] = 0xffu;
    board.sound_ram[VF2_SOUND_SHARED_COUNTER_151E] = 2u;
    board.sound_ram[VF2_SOUND_SHARED_COUNTER_152E] = 3u;
    /* Sentinels proving voice 1 (slot 0x0004) is never serviced under
       the frozen-pointer model: key_off for voice 0 touches only
       SCSP[0x000e] / ram[0x2003]. */
    board.sound_ram[0x2004u] = 0xaau;
    EXPECT_TRUE(vf2_scsp_write_u8(&board.scsp, 0x000fu, 0x5au) == VF2_OK);
    /* P0c silicon-equivalent contract for maintain_voices (v0799c +
   v0803): the adda.w #$20,a0 at the loop tail is jumped over by skip
   bodges (beq/bne -> mid-adda ori.b), so a0 never advances. ALL 32
   silicon iterations visit voice 0. The stepper traps on the inner
   rebalance at PC 0x1640 (`adda.l #imm32,a1` decode fail), so the
   C refuses before that rebalance, mirroring silicon's partial
   state. This means a single `maintain_voices` call processes
   exactly ONE expiration (voice[0] in this fixture - which is
   pre-set as the active normal-expiration voice), then returns
   VF2_ERROR_UNSUPPORTED. Voices 1+ are NEVER serviced; their
   state is unchanged. The OLD per-iteration indexing + inner-
   rebalance decrement model that this test was pinned to is
   documented as the pre-fix recovery shape but does NOT match
   silicon (it would require the silicon to behave like the C's
   frozen-pointer view + the rebalance to actually fire, which
   the stepper proves it does not). The triangle differential
   gates the silicon-correct shape (`s1-clean`, `s3-skip`,
   `s5-release`); this unit test is now pinned to the same
   contract. */
    EXPECT_TRUE(vf2_sound_board_maintain_voices(&board, &maintenance) ==
                VF2_ERROR_UNSUPPORTED);
    /* Iter 0 only: voice[0] expired (the frozen-pointer model). */
    EXPECT_TRUE(maintenance.voices_scanned == 1u);
    EXPECT_TRUE(maintenance.voices_expired == 1u);
    EXPECT_TRUE(maintenance.normal_expirations == 1u);
    /* Release path (voice[1] in this fixture) is never reached -
       the silicon would visit it on a later call, not this one. */
    EXPECT_TRUE(maintenance.release_expirations == 0u);
    /* No inner rebalance in the silicon-equivalent model (the
       rebalance at PC 0x67C traps on the stepper at 0x1640). */
    EXPECT_TRUE(maintenance.voices_aged == 0u);
    /* Counter 151e (normal expiration): decremented exactly once. */
    EXPECT_TRUE(maintenance.counter_151e_before == 2u);
    EXPECT_TRUE(maintenance.counter_151e_after == 1u);
    /* Counter 152e (release path): unchanged - release never
       fired under the frozen-pointer single-iteration model. */
    EXPECT_TRUE(maintenance.counter_152e_before == 3u);
    EXPECT_TRUE(maintenance.counter_152e_after == 3u);
    /* Voice 0 zeroed + key_off slot-side effects still commit. */
    EXPECT_TRUE(voices[2u] == 0u);
    EXPECT_TRUE(voices[9u] == 0u);
    /* Voice 1 is NEVER reached in this call (the frozen pointer
       never advances off voice 0; its state is byte-for-byte
       unchanged from the fixture set above). */
    EXPECT_TRUE(voices[VF2_SOUND_VOICE_BYTES + 2u] == 9u);
    EXPECT_TRUE(voices[VF2_SOUND_VOICE_BYTES + 9u] == 8u);
    EXPECT_TRUE(voices[VF2_SOUND_VOICE_BYTES + 5u] == 2u);
    EXPECT_TRUE(voices[VF2_SOUND_VOICE_BYTES + 2u * 0u + 9u] == 8u);
    EXPECT_TRUE(voices[2u * VF2_SOUND_VOICE_BYTES + 9u] == 0xffu);
    /* ROM V1 key: SCSP[slotword+0x0b] = 0x1f, ram[0x2000+slotword] = 0
       (slot words 0x0003/0x0004 above). No control-bit RMW exists. */
    EXPECT_TRUE(vf2_scsp_read_u8(&board.scsp, 0x000eu, &byte) == VF2_OK);
    EXPECT_TRUE(byte == 0x1fu);
    /* Voice 1 never reached in this call (frozen pointer never leaves
       voice 0): slot-0x0004 sentinels unchanged. */
    EXPECT_TRUE(vf2_scsp_read_u8(&board.scsp, 0x000fu, &byte) == VF2_OK);
    EXPECT_TRUE(byte == 0x5au);
    EXPECT_TRUE(board.sound_ram[0x2003u] == 0u);
    EXPECT_TRUE(board.sound_ram[0x2004u] == 0xaau);
    EXPECT_TRUE(vf2_scsp_read_u16(&board.scsp, 0x041au, &value) == VF2_OK);
    EXPECT_TRUE(value == 0x03c0u);
    EXPECT_TRUE(vf2_scsp_read_u16(&board.scsp, 0x0422u, &value) == VF2_OK);
    EXPECT_TRUE(value == 0x0080u);

    EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                               sample_rom, sizeof(sample_rom)) == VF2_OK);
    {
        uint8_t *descriptor = board.sound_ram +
            VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        uint8_t *voice = board.sound_ram + VF2_SOUND_VOICE_TABLE_BASE;
        vf2_sound_dispatch_report dispatch;

        descriptor[0] = 0x80u;
        descriptor[1] = 0u;
        descriptor[2] = 0x01u;
        voice[0] = 0u;
        voice[1] = 3u;
        voice[2] = 0x40u;
        /* Byte match (v0819): ROM 0x1e1c is cmp.B on voice[4]. */
        voice[4] = 1u;
        voice[5] = 1u;
        board.sound_ram[VF2_SOUND_SHARED_COUNTER_151E] = 1u;
        board.sound_ram[0x2000u] = 0xaau;
        EXPECT_TRUE(vf2_sound_board_write_u8(
            &board, VF2_SCSP_CPU_BASE + 0u, 0xffu
        ) == VF2_OK);
        {
            const uint8_t packet[4] = {0xc1u, 0x70u, 0u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        board.sound_ram[VF2_SOUND_COMMAND_CURSOR] = 0u;
        board.sound_ram[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) == VF2_OK);
        EXPECT_TRUE(dispatch.command == 0xc1u);
        EXPECT_TRUE(dispatch.payload == 0x70u);
        EXPECT_TRUE(dispatch.descriptors_scanned == 20u);
        EXPECT_TRUE(dispatch.descriptors_matched == 1u);
        EXPECT_TRUE(dispatch.voices_cleared == 1u);
        EXPECT_TRUE(dispatch.handler_address == 0x1e5cu);
        EXPECT_TRUE(dispatch.handled != 0);
        EXPECT_TRUE(descriptor[2] == 0xc1u);
        EXPECT_TRUE(voice[2] == 0u && voice[4] == 0u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_SHARED_COUNTER_151E] == 0u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);
        EXPECT_TRUE(dispatch.cursor_after == 4u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_WRITE_CURSOR + 1u] == 4u);
        EXPECT_TRUE(vf2_scsp_read_u16(&board.scsp, 0u, &value) == VF2_OK);
        EXPECT_TRUE(value == 0xff00u);
        /* ROM V5 key (d4 = voice[0] = 0x0003): SCSP[0x000e] = 0x1f,
           SCSP[0x0003] = 0x10, ram[0x2003] = 0. ram[0x2000] keeps its
           0xaa (untouched); SCSP[0] keeps the pre-write 0xff. */
        EXPECT_TRUE(vf2_scsp_read_u8(&board.scsp, 0x0eu, &byte) == VF2_OK);
        EXPECT_TRUE(byte == 0x1fu);
        EXPECT_TRUE(vf2_scsp_read_u8(&board.scsp, 0x0003u, &byte) == VF2_OK);
        EXPECT_TRUE(byte == 0x10u);
        EXPECT_TRUE(board.sound_ram[0x2003u] == 0u);
        EXPECT_TRUE(board.sound_ram[0x2000u] == 0xaau);

        /* 90-class with a zero second byte traps at 0x866 (continues
           into the 80 body mid-instruction): base is stored, mode is not
           read, and the packet stays queued. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        audio_rom[0xc6a6u] = 0x77u;
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 0u;
        descriptor[2] = 0u;
        descriptor[4] = 0x10u;
        {
            const uint8_t packet[4] = {0x90u, 0u, 0u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) ==
                    VF2_ERROR_UNSUPPORTED);
        EXPECT_TRUE(dispatch.handler_address == 0x85au);
        EXPECT_TRUE(dispatch.channel == 0u);
        EXPECT_TRUE(dispatch.descriptors_matched == 1u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_CURRENT] == 0xa0u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_MODE] == 0u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 1u);
        EXPECT_TRUE(dispatch.cursor_after == dispatch.cursor_before);

        /* 90-class prefix (v0794b): d2 = command, second = payload. Base,
           mode, then refuse at the low/high split (low is a stale
           branch, high cannot match the drain). */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        audio_rom[0xc6a6u + 5u] = 0x44u;
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 0u;
        descriptor[2] = 3u;
        descriptor[4] = 0x10u;
        {
            const uint8_t packet[4] = {0x93u, 0x05u, 0x02u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) ==
                    VF2_ERROR_UNSUPPORTED);
        EXPECT_TRUE(dispatch.handler_address == 0x85au);
        EXPECT_TRUE(dispatch.channel == 3u);
        EXPECT_TRUE(dispatch.descriptors_matched == 1u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_CURRENT] == 0xa3u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_MODE] == 0x44u);
        EXPECT_TRUE(dispatch.selected_value == 0x44u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 1u);
        EXPECT_TRUE(dispatch.cursor_after == dispatch.cursor_before);

        /* 90-class mode with a nonzero middle byte: the long index
           ((base>>8)<<16)+second is exact (d3.mid = 0 proven). Needs a
           bigger image: mid 1 reads file 0x1c6a6+. */
        {
            static uint8_t big_rom[0x20000u];
            memset(big_rom, 0, sizeof(big_rom));
            test_boot_rom(big_rom);
            big_rom[0xc6a6u + 0x10000u + 5u] = 0x2au;
            EXPECT_TRUE(vf2_sound_board_initialize(&board, big_rom, sizeof(big_rom),
                                       sample_rom, sizeof(sample_rom)) == VF2_OK);
            descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
            descriptor[0] = 0x80u;
            descriptor[1] = 0u;
            descriptor[2] = 15u;
            descriptor[4] = 0x80u;
            {
                const uint8_t packet[4] = {0x9fu, 0x05u, 0x02u, 0u};
                EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
            }
            EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) ==
                        VF2_ERROR_UNSUPPORTED);
            EXPECT_TRUE(dispatch.handler_address == 0x85au);
            EXPECT_TRUE(dispatch.channel == 15u);
            EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_CURRENT] == 0x1fu);
            EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_MODE] == 0x2au);
            EXPECT_TRUE(dispatch.selected_value == 0x2au);
            EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 1u);
        }

        /* Default class via the 0x858 primer: d2 = payload, second =
           ring byte 2, handler 0x858. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        audio_rom[0xc6a6u + 9u] = 0x33u;
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 0u;
        descriptor[2] = 3u;
        descriptor[4] = 0x10u;
        {
            const uint8_t packet[4] = {0xd3u, 0x07u, 0x09u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) ==
                    VF2_ERROR_UNSUPPORTED);
        EXPECT_TRUE(dispatch.handler_address == 0x858u);
        EXPECT_TRUE(dispatch.command_class == 0xd0u);
        EXPECT_TRUE(dispatch.channel == 3u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_CURRENT] == 0x17u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_MODE] == 0x33u);
        EXPECT_TRUE(dispatch.selected_value == 0x33u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 1u);
        EXPECT_TRUE(dispatch.cursor_after == dispatch.cursor_before);

        /* Default class with a zero second byte: base stored, mode
           untouched, refuse. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        audio_rom[0xc6a6u] = 0x77u;
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 0u;
        descriptor[2] = 3u;
        descriptor[4] = 0x10u;
        {
            const uint8_t packet[4] = {0xd3u, 0x07u, 0u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) ==
                    VF2_ERROR_UNSUPPORTED);
        EXPECT_TRUE(dispatch.handler_address == 0x858u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_CURRENT] == 0x17u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_MODE] == 0u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 1u);

        /* A0 refuses at entry (v0795b): the body splits on cmp.l
           d2,d0 with d2 never loaded, so stale-vs-primary mis-splits by
           history and the stale arm stores through residue-a2. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 0u;
        descriptor[2] = 0u;
        {
            const uint8_t packet[4] = {0xa0u, 0x01u, 0x01u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) ==
                    VF2_ERROR_UNSUPPORTED);
        EXPECT_TRUE(dispatch.handler_address == 0x1e74u);
        EXPECT_TRUE(dispatch.channel == 0u);
        EXPECT_TRUE(dispatch.descriptors_matched == 1u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 1u);
        EXPECT_TRUE(dispatch.cursor_after == dispatch.cursor_before);

        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 0u;
        descriptor[2] = 5u;
        {
            const uint8_t packet[4] = {0xa5u, 0x02u, 0x03u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) ==
                    VF2_ERROR_UNSUPPORTED);
        EXPECT_TRUE(dispatch.handler_address == 0x1e74u);
        EXPECT_TRUE(dispatch.channel == 5u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 1u);
        EXPECT_TRUE(dispatch.cursor_after == dispatch.cursor_before);

        /* Stream maintenance is a documented no-op (v0796): the ROM
           sequence engine (driver/worker/appliers) has no static entry
           and every slot aborts on its zero +0x0c byte, so nothing on
           ROM sequences per frame. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        EXPECT_TRUE(vf2_sound_board_maintain_streams(
            &board, &stream_maintenance
        ) == VF2_OK);
        EXPECT_TRUE(stream_maintenance.descriptors_scanned == 0u);
        EXPECT_TRUE(stream_maintenance.timers_expired == 0u);
        EXPECT_TRUE(stream_maintenance.packets_emitted == 0u);
        EXPECT_TRUE(stream_maintenance.escape_records == 0u);
        EXPECT_TRUE(stream_maintenance.pointer_handoffs == 0u);
        EXPECT_TRUE(stream_maintenance.bytes_consumed == 0u);
        EXPECT_TRUE(stream_maintenance.write_cursor_before ==
                    stream_maintenance.write_cursor_after);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);

        /* Even an armed stream descriptor is untouched: no timers decay,
           no packets emit, cursors hold. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        descriptor = board.sound_ram + VF2_SOUND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 0xc0u;
        descriptor[2] = 0u;
        descriptor[3] = 1u;
        descriptor[4] = 0u;
        descriptor[5] = 0x60u;
        descriptor[6] = 0x81u;
        descriptor[7] = 0u;
        EXPECT_TRUE(vf2_sound_board_maintain_streams(
            &board, &stream_maintenance
        ) == VF2_OK);
        EXPECT_TRUE(stream_maintenance.packets_emitted == 0u);
        EXPECT_TRUE(stream_maintenance.timers_expired == 0u);
        EXPECT_TRUE(descriptor[2] == 0u && descriptor[3] == 1u);
        EXPECT_TRUE(descriptor[4] == 0u && descriptor[5] == 0x60u &&
                    descriptor[6] == 0x81u && descriptor[7] == 0u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);
        EXPECT_TRUE(stream_maintenance.write_cursor_before ==
                    stream_maintenance.write_cursor_after);

        /* The instruction-aligned e0 handler completes its table lookup and
           channel result write when no live voice needs the wider SCSP tail. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        audio_rom[0x2b26u] = 0x00u;
        audio_rom[0x2b27u] = 0x60u;
        audio_rom[0x2b28u] = 0x10u;
        audio_rom[0x2b29u] = 0x00u;
        /* Bit-8 rule: transformed 3 has bit8 clear, so the index
           negates (3 -> 0xfd). The marker lives at the NEGATED
           index: a bit-0-rule model would read +3 instead. */
        audio_rom[0x1000u + 0xfdu] = 0x5au;
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 0u;
        descriptor[2] = 0x09u;
        descriptor[7] = 0u;
        {
            /* A matching channel-9 voice must NOT trigger the tail: the
               ROM exits via rts at 0x13c8 (e0 gate) instead. */
            uint8_t *nine_voice = board.sound_ram + VF2_SOUND_VOICE_TABLE_BASE;
            nine_voice[2] = 0x01u;
            nine_voice[4] = 0u;
            nine_voice[5] = 9u;
        }
        {
            const uint8_t packet[4] = {0xe9u, 0u, 0u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) == VF2_OK);
        EXPECT_TRUE(dispatch.handler_address == 0x12b6u);
        EXPECT_TRUE(dispatch.channel == 9u);
        EXPECT_TRUE(dispatch.voices_matched == 0u);
        EXPECT_TRUE(dispatch.selected_value == 0x5au);
        EXPECT_TRUE(dispatch.signed_value == (int16_t)-0x5a);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_CHANNEL_TABLE_BASE + 18u] == 0xffu);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_CHANNEL_TABLE_BASE + 19u] == 0xa6u);

        /* No stale word (v0819): the 0x12ba/0x12c0 andi.l #$7f are
           LONG (byte-verified), so both uppers are zeroed
           entry-independently and the stored word is ±selected.
           Payload 0x42 -> no stale 0x2100; bit8 of transformed is
           set here, so no negation either: selected 0x05 stored
           as +0x0005. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        audio_rom[0x2b26u] = 0x00u; audio_rom[0x2b27u] = 0x60u;
        audio_rom[0x2b28u] = 0x40u; audio_rom[0x2b29u] = 0x00u;
        audio_rom[0x4000u + 0x0bu] = 0x05u;
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 0u;
        descriptor[2] = 2u;
        descriptor[7] = 0u;
        {
            const uint8_t packet[4] = {0xe2u, 0x42u, 0u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) == VF2_OK);
        EXPECT_TRUE(dispatch.handler_address == 0x12b6u);
        EXPECT_TRUE(dispatch.channel == 2u);
        EXPECT_TRUE(dispatch.selected_value == 0x05u);
        EXPECT_TRUE(dispatch.signed_value == (int16_t)0x0005);
        EXPECT_TRUE(dispatch.voices_matched == 0u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_SHARED_COUNTER_151E + 6u] == 0x00u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_SHARED_COUNTER_151E + 7u] == 0x05u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_CHANNEL_TABLE_BASE + 4u] == 0x00u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_CHANNEL_TABLE_BASE + 5u] == 0x05u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);

        /* e0 voice tail (0x1356): per-match SCSP+0x10 and ram[0x601a]. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        audio_rom[0x2b26u] = 0x00u; audio_rom[0x2b27u] = 0x60u;
        audio_rom[0x2b28u] = 0x40u; audio_rom[0x2b29u] = 0x00u;
        audio_rom[0x4000u + 0x0bu] = 0x05u;
        audio_rom[0x2682u] = 0x00u; audio_rom[0x2683u] = 0x60u;
        audio_rom[0x2684u] = 0x50u; audio_rom[0x2685u] = 0x00u;
        /* Slide is 0x0025 under the corrected result (+5 shared +
           0x20 addend), so the picker/timer bytes live at the
           +0x4a offsets, not the legacy +0x424a ones. */
        audio_rom[0x504au] = 0x12u; audio_rom[0x504bu] = 0x34u;
        audio_rom[0xc72eu] = 0x00u; audio_rom[0xc72fu] = 0x60u;
        audio_rom[0xc730u] = 0x60u; audio_rom[0xc731u] = 0x00u;
        audio_rom[0x60aau] = 0xabu; audio_rom[0x60abu] = 0xcdu;
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 0u;
        descriptor[2] = 2u;
        /* P6 fixture shift (here and at the small-shaped e0 tail
           below): ROM 0x135e is `add.b`, so only descriptor[5] (the
           low byte) contributes to slide. voice[10] = 0x20 +
           desc[5] = 0 gives addend 0x20; with the corrected shared
           result (+5) slide = 0x0025, so the picker/timer marker
           bytes are staged at the +0x4a offsets. The rest of the e0
           fixture -- SCSP[0x30] expected 0x2234 here and 0x0020 in
           the small-shaped leg, ram[0x203a..0x203b] = 0xab,0xcd --
           continues to test the SAME indexed ROM byte. */
        descriptor[5] = 0u;
        descriptor[6] = 0u;
        descriptor[7] = 0u;
        {
            uint8_t *tail_voice = board.sound_ram + VF2_SOUND_VOICE_TABLE_BASE;
            /* Slot word 0x0020 (voice[0..1]) proves the stores are
               slot-indexed: SCSP[0x0030], ram[0x203a]. */
            tail_voice[0] = 0u;
            tail_voice[1] = 0x20u;
            tail_voice[2] = 0x01u;
            tail_voice[3] = 0x23u;
            /* Byte match (v0819): ROM 0x1332 is cmp.B on voice[4]. */
            tail_voice[4] = 2u;
            tail_voice[5] = 2u;
            tail_voice[10] = 0x20u;
        }
        {
            const uint8_t packet[4] = {0xe2u, 0x42u, 0u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) == VF2_OK);
        EXPECT_TRUE(dispatch.handler_address == 0x12b6u);
        EXPECT_TRUE(dispatch.voices_matched == 1u);
        /* ROM 0x1396: SCSP[d4+0x10] = shaped with d4 = 0x0020, so
           SCSP[0x0030] = 0x2234. A d4 of 0 would coincide with the
           old absolute-write misreading; the nonzero slot word
           proves the indexing. */
        EXPECT_TRUE(vf2_scsp_read_u16(&board.scsp, 0x30u, &value) == VF2_OK);
        EXPECT_TRUE(value == 0x2234u);
        /* ROM 0x13c4: ram[0x201a+d4] = ram[0x203a], the stream-slot
           timer word, not ram[0x601a]. */
        EXPECT_TRUE(board.sound_ram[0x203au] == 0xabu);
        EXPECT_TRUE(board.sound_ram[0x203bu] == 0xcdu);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);

        /* e0 voice tail, small shaped value: SCSP[d4+0x10] with
           d4 = 0x0020. voice[3] = 0x03 keeps nibble 3 (same tables)
           with high 0x00; picker 0x0020 -> shaped 0x0020 ->
           SCSP[0x0030] = 0x0020. Row base repatched for row 0. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        audio_rom[0x2b26u] = 0x00u; audio_rom[0x2b27u] = 0x60u;
        audio_rom[0x2b28u] = 0x40u; audio_rom[0x2b29u] = 0x00u;
        audio_rom[0x4000u + 0x0bu] = 0x05u;
        audio_rom[0x2682u] = 0x00u; audio_rom[0x2683u] = 0x60u;
        audio_rom[0x2684u] = 0x50u; audio_rom[0x2685u] = 0x00u;
        audio_rom[0x504au] = 0x00u; audio_rom[0x504bu] = 0x20u;
        audio_rom[0xc726u] = 0x00u; audio_rom[0xc727u] = 0x60u;
        audio_rom[0xc728u] = 0x60u; audio_rom[0xc729u] = 0x00u;
        audio_rom[0x60aau] = 0xabu; audio_rom[0x60abu] = 0xcdu;
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 0u;
        descriptor[2] = 2u;
        /* P6 fixture shift: see the matching note in the first e0
           fixture above. desc[6] = 0 here too; slide is 0x0025. */
        descriptor[5] = 0u;
        descriptor[6] = 0u;
        descriptor[7] = 0u;
        {
            uint8_t *tail_voice = board.sound_ram + VF2_SOUND_VOICE_TABLE_BASE;
            tail_voice[0] = 0u;
            tail_voice[1] = 0x20u;
            tail_voice[2] = 0x01u;
            tail_voice[3] = 0x03u;
            /* Byte match (v0819): ROM 0x1332 is cmp.B on voice[4]. */
            tail_voice[4] = 2u;
            tail_voice[5] = 2u;
            /* P6 fixture shift: ROM 0x135e is `add.b`, so only
               descriptor[5] (the low byte) adds; slide is 0x0025
               (see above). */
            tail_voice[10] = 0x20u;
        }
        {
            const uint8_t packet[4] = {0xe2u, 0x42u, 0u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) == VF2_OK);
        EXPECT_TRUE(dispatch.handler_address == 0x12b6u);
        EXPECT_TRUE(dispatch.voices_matched == 1u);
        EXPECT_TRUE(vf2_scsp_read_u16(&board.scsp, 0x0030u, &value) == VF2_OK);
        EXPECT_TRUE(value == 0x0020u);
        EXPECT_TRUE(board.sound_ram[0x203au] == 0xabu);
        EXPECT_TRUE(board.sound_ram[0x203bu] == 0xcdu);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);

        /* B0 default (v0795b): entry = command&0x7f; anything but 0x3a
           takes the table-default rts: consume, no effects. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 0u;
        descriptor[2] = 1u;
        {
            const uint8_t packet[4] = {0xb1u, 0x01u, 0x50u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) == VF2_OK);
        EXPECT_TRUE(dispatch.handler_address == 0x13ccu);
        EXPECT_TRUE(dispatch.channel == 1u);
        EXPECT_TRUE(dispatch.selected_value == 0x31u);
        EXPECT_TRUE(dispatch.descriptors_matched == 1u);
        EXPECT_TRUE(dispatch.handled != 0);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);
        EXPECT_TRUE(dispatch.cursor_after == 4u);

        /* B0 entry 0x3f is likewise default-rts: only 0x3a is special. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 0u;
        descriptor[2] = 15u;
        {
            const uint8_t packet[4] = {0xbfu, 0x01u, 0x02u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) == VF2_OK);
        EXPECT_TRUE(dispatch.handler_address == 0x13ccu);
        EXPECT_TRUE(dispatch.channel == 15u);
        EXPECT_TRUE(dispatch.selected_value == 0x3fu);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);
        EXPECT_TRUE(dispatch.cursor_after == 4u);

        /* B0 entry 0x3a with payload&0x7f == 0x7f traps (mid-dbf, stale
           +2): staged bytes persist, packet held. */
        {
            static uint8_t rich_rom[0x10010u];
            memset(rich_rom, 0, sizeof(rich_rom));
            test_boot_rom(rich_rom);
            rich_rom[0x5b26u] = 0x00u; rich_rom[0x5b27u] = 0x02u;
            rich_rom[0x5b28u] = 0xaau; rich_rom[0x5b29u] = 0xbbu;
            EXPECT_TRUE(vf2_sound_board_initialize(&board, rich_rom, sizeof(rich_rom),
                                       sample_rom, sizeof(sample_rom)) == VF2_OK);
            descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
            descriptor[0] = 0x80u;
            descriptor[1] = 0u;
            descriptor[2] = 10u;
            {
                const uint8_t packet[4] = {0xbau, 0x7fu, 0u, 0u};
                EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
            }
            EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) ==
                        VF2_ERROR_UNSUPPORTED);
            EXPECT_TRUE(dispatch.handler_address == 0x13ccu);
            EXPECT_TRUE(dispatch.channel == 10u);
            EXPECT_TRUE(dispatch.selected_value == 0x3au);
            EXPECT_TRUE(board.sound_ram[0x5000u] == 0xaau);
            EXPECT_TRUE(board.sound_ram[0x5001u] == 0xbbu);
            EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 1u);
            EXPECT_TRUE(dispatch.cursor_after == dispatch.cursor_before);
        }

        /* B0 entry 0x3a upload (payload 0x05): staged + record copy +
           slot table, consumed. */
        {
            static uint8_t rich_rom[0x10010u];
            memset(rich_rom, 0, sizeof(rich_rom));
            test_boot_rom(rich_rom);
            rich_rom[0x5b26u] = 0x00u; rich_rom[0x5b27u] = 0x00u;
            rich_rom[0x5b28u] = 0x00u; rich_rom[0x5b29u] = 0x60u;
            rich_rom[0x5b2au] = 0x7au; rich_rom[0x5b2bu] = 0x00u;
            rich_rom[0x8df0u + 0x14u] = 0x00u; rich_rom[0x8df1u + 0x14u] = 0x60u;
            rich_rom[0x8df2u + 0x14u] = 0x78u; rich_rom[0x8df3u + 0x14u] = 0x00u;
            rich_rom[0x7800u] = 0x00u; rich_rom[0x7801u] = 0x01u;
            rich_rom[0x7802u] = 0x00u; rich_rom[0x7803u] = 0x00u;
            rich_rom[0x7a00u] = 0x00u; rich_rom[0x7a01u] = 0x60u;
            rich_rom[0x7a02u] = 0x74u; rich_rom[0x7a03u] = 0x00u;
            rich_rom[0x7a04u] = 0x00u; rich_rom[0x7a05u] = 0x00u;
            rich_rom[0x7a06u] = 0xfeu; rich_rom[0x7a07u] = 0x04u;
            rich_rom[0x7a08u] = 0x11u; rich_rom[0x7a09u] = 0x22u;
            rich_rom[0x7a0au] = 0x33u; rich_rom[0x7a0bu] = 0x44u;
            rich_rom[0x7400u] = 0xdeu; rich_rom[0x7401u] = 0xadu;
            rich_rom[0x7402u] = 0xbeu; rich_rom[0x7403u] = 0xefu;
            EXPECT_TRUE(vf2_sound_board_initialize(&board, rich_rom, sizeof(rich_rom),
                                       sample_rom, sizeof(sample_rom)) == VF2_OK);
            descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
            descriptor[0] = 0x80u;
            descriptor[1] = 0u;
            descriptor[2] = 10u;
            {
                const uint8_t packet[4] = {0xbau, 0x05u, 0u, 0u};
                EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
            }
            EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) == VF2_OK);
            EXPECT_TRUE(dispatch.handler_address == 0x13ccu);
            EXPECT_TRUE(dispatch.channel == 10u);
            EXPECT_TRUE(dispatch.selected_value == 0x3au);
            EXPECT_TRUE(dispatch.handled != 0);
            EXPECT_TRUE(board.sound_ram[0x6008u] == 0x11u);
            EXPECT_TRUE(board.sound_ram[0x6009u] == 0x22u);
            EXPECT_TRUE(board.sound_ram[0x600au] == 0x33u);
            EXPECT_TRUE(board.sound_ram[0x600bu] == 0x44u);
            EXPECT_TRUE(board.sound_ram[0x5000u] == 0x80u);
            EXPECT_TRUE(board.sound_ram[0x50000u] == 0xdeu);
            EXPECT_TRUE(board.sound_ram[0x50001u] == 0xadu);
            EXPECT_TRUE(board.sound_ram[0x50002u] == 0xbeu);
            EXPECT_TRUE(board.sound_ram[0x50003u] == 0xefu);
            EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);
            EXPECT_TRUE(dispatch.cursor_after == 4u);
        }
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_VOICE_TABLE_BASE + 2u] == 0u);
        /* 80 minimal (v0795a): d2 = command (primer skipped), base =
           (command&0x7f) + desc[4..5]-word, then refuse. The low arm is
           the only matchable shape (stale branch); the high arm cannot
           match the drain. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 0u;
        descriptor[2] = 1u;
        descriptor[4] = 0u;
        descriptor[5] = 0x20u;
        {
            const uint8_t packet[4] = {0x81u, 0u, 0u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) ==
                    VF2_ERROR_UNSUPPORTED);
        EXPECT_TRUE(dispatch.handler_address == 0x10d2u);
        EXPECT_TRUE(dispatch.channel == 1u);
        EXPECT_TRUE(dispatch.descriptors_matched == 1u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_CURRENT] == 0x21u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 1u);
        EXPECT_TRUE(dispatch.cursor_after == dispatch.cursor_before);

        /* 80 base word-add with low truncation: 0x0f + 0x100 -> 0x0f. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 0u;
        descriptor[2] = 15u;
        descriptor[4] = 1u;
        descriptor[5] = 0u;
        {
            const uint8_t packet[4] = {0x8fu, 0u, 0u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) ==
                    VF2_ERROR_UNSUPPORTED);
        EXPECT_TRUE(dispatch.handler_address == 0x10d2u);
        EXPECT_TRUE(dispatch.channel == 15u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_CURRENT] == 0x0fu);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 1u);
        EXPECT_TRUE(dispatch.cursor_after == dispatch.cursor_before);

        /* B0 entries 0x7d/0x7e are table-default (dead 0x1c0a): consume
           with no voice effects. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 0u;
        descriptor[2] = 0x03u;
        {
            const uint8_t packet[4] = {0xb3u, 0x7du, 0u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) == VF2_OK);
        EXPECT_TRUE(dispatch.handler_address == 0x13ccu);
        EXPECT_TRUE(dispatch.selected_value == 0x33u);
        EXPECT_TRUE(dispatch.voices_cleared == 0u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);
        EXPECT_TRUE(dispatch.cursor_after == 4u);

        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 0u;
        descriptor[2] = 0x03u;
        {
            const uint8_t packet[4] = {0xb3u, 0x7eu, 0u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) == VF2_OK);
        EXPECT_TRUE(dispatch.handler_address == 0x13ccu);
        EXPECT_TRUE(dispatch.selected_value == 0x33u);
        EXPECT_TRUE(dispatch.voices_cleared == 0u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);
        EXPECT_TRUE(dispatch.cursor_after == 4u);

        /* The ROM class switch has no ignore arm: a 0xd3 command falls
           through into the 0x90 head at 0x858 (bne.w from 0x84e). With a
           zero second byte it traps at 0x866: base stored, packet held. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 0u;
        descriptor[2] = 0x03u;
        {
            const uint8_t packet[4] = {0xd3u, 0u, 0u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) ==
                    VF2_ERROR_UNSUPPORTED);
        EXPECT_TRUE(dispatch.command_class == 0xd0u);
        EXPECT_TRUE(dispatch.handler_address == 0x858u);
        EXPECT_TRUE(dispatch.channel == 3u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_CURRENT] == 0u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_LOOKUP_MODE] == 0u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 1u);
        EXPECT_TRUE(dispatch.cursor_after == dispatch.cursor_before);
    }
    {
        /* Boot mirror (v0794a): voice slot words, stream records,
           shared slots, SCSP constants. */
        size_t voice = 0u;
        uint8_t level = 0u;
        uint16_t word = 0u;
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        for (voice = 0u; voice < 32u; ++voice) {
            uint8_t *record = board.sound_ram +
                VF2_SOUND_VOICE_TABLE_BASE +
                voice * VF2_SOUND_VOICE_BYTES;
            EXPECT_TRUE(record[0u] == (uint8_t)(((uint16_t)voice * 0x20u) >> 8u));
            EXPECT_TRUE(record[1u] ==
                        (uint8_t)(((uint16_t)voice * 0x20u) & 0xffu));
            EXPECT_TRUE(record[0x0cu] == 0u && record[0x0du] == 0u &&
                        record[0x0eu] == 0u && record[0x0fu] == 0u);
        }
        for (voice = 0u; voice < 32u; ++voice) {
            uint8_t *record = board.sound_ram + 0x2000u + voice * 0x20u;
            EXPECT_TRUE(record[0xeu] == 0u);
            EXPECT_TRUE(record[0xfu] == (uint8_t)(1u + voice / 8u));
            EXPECT_TRUE(record[0x10u] ==
                        (uint8_t)(((uint8_t)voice * 0x20u) & 0xffu));
            EXPECT_TRUE(record[0x11u] == 0u);
            EXPECT_TRUE(record[0x12u] == (uint8_t)((uint8_t)voice * 2u));
            EXPECT_TRUE(record[0x16u] ==
                        (uint8_t)(8u + (uint8_t)voice * 8u));
        }
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_SHARED_FACTOR_151F] == 0xffu);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_SHARED_FACTOR_151F] == 0xffu);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_SHARED_COUNTER_151E + 8u] ==
                    0xefu);
        word = (uint16_t)((uint16_t)board.sound_ram[0x251au] << 8u) |
            board.sound_ram[0x251bu];
        EXPECT_TRUE(word == 0x0000u);
        EXPECT_TRUE(board.sound_ram[0x251cu] == 0x28u);
        EXPECT_TRUE(board.sound_ram[0x251du] == 0x00u);
        EXPECT_TRUE(vf2_scsp_read_u16(&board.scsp, 0x402u, &word) == VF2_OK);
        EXPECT_TRUE(word == 0x138u);
        EXPECT_TRUE(vf2_scsp_read_u16(&board.scsp, 0x418u, &word) == VF2_OK);
        EXPECT_TRUE(word == 0xceu);
        EXPECT_TRUE(vf2_scsp_read_u16(&board.scsp, 0x41au, &word) == VF2_OK);
        EXPECT_TRUE(word == 0x3c0u);
        EXPECT_TRUE(vf2_scsp_read_u16(&board.scsp, 0x422u, &word) == VF2_OK);
        EXPECT_TRUE(word == 0x40u);
        EXPECT_TRUE(vf2_scsp_read_u8(&board.scsp, 0x00u, &level) == VF2_OK);
        EXPECT_TRUE(level == 0x10u);
        EXPECT_TRUE(vf2_scsp_read_u8(&board.scsp, 0x3edu, &level) == VF2_OK);
        EXPECT_TRUE(level == 0xffu);
        EXPECT_TRUE(vf2_scsp_read_u8(&board.scsp, 0x17u, &level) == VF2_OK);
        EXPECT_TRUE(level == 0x5au);
        EXPECT_TRUE(vf2_scsp_read_u8(&board.scsp, 0x700u, &level) == VF2_OK);
        EXPECT_TRUE(level == 0x5au);
        EXPECT_TRUE(vf2_scsp_read_u8(&board.scsp, 0x701u, &level) == VF2_OK);
        EXPECT_TRUE(level == 0u);
    }
    {
        /* Fail-closed init: a short ROM refuses instead of booting. */
        static const uint8_t tiny_rom[0x100u] = {0u};
        EXPECT_TRUE(vf2_sound_board_initialize(&board, tiny_rom, sizeof(tiny_rom),
                                   sample_rom, sizeof(sample_rom)) ==
                    VF2_ERROR_OUT_OF_BOUNDS);
    }
    {
        /* Boot descriptor precompute (v0794a): desc[10] from desc[3]. */
        uint8_t *descriptor = NULL;
        audio_rom[0x7103u] = 0x80u;
        audio_rom[0x710au] = 0x55u;
        audio_rom[0x7113u] = 0x00u;
        audio_rom[0x711au] = 0x77u;
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        EXPECT_TRUE(descriptor[3u] == 0x80u);
        EXPECT_TRUE(descriptor[0x0au] == 0x80u);
        EXPECT_TRUE(descriptor[0x13u] == 0x00u);
        EXPECT_TRUE(descriptor[0x1au] == 0x77u);
        audio_rom[0x7103u] = 0x00u;
        audio_rom[0x710au] = 0x00u;
        audio_rom[0x7113u] = 0x00u;
        audio_rom[0x711au] = 0x00u;
    }
    {
        /* Sample uploader (v0794a): staging, record copy, guard. */
        /* test_boot_rom clears 0x10010 bytes; the board is initialised with a 0x10000-byte ROM. */
        static uint8_t rich_rom_storage[0x10010u];
        uint8_t *const rich_rom = rich_rom_storage;
        uint8_t *slot_table = NULL;
        test_boot_rom(rich_rom);
        /* Stage-only (d1 < 0 skips the upload). */
        rich_rom[0x5b26u] = 0x00u; rich_rom[0x5b27u] = 0x04u;
        rich_rom[0x5b28u] = 0xaau; rich_rom[0x5b29u] = 0xbbu;
        rich_rom[0x5b2au] = 0xccu; rich_rom[0x5b2bu] = 0xddu;
        EXPECT_TRUE(vf2_sound_board_initialize(&board, rich_rom, UINT32_C(0x10000),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        EXPECT_TRUE(board.sound_ram[0x5000u] == 0xaau);
        EXPECT_TRUE(board.sound_ram[0x5001u] == 0xbbu);
        EXPECT_TRUE(board.sound_ram[0x5002u] == 0xccu);
        EXPECT_TRUE(board.sound_ram[0x5003u] == 0xddu);
        /* One-record upload (d1 >= 0, row 0x607800, sel 0). */
        rich_rom[0x5b26u] = 0x00u; rich_rom[0x5b27u] = 0x00u;
        rich_rom[0x5b28u] = 0x00u; rich_rom[0x5b29u] = 0x60u;
        rich_rom[0x5b2au] = 0x7au; rich_rom[0x5b2bu] = 0x00u;
        rich_rom[0x8df0u] = 0x00u; rich_rom[0x8df1u] = 0x60u;
        rich_rom[0x8df2u] = 0x78u; rich_rom[0x8df3u] = 0x00u;
        rich_rom[0x7800u] = 0x00u; rich_rom[0x7801u] = 0x01u;
        rich_rom[0x7802u] = 0x00u; rich_rom[0x7803u] = 0x00u;
        rich_rom[0x7a00u] = 0x00u; rich_rom[0x7a01u] = 0x60u;
        rich_rom[0x7a02u] = 0x74u; rich_rom[0x7a03u] = 0x00u;
        rich_rom[0x7a04u] = 0x00u; rich_rom[0x7a05u] = 0x00u;
        rich_rom[0x7a06u] = 0xfeu; rich_rom[0x7a07u] = 0x04u;
        rich_rom[0x7a08u] = 0x11u; rich_rom[0x7a09u] = 0x22u;
        rich_rom[0x7a0au] = 0x33u; rich_rom[0x7a0bu] = 0x44u;
        rich_rom[0x7400u] = 0xdeu; rich_rom[0x7401u] = 0xadu;
        rich_rom[0x7402u] = 0xbeu; rich_rom[0x7403u] = 0xefu;
        EXPECT_TRUE(vf2_sound_board_initialize(&board, rich_rom, UINT32_C(0x10000),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        EXPECT_TRUE(board.sound_ram[0x6008u] == 0x11u);
        EXPECT_TRUE(board.sound_ram[0x6009u] == 0x22u);
        EXPECT_TRUE(board.sound_ram[0x600au] == 0x33u);
        EXPECT_TRUE(board.sound_ram[0x600bu] == 0x44u);
        EXPECT_TRUE(board.sound_ram[0x5000u] == 0x80u);
        slot_table = board.sound_ram + 0x5000u;
        EXPECT_TRUE(slot_table[1u] == 0x05u && slot_table[2u] == 0x00u &&
                    slot_table[3u] == 0x00u);
        EXPECT_TRUE(board.sound_ram[0x50000u] == 0xdeu);
        EXPECT_TRUE(board.sound_ram[0x50001u] == 0xadu);
        EXPECT_TRUE(board.sound_ram[0x50002u] == 0xbeu);
        EXPECT_TRUE(board.sound_ram[0x50003u] == 0xefu);
        /* Guard trip (4 x 0x8000-byte records cross $6FFFF). */
        rich_rom[0x7800u] = 0x00u; rich_rom[0x7801u] = 0x04u;
        rich_rom[0x7802u] = 0x00u; rich_rom[0x7803u] = 0x00u;
        rich_rom[0x7804u] = 0x00u; rich_rom[0x7805u] = 0x00u;
        rich_rom[0x7806u] = 0x00u; rich_rom[0x7807u] = 0x00u;
        rich_rom[0x7808u] = 0x00u; rich_rom[0x7809u] = 0x00u;
        rich_rom[0x7a00u] = 0x00u; rich_rom[0x7a01u] = 0x60u;
        rich_rom[0x7a02u] = 0x76u; rich_rom[0x7a03u] = 0x00u;
        rich_rom[0x7a04u] = 0x00u; rich_rom[0x7a05u] = 0x00u;
        rich_rom[0x7a06u] = 0x7eu; rich_rom[0x7a07u] = 0x00u;
        rich_rom[0x7a08u] = 0x00u; rich_rom[0x7a09u] = 0x00u;
        rich_rom[0x7a0au] = 0x00u; rich_rom[0x7a0bu] = 0x00u;
        EXPECT_TRUE(vf2_sound_board_initialize(&board, rich_rom, UINT32_C(0x10000),
                                   sample_rom, sizeof(sample_rom)) ==
                    VF2_ERROR_UNSUPPORTED);
        EXPECT_TRUE(board.sound_ram[0x50000u] == 0x00u);
    }
    return failures == 0 ? 0 : 1;
}
