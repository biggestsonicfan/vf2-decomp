#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "vf2/model2a.h"
#include "vf2/i960/executor.h"
#include "vf2/i960/snapshot.h"
#include "vf2/rom.h"

int main(int argc, char **argv){
    if(argc!=3){
        fprintf(stderr,"usage: %s <rom-dir> <out.snap>\n", argv[0]);
        return 1;
    }
    const char *rom_dir = argv[1];
    const char *out = argv[2];
    vf2_verify_summary summary={0};
    uint8_t *main_rom=NULL; size_t main_rom_size=0;
    uint8_t *main_data=NULL; size_t main_data_size=0;
    vf2_model2a machine; memset(&machine,0,sizeof(machine));
    vf2_i960_cpu cpu; memset(&cpu,0,sizeof(cpu));
    vf2_i960_snapshot snap; vf2_i960_snapshot_init(&snap);
    vf2_status status = vf2_romset_verify(rom_dir,NULL,&summary);
    if(status!=VF2_OK){fprintf(stderr,"verify %d\n",status); return 2;}
    status=vf2_romset_build_region(rom_dir,VF2_REGION_MAINCPU,&main_rom,&main_rom_size);
    if(status!=VF2_OK){fprintf(stderr,"build maincpu %d\n",status); return 3;}
    status=vf2_romset_build_region(rom_dir,VF2_REGION_MAIN_DATA,&main_data,&main_data_size);
    if(status!=VF2_OK){fprintf(stderr,"build maindata %d\n",status); return 4;}
    if(!vf2_model2a_initialize(&machine)){fprintf(stderr,"init\n"); return 5;}
    status=vf2_model2a_attach_main_rom(&machine,main_rom,main_rom_size);
    if(status!=VF2_OK){fprintf(stderr,"attach rom %d\n",status); return 6;}
    status=vf2_model2a_attach_main_data(&machine,main_data,main_data_size);
    if(status!=VF2_OK){fprintf(stderr,"attach data %d\n",status); return 7;}
    // Setup CPU at 0x4bfe0 with a call frame
    vf2_i960_cpu_reset(&cpu,0,0,0x1000);
    cpu.registers[1]=VF2_WORK_RAM_BASE + 0x3000; // stack pointer
    // need work RAM initialized with valid memory for 0x50068 etc. Use defaults.
    // Enter procedure at 0x4bfe0 returning to 0x0004c11c? Actually target until is 0x4c11c ret; but for snapshot entry, we want IP=0x4bfe0 with return to e.g. 0x1234
    status=vf2_i960_cpu_enter_procedure(&cpu, 0x0004bfe0, 0x0004c11c);
    if(status!=VF2_OK){fprintf(stderr,"enter %d\n",status); return 8;}
    // ensure memory defaults: zero runtime flags, mode, etc.
    // snapshot capture
    status=vf2_i960_snapshot_capture(&snap,&cpu,&machine);
    if(status!=VF2_OK){fprintf(stderr,"capture %d\n",status); return 9;}
    status=vf2_i960_snapshot_write_file(&snap, out);
    if(status!=VF2_OK){fprintf(stderr,"write %d\n",status); return 10;}
    vf2_i960_snapshot_destroy(&snap);
    vf2_model2a_shutdown(&machine);
    free(main_rom); free(main_data);
    printf("wrote %s\n",out);
    return 0;
}
