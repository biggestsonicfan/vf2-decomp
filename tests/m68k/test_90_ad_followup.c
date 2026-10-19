#include "vf2/sound_board.h"
#include "vf2/status.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ROM-free control fixture for AD23/AD24 90 HIGH packets.
 * Full original-68000 parity is separately required. */
static vf2_sound_board board;
static uint8_t rom[0x80000u];
static int failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"90 followup fail line %u: %s\n", (unsigned)__LINE__, #c); ++failures; } } while (0)

static void write_pointer(uint32_t rom_address, uint32_t value)
{
    size_t at=(size_t)(rom_address-UINT32_C(0x600000));
    rom[at] = (uint8_t)(value>>24u);
    rom[at+1u] = (uint8_t)(value>>16u);
    rom[at+2u] = (uint8_t)(value>>8u);
    rom[at+3u] = (uint8_t)value;
}

static void setup(uint8_t command, uint8_t payload, uint8_t third,
                  uint8_t desc_mode, uint8_t mode, uint32_t row,
                  uint16_t word, uint8_t loopb)
{
    uint8_t *desc;
    size_t entry=(size_t)(row-UINT32_C(0x600000))+(size_t)(payload&0x7fu)*6u;
    memset(&board,0,sizeof(board));
    memset(rom,0,sizeof(rom));
    board.audio_rom=rom;
    board.audio_rom_size=sizeof(rom);
    vf2_scsp_initialize(&board.scsp);
    rom[0xc6a6u+(size_t)third]=mode;
    write_pointer(UINT32_C(0x6032c6)+(uint32_t)(desc_mode-0xf0u)*4u,row);
    rom[entry]=(uint8_t)(word>>8u);
    rom[entry+1u]=(uint8_t)word;
    rom[entry+2u]=0xe0u;
    rom[entry+3u]=0u;
    rom[entry+4u]=0u;
    rom[entry+5u]=0u;
    rom[0xc3e6u+(size_t)(payload&0x7fu)]=loopb;
    desc=board.sound_ram+VF2_SOUND_COMMAND_DESCRIPTOR_BASE+
         (size_t)(command==0x9bu?11u:10u)*VF2_SOUND_COMMAND_DESCRIPTOR_BYTES;
    desc[0]=0x81u;desc[1]=(uint8_t)(command&0x0fu);
    desc[2]=desc_mode;desc[3]=0xffu;desc[12]=8u;
    board.sound_ram[UINT32_C(0x251a)+2u]=0x28u;
    board.sound_ram[VF2_SOUND_COMMAND_RING_BASE]=command;
    board.sound_ram[VF2_SOUND_COMMAND_RING_BASE+1u]=payload;
    board.sound_ram[VF2_SOUND_COMMAND_RING_BASE+2u]=third;
    board.sound_ram[VF2_SOUND_COMMAND_COUNT]=1u;
}

static void test_ad90(uint8_t cmd,uint8_t payload,uint8_t third,
                      uint8_t dmode,uint8_t mode,uint32_t row,
                      uint16_t word,uint8_t loopb)
{
    vf2_sound_dispatch_report report={0};
    vf2_status status;
    setup(cmd,payload,third,dmode,mode,row,word,loopb);
    status=vf2_sound_board_dispatch_next(&board,&report);
    CHECK(status==VF2_ERROR_UNSUPPORTED);
    CHECK(report.handler_address==UINT32_C(0xfac));
    CHECK(report.descriptors_matched==1u);
    CHECK(report.channel==(uint8_t)(cmd&0x0fu));
    CHECK(report.selected_value==mode);
    CHECK(report.handled==0);
    CHECK(report.cursor_before==0u && report.cursor_after==0u);
    CHECK(board.sound_ram[VF2_SOUND_COMMAND_COUNT]==1u);
    CHECK(board.sound_ram[VF2_SOUND_LOOKUP_CURRENT]==payload);
    CHECK(board.sound_ram[VF2_SOUND_LOOKUP_MODE]==mode);
    CHECK(board.sound_ram[VF2_SOUND_LOOKUP_CONTROL]==0u);
    CHECK(board.sound_ram[VF2_SOUND_LOOKUP_WORD_ALT]==0xe0u);
    CHECK(board.sound_ram[VF2_SOUND_LOOKUP_WORD_ALT+1u]==0u);
    CHECK(board.sound_ram[VF2_SOUND_LOOKUP_WORD]==(uint8_t)(word>>8u));
    CHECK(board.sound_ram[VF2_SOUND_LOOKUP_WORD+1u]==(uint8_t)word);
    CHECK(board.sound_ram[VF2_SOUND_LOOKUP_RELEASE]==0u);
    CHECK(board.sound_ram[VF2_SOUND_LOOKUP_BYTE]==loopb);
    CHECK(board.sound_ram[UINT32_C(0x251a)+2u]==0x28u);
    CHECK(board.sound_ram[UINT32_C(0x251a)+3u]==0x10u);
}

static void test_refuse_bad_table(void)
{
    vf2_sound_dispatch_report report={0};
    vf2_status status;
    setup(0x9bu,0x42u,0x78u,0xffu,0xfdu,UINT32_C(0x6046fe),
          0x0242u,0xd6u);
    write_pointer(UINT32_C(0x6032c6)+15u*4u,UINT32_C(0x690000));
    status=vf2_sound_board_dispatch_next(&board,&report);
    CHECK(status==VF2_ERROR_OUT_OF_BOUNDS);
    CHECK(board.sound_ram[VF2_SOUND_COMMAND_COUNT]==1u);
    CHECK(report.cursor_after==0u);
}

static void test_high_sentinel(void)
{
    vf2_sound_dispatch_report report={0};
    vf2_status status;
    const size_t entry=0x46feu+0x42u*6u;
    setup(0x9bu,0x42u,0x78u,0xffu,0xfdu,UINT32_C(0x6046fe),
          0x0242u,0xd6u);
    rom[entry]=0u;rom[entry+1u]=0xffu;
    status=vf2_sound_board_dispatch_next(&board,&report);
    CHECK(status==VF2_OK);
    CHECK(board.sound_ram[VF2_SOUND_COMMAND_COUNT]==0u);
    CHECK(report.handled==1);
    CHECK(board.sound_ram[VF2_SOUND_LOOKUP_CURRENT]==0x42u);
    CHECK(board.sound_ram[VF2_SOUND_LOOKUP_MODE]==0xfdu);
}

int main(void)
{
    test_ad90(0x9bu,0x42u,0x78u,0xffu,0xfdu,
              UINT32_C(0x6046fe),0x0242u,0xd6u);
    test_ad90(0x9au,0x01u,0x7fu,0xfeu,0xffu,
              UINT32_C(0x6045de),0x0261u,0x81u);
    test_refuse_bad_table();
    test_high_sentinel();
    if(failures==0)puts("AD-family 90 HIGH: two ROM-shaped routes, gate/refusal PASS");
    return failures!=0;
}
