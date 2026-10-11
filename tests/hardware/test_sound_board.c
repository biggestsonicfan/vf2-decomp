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
    /* 0x28e wave-blob source -> 0x607000 (0x500 zero bytes follow). */
    rom[0x4940u] = 0x00u; rom[0x4941u] = 0x60u;
    rom[0x4942u] = 0x70u; rom[0x4943u] = 0x00u;
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
    EXPECT_TRUE(vf2_sound_board_write_u16(
        &board, VF2_SCSP_CPU_BASE + 3u * VF2_SCSP_SLOT_BYTES, 0x0800u
    ) == VF2_OK);
    EXPECT_TRUE(vf2_sound_board_write_u16(
        &board, VF2_SCSP_CPU_BASE + 4u * VF2_SCSP_SLOT_BYTES, 0x0800u
    ) == VF2_OK);
    EXPECT_TRUE(vf2_sound_board_maintain_voices(&board, &maintenance) == VF2_OK);
    EXPECT_TRUE(maintenance.voices_scanned == VF2_SOUND_VOICE_COUNT);
    EXPECT_TRUE(maintenance.voices_expired == 2u);
    EXPECT_TRUE(maintenance.normal_expirations == 1u);
    EXPECT_TRUE(maintenance.release_expirations == 1u);
    EXPECT_TRUE(maintenance.voices_aged == 2u);
    EXPECT_TRUE(maintenance.counter_151e_before == 2u);
    EXPECT_TRUE(maintenance.counter_151e_after == 1u);
    EXPECT_TRUE(maintenance.counter_152e_before == 3u);
    EXPECT_TRUE(maintenance.counter_152e_after == 2u);
    EXPECT_TRUE(voices[2u] == 0u);
    EXPECT_TRUE(voices[VF2_SOUND_VOICE_BYTES + 2u] == 0u);
    EXPECT_TRUE(voices[VF2_SOUND_VOICE_BYTES + 9u] == 0u);
    EXPECT_TRUE(voices[9u] == 0u);
    EXPECT_TRUE(voices[VF2_SOUND_VOICE_BYTES + 5u] == 0u);
    EXPECT_TRUE(voices[VF2_SOUND_VOICE_BYTES + 2u * 0u + 9u] == 0u);
    EXPECT_TRUE(voices[2u * VF2_SOUND_VOICE_BYTES + 9u] == 0xffu);
    EXPECT_TRUE(vf2_scsp_read_u16(&board.scsp, 3u * VF2_SCSP_SLOT_BYTES,
                                  &value) == VF2_OK);
    EXPECT_TRUE((value & 0x0800u) == 0u);
    EXPECT_TRUE(vf2_scsp_read_u16(&board.scsp, 4u * VF2_SCSP_SLOT_BYTES,
                                  &value) == VF2_OK);
    EXPECT_TRUE((value & 0x0800u) == 0u);
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
        voice[4] = 0u;
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
        EXPECT_TRUE(vf2_sound_board_write_u16(
            &board, VF2_SCSP_CPU_BASE + 3u * VF2_SCSP_SLOT_BYTES, 0x0800u
        ) == VF2_OK);
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
        EXPECT_TRUE(value == 0x1000u);
        EXPECT_TRUE(vf2_scsp_read_u8(&board.scsp, 0x0bu, &byte) == VF2_OK);
        EXPECT_TRUE(byte == 0x1fu);
        EXPECT_TRUE(board.sound_ram[0x2000u] == 0u);

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

        /* The bounded 0x1f7c C/D packet path emits a four-byte ring entry and
           reloads its timer from the following continuation byte. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        audio_rom[0x8100u] = 0x12u;
        audio_rom[0x8101u] = 0x34u;
        audio_rom[0x8102u] = 0u;
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
        EXPECT_TRUE(stream_maintenance.descriptors_scanned == 8u);
        EXPECT_TRUE(stream_maintenance.timers_expired == 1u);
        EXPECT_TRUE(stream_maintenance.packets_emitted == 1u);
        EXPECT_TRUE(stream_maintenance.bytes_consumed == 2u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 1u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_RING_BASE] == 0xc0u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_RING_BASE + 1u] == 0x12u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_RING_BASE + 2u] == 0u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_RING_BASE + 3u] == 0u);
        EXPECT_TRUE(descriptor[2] == 0u && descriptor[3] == 0x34u);
        EXPECT_TRUE(descriptor[4] == 0u && descriptor[5] == 0x60u &&
                    descriptor[6] == 0x81u && descriptor[7] == 2u);

        /* F7 is a counted source skip with no emitted command packet. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        audio_rom[0x8200u] = 2u;
        audio_rom[0x8203u] = 1u;
        descriptor = board.sound_ram + VF2_SOUND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 0xf7u;
        descriptor[2] = 0u;
        descriptor[3] = 1u;
        descriptor[4] = 0u;
        descriptor[5] = 0x60u;
        descriptor[6] = 0x82u;
        descriptor[7] = 0u;
        EXPECT_TRUE(vf2_sound_board_maintain_streams(
            &board, &stream_maintenance
        ) == VF2_OK);
        EXPECT_TRUE(stream_maintenance.escape_records == 1u);
        EXPECT_TRUE(stream_maintenance.packets_emitted == 0u);
        EXPECT_TRUE(stream_maintenance.bytes_consumed == 4u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);
        EXPECT_TRUE(descriptor[2] == 0u && descriptor[3] == 1u);
        EXPECT_TRUE(descriptor[4] == 0u && descriptor[5] == 0x60u &&
                    descriptor[6] == 0x82u && descriptor[7] == 4u);
        /* Stream files live at absolute 0x3000, not 0x2000 (driver a2
           bases are 0x2000(a6) with A6 = 0x1000): pin both sides. */
        EXPECT_TRUE(board.sound_ram[0x3000u] == 0x80u);
        EXPECT_TRUE(board.sound_ram[0x2000u] == 0u);

        /* F0 scans through source bytes to the next F7 marker, then reloads
           the continuation timer without emitting a command packet. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        audio_rom[0x8400u] = 0xf0u;
        audio_rom[0x8401u] = 0x12u;
        audio_rom[0x8402u] = 0xf7u;
        audio_rom[0x8403u] = 1u;
        descriptor = board.sound_ram + VF2_SOUND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 0xf0u;
        descriptor[2] = 0u;
        descriptor[3] = 1u;
        descriptor[4] = 0u;
        descriptor[5] = 0x60u;
        descriptor[6] = 0x84u;
        descriptor[7] = 0u;
        EXPECT_TRUE(vf2_sound_board_maintain_streams(
            &board, &stream_maintenance
        ) == VF2_OK);
        EXPECT_TRUE(stream_maintenance.escape_records == 1u);
        EXPECT_TRUE(stream_maintenance.packets_emitted == 0u);
        EXPECT_TRUE(stream_maintenance.bytes_consumed == 4u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);
        EXPECT_TRUE(descriptor[2] == 0u && descriptor[3] == 1u);
        EXPECT_TRUE(descriptor[4] == 0u && descriptor[5] == 0x60u &&
                    descriptor[6] == 0x84u && descriptor[7] == 4u);

        /* FF consumes a non-pointer marker, skips its counted payload, and
           resumes at the continuation timer. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        audio_rom[0x8500u] = 0xffu;
        audio_rom[0x8501u] = 0x01u;
        audio_rom[0x8502u] = 2u;
        audio_rom[0x8505u] = 1u;
        descriptor = board.sound_ram + VF2_SOUND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 0xffu;
        descriptor[2] = 0u;
        descriptor[3] = 1u;
        descriptor[4] = 0u;
        descriptor[5] = 0x60u;
        descriptor[6] = 0x85u;
        descriptor[7] = 0u;
        EXPECT_TRUE(vf2_sound_board_maintain_streams(
            &board, &stream_maintenance
        ) == VF2_OK);
        EXPECT_TRUE(stream_maintenance.escape_records == 1u);
        EXPECT_TRUE(stream_maintenance.packets_emitted == 0u);
        EXPECT_TRUE(stream_maintenance.bytes_consumed == 6u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);
        EXPECT_TRUE(descriptor[2] == 0u && descriptor[3] == 1u);
        EXPECT_TRUE(descriptor[4] == 0u && descriptor[5] == 0x60u &&
                    descriptor[6] == 0x85u && descriptor[7] == 6u);

        /* FF/2F chain sentinels are bounded: an exhausted chain clears the
           descriptor without emitting a packet. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        audio_rom[0x8600u] = 0xffu;
        audio_rom[0x8601u] = 0x2fu;
        audio_rom[0x8700u] = 0xffu;
        audio_rom[0x8701u] = 0xffu;
        audio_rom[0x8702u] = 0xffu;
        audio_rom[0x8703u] = 0xffu;
        descriptor = board.sound_ram + VF2_SOUND_DESCRIPTOR_BASE;
        descriptor[0] = 0x88u;
        descriptor[1] = 0xffu;
        descriptor[2] = 0u;
        descriptor[3] = 1u;
        descriptor[4] = 0u;
        descriptor[5] = 0x60u;
        descriptor[6] = 0x86u;
        descriptor[7] = 0u;
        descriptor[8] = 0u;
        descriptor[9] = 0x60u;
        descriptor[10] = 0x87u;
        descriptor[11] = 0u;
        EXPECT_TRUE(vf2_sound_board_maintain_streams(
            &board, &stream_maintenance
        ) == VF2_OK);
        EXPECT_TRUE(stream_maintenance.escape_records == 1u);
        EXPECT_TRUE(stream_maintenance.packets_emitted == 0u);
        EXPECT_TRUE(stream_maintenance.bytes_consumed == 2u);
        EXPECT_TRUE(descriptor[0] == 0u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);

        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        audio_rom[0x8600u] = 0xffu;
        audio_rom[0x8601u] = 0x2fu;
        audio_rom[0x8700u] = 0xffu;
        audio_rom[0x8701u] = 0xffu;
        audio_rom[0x8702u] = 0xffu;
        audio_rom[0x8703u] = 0xf2u;
        descriptor = board.sound_ram + VF2_SOUND_DESCRIPTOR_BASE;
        descriptor[0] = 0x88u;
        descriptor[1] = 0xffu;
        descriptor[2] = 0u;
        descriptor[3] = 1u;
        descriptor[4] = 0u;
        descriptor[5] = 0x60u;
        descriptor[6] = 0x86u;
        descriptor[7] = 0u;
        descriptor[8] = 0u;
        descriptor[9] = 0x60u;
        descriptor[10] = 0x87u;
        descriptor[11] = 0u;
        board.sound_ram[VF2_SOUND_DESCRIPTOR_BASE + 0x20u] = 0x80u;
        EXPECT_TRUE(vf2_sound_board_maintain_streams(
            &board, &stream_maintenance
        ) == VF2_OK);
        EXPECT_TRUE(stream_maintenance.escape_records == 1u);
        EXPECT_TRUE(stream_maintenance.bytes_consumed == 2u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_DESCRIPTOR_BASE] == 0u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_DESCRIPTOR_BASE + 0x20u] == 0u);

        /* Ordinary 2F records consume one pointer longword, preserve the
           next chain address, and re-enter the common packet decoder. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        audio_rom[0x8800u] = 0xffu;
        audio_rom[0x8801u] = 0x2fu;
        audio_rom[0x8900u] = 0x00u;
        audio_rom[0x8901u] = 0x60u;
        audio_rom[0x8902u] = 0x8au;
        audio_rom[0x8903u] = 0x00u;
        audio_rom[0x8a00u] = 0xc0u;
        audio_rom[0x8a01u] = 0x34u;
        audio_rom[0x8a02u] = 0u;
        descriptor = board.sound_ram + VF2_SOUND_DESCRIPTOR_BASE;
        descriptor[0] = 0x88u;
        descriptor[1] = 0xffu;
        descriptor[2] = 0u;
        descriptor[3] = 1u;
        descriptor[4] = 0u;
        descriptor[5] = 0x60u;
        descriptor[6] = 0x88u;
        descriptor[7] = 0u;
        descriptor[8] = 0u;
        descriptor[9] = 0x60u;
        descriptor[10] = 0x89u;
        descriptor[11] = 0u;
        EXPECT_TRUE(vf2_sound_board_maintain_streams(
            &board, &stream_maintenance
        ) == VF2_OK);
        EXPECT_TRUE(stream_maintenance.pointer_handoffs == 1u);
        EXPECT_TRUE(stream_maintenance.packets_emitted == 1u);
        EXPECT_TRUE(stream_maintenance.bytes_consumed == 5u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_RING_BASE] == 0xc0u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_RING_BASE + 1u] == 0x34u);
        EXPECT_TRUE(descriptor[4] == 0u && descriptor[5] == 0x60u &&
                    descriptor[6] == 0x8au && descriptor[7] == 3u);
        EXPECT_TRUE(descriptor[8] == 0u && descriptor[9] == 0x60u &&
                    descriptor[10] == 0x89u && descriptor[11] == 4u);

        /* F1 records consume a nested pointer and source longword, then
           re-enter the common packet decoder at the pointed stream. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        audio_rom[0x8b00u] = 0xffu;
        audio_rom[0x8b01u] = 0x2fu;
        audio_rom[0x8c00u] = 0xffu;
        audio_rom[0x8c01u] = 0xffu;
        audio_rom[0x8c02u] = 0xffu;
        audio_rom[0x8c03u] = 0xf1u;
        audio_rom[0x8c04u] = 0x00u;
        audio_rom[0x8c05u] = 0x60u;
        audio_rom[0x8c06u] = 0x8du;
        audio_rom[0x8c07u] = 0x00u;
        audio_rom[0x8d00u] = 0x00u;
        audio_rom[0x8d01u] = 0x60u;
        audio_rom[0x8d02u] = 0x8eu;
        audio_rom[0x8d03u] = 0x00u;
        audio_rom[0x8e00u] = 0xc0u;
        audio_rom[0x8e01u] = 0x45u;
        audio_rom[0x8e02u] = 0u;
        descriptor = board.sound_ram + VF2_SOUND_DESCRIPTOR_BASE;
        descriptor[0] = 0x88u;
        descriptor[1] = 0xffu;
        descriptor[2] = 0u;
        descriptor[3] = 1u;
        descriptor[4] = 0u;
        descriptor[5] = 0x60u;
        descriptor[6] = 0x8bu;
        descriptor[7] = 0u;
        descriptor[8] = 0u;
        descriptor[9] = 0x60u;
        descriptor[10] = 0x8cu;
        descriptor[11] = 0u;
        EXPECT_TRUE(vf2_sound_board_maintain_streams(
            &board, &stream_maintenance
        ) == VF2_OK);
        EXPECT_TRUE(stream_maintenance.pointer_handoffs == 1u);
        EXPECT_TRUE(stream_maintenance.packets_emitted == 1u);
        EXPECT_TRUE(stream_maintenance.bytes_consumed == 5u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_RING_BASE] == 0xc0u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_RING_BASE + 1u] == 0x45u);
        EXPECT_TRUE(descriptor[4] == 0u && descriptor[5] == 0x60u &&
                    descriptor[6] == 0x8eu && descriptor[7] == 3u);
        EXPECT_TRUE(descriptor[8] == 0u && descriptor[9] == 0x60u &&
                    descriptor[10] == 0x8du && descriptor[11] == 4u);

        /* B0 stream control 0x10 updates descriptor +0x0e when +0x0c is clear. */
        EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom, sizeof(audio_rom),
                                   sample_rom, sizeof(sample_rom)) == VF2_OK);
        memset(audio_rom, 0, sizeof(audio_rom));
        test_boot_rom(audio_rom);
        audio_rom[0x8300u] = 0x10u;
        audio_rom[0x8301u] = 0x85u;
        audio_rom[0x8302u] = 1u;
        descriptor = board.sound_ram + VF2_SOUND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 0xb0u;
        descriptor[2] = 0u;
        descriptor[3] = 1u;
        descriptor[4] = 0u;
        descriptor[5] = 0x60u;
        descriptor[6] = 0x83u;
        descriptor[7] = 0u;
        descriptor[12] = 0u;
        descriptor[14] = 0u;
        EXPECT_TRUE(vf2_sound_board_maintain_streams(
            &board, &stream_maintenance
        ) == VF2_OK);
        EXPECT_TRUE(stream_maintenance.control_updates == 1u);
        EXPECT_TRUE(stream_maintenance.packets_emitted == 1u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_RING_BASE] == 0xb0u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_RING_BASE + 1u] == 0x10u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_RING_BASE + 2u] == 0x05u);
        EXPECT_TRUE(descriptor[14] == 0x05u);

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
        audio_rom[0x1000u + 0x03u] = 0x5au;
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 0u;
        descriptor[2] = 0x09u;
        descriptor[7] = 0u;
        {
            /* A matching channel-9 voice must NOT trigger the tail: the
               ROM falls into the benign B0-default rts instead. */
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
        EXPECT_TRUE(dispatch.signed_value == 0x5a);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_CHANNEL_TABLE_BASE + 18u] == 0x00u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_CHANNEL_TABLE_BASE + 19u] == 0x5au);

        /* e0 stale word (v0795a): d0.upper = ((payload&0x7f)<<7)&0xff00
           rides into the stored word on the odd path. Payload 0x42 ->
           stale 0x2100; selected 0x05 at the un-negated index 0x0b. */
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
        EXPECT_TRUE(dispatch.signed_value == (int16_t)0x2105);
        EXPECT_TRUE(dispatch.voices_matched == 0u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_SHARED_COUNTER_151E + 6u] == 0x21u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_SHARED_COUNTER_151E + 7u] == 0x05u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_CHANNEL_TABLE_BASE + 4u] == 0x21u);
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
        audio_rom[0x924au] = 0x12u; audio_rom[0x924bu] = 0x34u;
        audio_rom[0xc72eu] = 0x00u; audio_rom[0xc72fu] = 0x60u;
        audio_rom[0xc730u] = 0x60u; audio_rom[0xc731u] = 0x00u;
        audio_rom[0xa2aau] = 0xabu; audio_rom[0xa2abu] = 0xcdu;
        descriptor = board.sound_ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
        descriptor[0] = 0x80u;
        descriptor[1] = 0u;
        descriptor[2] = 2u;
        descriptor[5] = 0u;
        descriptor[6] = 0x10u;
        descriptor[7] = 0u;
        {
            uint8_t *tail_voice = board.sound_ram + VF2_SOUND_VOICE_TABLE_BASE;
            tail_voice[2] = 0x01u;
            tail_voice[3] = 0x23u;
            tail_voice[4] = 0u;
            tail_voice[5] = 2u;
            tail_voice[10] = 0x10u;
        }
        {
            const uint8_t packet[4] = {0xe2u, 0x42u, 0u, 0u};
            EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) == VF2_OK);
        }
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &dispatch) == VF2_OK);
        EXPECT_TRUE(dispatch.handler_address == 0x12b6u);
        EXPECT_TRUE(dispatch.voices_matched == 1u);
        EXPECT_TRUE(vf2_scsp_read_u16(&board.scsp, 0x10u, &value) == VF2_OK);
        EXPECT_TRUE(value == 0x2234u);
        EXPECT_TRUE(board.sound_ram[0x601au] == 0xabu);
        EXPECT_TRUE(board.sound_ram[0x601bu] == 0xcdu);
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
        static uint8_t rich_rom[0x10000u];
        uint8_t *slot_table = NULL;
        test_boot_rom(rich_rom);
        /* Stage-only (d1 < 0 skips the upload). */
        rich_rom[0x5b26u] = 0x00u; rich_rom[0x5b27u] = 0x04u;
        rich_rom[0x5b28u] = 0xaau; rich_rom[0x5b29u] = 0xbbu;
        rich_rom[0x5b2au] = 0xccu; rich_rom[0x5b2bu] = 0xddu;
        EXPECT_TRUE(vf2_sound_board_initialize(&board, rich_rom, sizeof(rich_rom),
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
        EXPECT_TRUE(vf2_sound_board_initialize(&board, rich_rom, sizeof(rich_rom),
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
        EXPECT_TRUE(vf2_sound_board_initialize(&board, rich_rom, sizeof(rich_rom),
                                   sample_rom, sizeof(sample_rom)) ==
                    VF2_ERROR_UNSUPPORTED);
        EXPECT_TRUE(board.sound_ram[0x50000u] == 0x00u);
    }
    return failures == 0 ? 0 : 1;
}
