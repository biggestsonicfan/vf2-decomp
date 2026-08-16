from pathlib import Path

p=Path('src/recovered/texture_bridge_match.c')
s=p.read_text()
marker='static vf2_status execute_frame_phase17_bit7_index11('
if marker not in s: raise SystemExit('index11 marker missing')
code=r'''
static vf2_status phase17_index9_render_choices(vf2_model2a *machine)
{
    static const uint32_t records[4] = {
        UINT32_C(0x0005ff28), UINT32_C(0x0005ff24),
        UINT32_C(0x0005ff14), UINT32_C(0x0005ff18)
    };
    uint32_t record=0u,destination=0u,last_source=0u,last_destination=0u;
    uint64_t characters=0u; size_t index=0u; vf2_status status=VF2_OK;
    for(index=0u;status==VF2_OK&&index<2u;++index){
        status=vf2_model2a_read_u32(machine,records[index],&record);
        if(status==VF2_OK)status=phase16_copy_text_record(machine,record,&last_source,&last_destination,&characters);
    }
    if(status==VF2_OK)status=write_phase17_index0_text(machine,UINT32_C(27*0x80),UINT32_C(27),"         ");
    for(index=2u;status==VF2_OK&&index<4u;++index){
        status=vf2_model2a_read_u32(machine,records[index],&record);
        if(status==VF2_OK)status=phase16_copy_text_record(machine,record,&last_source,&last_destination,&characters);
    }
    for(index=0u;status==VF2_OK&&index<2u;++index){
        status=vf2_model2a_read_u32(machine,records[index],&record);
        if(status==VF2_OK)status=vf2_model2a_read_u32(machine,record,&destination);
        if(status==VF2_OK&&destination<UINT32_C(4))status=VF2_ERROR_UNSUPPORTED;
        if(status==VF2_OK)status=write_u16(machine,destination-UINT32_C(4),index==0u?UINT16_C(0x8020):UINT16_C(0x801c));
    }
    return status;
}

static vf2_status phase17_index9_restore_menu(vf2_model2a *machine)
{
    static const uint32_t extra[3]={UINT32_C(0x0005ff08),UINT32_C(0x0005ff14),UINT32_C(0x0005ff18)};
    const uint8_t phase_index=UINT8_C(9),spill=UINT8_C(0x56);
    uint32_t record=0u,destination=0u,last_source=0u,last_destination=0u;
    uint64_t characters=0u; size_t index=0u;
    vf2_status status=clear_tile_plane_64x48(machine,UINT32_C(0x01000000));
    if(status==VF2_OK)status=vf2_model2a_write(machine,UINT32_C(0x005000a4),&phase_index,sizeof(phase_index));
    if(status==VF2_OK)status=vf2_model2a_read_u32(machine,UINT32_C(0x0005feac)+UINT32_C(72),&record);
    if(status==VF2_OK)status=vf2_model2a_read_u32(machine,record,&destination);
    if(status==VF2_OK&&destination<UINT32_C(4))status=VF2_ERROR_UNSUPPORTED;
    if(status==VF2_OK)status=write_u16(machine,destination-UINT32_C(4),UINT16_C(0x801c));
    for(index=0u;status==VF2_OK&&index<12u;++index){
        status=vf2_model2a_read_u32(machine,UINT32_C(0x0005feac)+(uint32_t)index*UINT32_C(8),&record);
        if(status==VF2_OK)status=phase16_copy_text_record(machine,record,&last_source,&last_destination,&characters);
    }
    for(index=0u;status==VF2_OK&&index<3u;++index){
        status=vf2_model2a_read_u32(machine,extra[index],&record);
        if(status==VF2_OK)status=phase16_copy_text_record(machine,record,&last_source,&last_destination,&characters);
    }
    if(status==VF2_OK)status=vf2_model2a_write(machine,UINT32_C(0x005ff602),&spill,sizeof(spill));
    return status;
}

static vf2_status phase17_index9_initialize_defaults(vf2_model2a *machine)
{
    static const uint8_t system_defaults[29]={
        2u,2u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,15u,
        0u,0u,0xa0u,0u,0xc8u,0u,64u,64u,64u,37u,37u,37u,31u
    };
    static const uint8_t coin_defaults[15]={0u,0u,0u,0u,0u,0u,0u,0u,0u,2u,2u,2u,2u,2u,2u};
    static const uint8_t signature[16]={
        'V','I','R','T','U','A',' ','F','I','G','H','T','E','R',' ','2'
    };
    uint32_t base=0u; uint16_t crc=0u; const uint16_t version=UINT16_C(24);
    vf2_status status=vf2_model2a_read_u32(machine,UINT32_C(0x0050016c),&base);
    if(status==VF2_OK)status=vf2_model2a_write(machine,base+UINT32_C(0x3340),system_defaults,sizeof(system_defaults));
    if(status==VF2_OK)status=vf2_model2a_write(machine,UINT32_C(0x01d03340),system_defaults,sizeof(system_defaults));
    if(status==VF2_OK)status=phase17_index3_rebuild_transfer_tables(machine,system_defaults+22u);
    if(status==VF2_OK)status=vf2_model2a_write(machine,base+UINT32_C(0x3320),coin_defaults,sizeof(coin_defaults));
    if(status==VF2_OK)status=vf2_model2a_write(machine,UINT32_C(0x01d03320),coin_defaults,sizeof(coin_defaults));
    if(status==VF2_OK)status=phase17_index7_clear_backup(machine);
    if(status==VF2_OK)status=compute_table_crc16(machine,base+UINT32_C(0x3340),UINT32_C(29),&crc);
    if(status==VF2_OK)status=write_u16(machine,UINT32_C(0x01d03302),crc);
    if(status==VF2_OK)status=compute_table_crc16(machine,base+UINT32_C(0x3320),UINT32_C(15),&crc);
    if(status==VF2_OK)status=write_u16(machine,UINT32_C(0x01d03300),crc);
    if(status==VF2_OK)status=vf2_model2a_write(machine,UINT32_C(0x01d03308),signature,sizeof(signature));
    if(status==VF2_OK)status=vf2_model2a_write(machine,base+UINT32_C(0x3308),signature,sizeof(signature));
    if(status==VF2_OK)status=write_u16(machine,UINT32_C(0x01d03306),version);
    if(status==VF2_OK)status=write_u16(machine,base+UINT32_C(0x3306),version);
    return status;
}

static vf2_status execute_frame_phase17_bit7_index9(
    vf2_model2a *machine,vf2_i960_cpu *cpu,
    vf2_hybrid_bridge_report *report,uint8_t flagged_phase_index)
{
    const uint32_t base_input=UINT32_C(0x0ff7f700);
    uint32_t target=0u,input=0u,nav=0u,released=0u,previous=0u,selector=0u,seed=0u,counter=0u,record=0u,destination=0u;
    uint8_t a5=0u,a6=0u,a7=0u,spill=UINT8_C(0x56);
    uint64_t instructions=0u,calls=0u; int equal=1; vf2_status status=VF2_OK;
    if(flagged_phase_index!=UINT8_C(0x89)||cpu->local_frame_depth==0u)return VF2_ERROR_UNSUPPORTED;
    status=vf2_model2a_read_u32(machine,UINT32_C(0x0005fef0),&target);
    if(status==VF2_OK)status=vf2_model2a_read_u32(machine,UINT32_C(0x00500700),&input);
    if(status==VF2_OK)status=vf2_model2a_read_u32(machine,UINT32_C(0x00500704),&nav);
    if(status==VF2_OK)status=vf2_model2a_read_u32(machine,UINT32_C(0x00500708),&released);
    if(status==VF2_OK)status=vf2_model2a_read_u32(machine,UINT32_C(0x0050070c),&previous);
    if(status==VF2_OK)status=vf2_model2a_read_u32(machine,UINT32_C(0x0050002c),&selector);
    if(status==VF2_OK)status=vf2_model2a_read(machine,UINT32_C(0x005000a5),&a5,sizeof(a5));
    if(status==VF2_OK)status=vf2_model2a_read(machine,UINT32_C(0x005000a6),&a6,sizeof(a6));
    if(status==VF2_OK)status=vf2_model2a_read(machine,UINT32_C(0x005000a7),&a7,sizeof(a7));
    if(status!=VF2_OK||target!=UINT32_C(0x0005eaa0)||input!=base_input||released!=0u||previous!=base_input||selector!=UINT32_C(0x00020000)||a5>UINT8_C(4)||a6!=UINT8_C(0xff)||a7!=UINT8_C(0xff))return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;

    if(a5==UINT8_C(0)){
        const uint8_t next=UINT8_C(1); if(nav!=0u)return VF2_ERROR_UNSUPPORTED;
        status=phase17_index9_render_choices(machine);
        if(status==VF2_OK)status=vf2_model2a_write(machine,UINT32_C(0x005000a5),&next,sizeof(next));
        instructions=UINT64_C(711); calls=UINT64_C(7);
    }else if(a5==UINT8_C(1)){
        if((nav&(UINT32_C(0x04000104)|UINT32_C(0x200)))!=0u){
            status=phase17_index9_restore_menu(machine); instructions=UINT64_C(14313); calls=UINT64_C(19);
            if(status==VF2_OK)phase17_index7_post(cpu,UINT32_C(0x00078cb0),0u,UINT32_C(0xc0a0a3d7),UINT32_C(0x010016ac),5u,0);
        }else if(nav==0u){instructions=UINT64_C(49);calls=UINT64_C(4);cpu->registers[16]=0u;}
        else if(nav==UINT32_C(0x1000)||nav==UINT32_C(0x2000)){
            const uint8_t next=UINT8_C(2); status=vf2_model2a_write(machine,UINT32_C(0x005000a5),&next,sizeof(next));
            instructions=nav==UINT32_C(0x1000)?UINT64_C(51):UINT64_C(48);calls=UINT64_C(4);
            cpu->registers[16]=nav==UINT32_C(0x1000)?UINT32_C(1):UINT32_MAX;equal=0;
        }else return VF2_ERROR_UNSUPPORTED;
    }else if(a5==UINT8_C(2)){
        const uint8_t next=UINT8_C(3); if(nav!=0u)return VF2_ERROR_UNSUPPORTED;
        status=vf2_model2a_read_u32(machine,UINT32_C(0x0005ff24),&record);
        if(status==VF2_OK)status=vf2_model2a_read_u32(machine,record,&destination);
        if(status==VF2_OK&&destination>=UINT32_C(4))status=write_u16(machine,destination-UINT32_C(4),UINT16_C(0x8020));else if(status==VF2_OK)status=VF2_ERROR_UNSUPPORTED;
        if(status==VF2_OK)status=vf2_model2a_read_u32(machine,UINT32_C(0x0005ff28),&record);
        if(status==VF2_OK)status=vf2_model2a_read_u32(machine,record,&destination);
        if(status==VF2_OK&&destination>=UINT32_C(4))status=write_u16(machine,destination-UINT32_C(4),UINT16_C(0x801c));else if(status==VF2_OK)status=VF2_ERROR_UNSUPPORTED;
        if(status==VF2_OK)status=vf2_model2a_write(machine,UINT32_C(0x005000a5),&next,sizeof(next));
        instructions=UINT64_C(43);calls=UINT64_C(2);cpu->registers[16]=0u;cpu->registers[25]=UINT32_C(0x01000bb0);
    }else if(a5==UINT8_C(3)){
        if(nav==0u){instructions=UINT64_C(41);calls=UINT64_C(2);cpu->registers[16]=0u;}
        else if((nav&UINT32_C(0x08001008))!=0u){const uint8_t next=UINT8_C(0);status=vf2_model2a_write(machine,UINT32_C(0x005000a5),&next,sizeof(next));instructions=UINT64_C(38);calls=UINT64_C(2);cpu->registers[16]=0u;equal=0;}
        else if((nav&UINT32_C(0x04000104))!=0u){
            const uint8_t next=UINT8_C(4); status=vf2_model2a_read_u32(machine,UINT32_C(0x00500098),&seed);
            if(status==VF2_OK&&seed!=UINT32_C(0xcbf33340))return VF2_ERROR_UNSUPPORTED;
            if(status==VF2_OK)status=phase17_index9_initialize_defaults(machine);
            if(status==VF2_OK)status=write_phase17_index0_text(machine,UINT32_C(27*0x80),UINT32_C(27),"COMPLETED");
            if(status==VF2_OK)status=vf2_model2a_write_u32(machine,UINT32_C(0x00500024),UINT32_C(100));
            if(status==VF2_OK)status=vf2_model2a_write(machine,UINT32_C(0x005000a5),&next,sizeof(next));
            instructions=UINT64_C(54853);calls=UINT64_C(30);
            if(status==VF2_OK)phase17_index7_post(cpu,UINT32_C(9),UINT32_C(1),UINT32_C(0x8180),UINT32_C(0x01000e36),UINT32_C(5),0);
        }else return VF2_ERROR_UNSUPPORTED;
    }else{
        if(nav!=0u)return VF2_ERROR_UNSUPPORTED;
        status=vf2_model2a_read_u32(machine,UINT32_C(0x00500024),&counter);
        if(status!=VF2_OK||counter==0u)return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
        --counter; status=vf2_model2a_write_u32(machine,UINT32_C(0x00500024),counter);
        if(status==VF2_OK&&counter==0u){status=phase17_index9_restore_menu(machine);instructions=UINT64_C(14308);calls=UINT64_C(18);if(status==VF2_OK)phase17_index7_post(cpu,UINT32_C(0x00078cb0),0u,UINT32_C(0xc0a0a3d7),UINT32_C(0x010016ac),UINT32_C(5),0);}
        else if(status==VF2_OK){instructions=UINT64_C(35);calls=UINT64_C(2);phase17_index7_post(cpu,0u,UINT32_C(0x3f4f5c29),UINT32_C(0xc0a0a3d7),UINT32_C(0x010016ac),UINT32_C(5),1);}
    }
    if(status==VF2_OK&&a5!=UINT8_C(1)&&a5!=UINT8_C(4)&&!(a5==UINT8_C(3)&&(nav&UINT32_C(0x04000104))!=0u))status=vf2_model2a_write(machine,UINT32_C(0x005ff602),&spill,sizeof(spill));
    if(status!=VF2_OK)return status;
    cpu->executed_instructions+=instructions;cpu->procedure_calls+=calls;cpu->procedure_returns+=calls;
    status=vf2_i960_cpu_return_procedure(cpu,machine);
    if(status!=VF2_OK||cpu->ip!=UINT32_C(0x0000a010))return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    if(a5<=UINT8_C(3)&&!((a5==UINT8_C(1)&&(nav&(UINT32_C(0x04000104)|UINT32_C(0x200)))!=0u)||(a5==UINT8_C(3)&&(nav&UINT32_C(0x04000104))!=0u))){
        phase17_index7_post(cpu,cpu->registers[16],a5==UINT8_C(0)?UINT32_C(0x3f4f5c29):cpu->registers[17],UINT32_C(0xc0a0a3d7),cpu->registers[25],UINT32_C(5),0);
        if(a5==UINT8_C(0))cpu->registers[16]=UINT32_C(0x00078cb0);
        if(!equal){cpu->arithmetic_control=(cpu->arithmetic_control&~UINT32_C(7))|UINT32_C(4);cpu->compare_result=VF2_I960_COMPARE_LESS;}
    }
    report->kind=VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;report->entry_address=VF2_FRAME_DISPATCH_TICK_ENTRY;report->exit_address=cpu->ip;report->iterations=UINT64_C(1);report->recovered_instruction_count=instructions;report->recovered_procedure_calls=calls;report->recovered_procedure_returns=calls+UINT64_C(1);report->cpu_poststate_applied=1;
    return VF2_OK;
}

'''
s=s.replace(marker,code+marker,1)
old='''        if (phase_index == UINT8_C(0x88)) {\n            return execute_frame_phase17_bit7_index8(\n                machine, cpu, report, phase_index\n            );\n        }'''
if old not in s: raise SystemExit('index8 dispatch missing')
new=old+'''\n        if (phase_index == UINT8_C(0x89)) {\n            return execute_frame_phase17_bit7_index9(\n                machine, cpu, report, phase_index\n            );\n        }'''
s=s.replace(old,new,1)
p.write_text(s)

Path('decomp/i960/notes/selector17_index9_all_initialize.md').write_text('''# Selector 17 / bit-7 index 9: ALL INITIALIZE\n\nROM slot `0x0005fef0` points to entry `0x0005eaa0`. The handler has five states selected by `0x005000a5`: `0x5eac8`, `0x5eb7c`, `0x5eba0`, `0x5ebe8`, and `0x5ecec`.\n\nThe selection UI mirrors BACK UP RAM CLEAR, but uses `YES(INIT.)`. Confirmation performs the full factory-style initialization chain: defaults at `0x3320` and `0x3340`, transfer-table rebuild, backup clear/rebuild, CRCs at backup offsets `0x3300`, `0x3302`, and `0x3304`, and the `VIRTUA FIGHTER 2` signature/version at `0x3306..0x3317`. It then displays `COMPLETED`, arms a 100-frame countdown, and returns to TEST MENU index 9.\n\nNative measurements at the `0x0000a6c0 -> 0x0000a010` frame boundary are: state 0 `711/7/8`; state 1 idle `49/4/5`; state 1 select `51/4/5` (`0x1000`) or `48/4/5` (`0x2000`); state 1 cancel `14313/19/20`; state 2 `43/2/3`; state 3 idle `41/2/3`; state 3 return-to-NO `38/2/3`; destructive confirm `54853/30/31`; state 4 countdown `35/2/3`; terminal countdown/return `14308/18/19`. Counts are instructions/calls/returns.\n\nThe destructive path is accepted only for the measured RNG seed `0xcbf33340`, matching the strict recovery policy already used by index 7.\n''')
