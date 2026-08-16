from pathlib import Path

p = Path('src/recovered/texture_bridge_match.c')
s = p.read_text()
start = s.index('static vf2_status execute_frame_phase17_bit7_index10(')
end = s.index('static vf2_status execute_frame_phase17_bit7_index11(', start)
replacement = r'''static int32_t phase17_index10_round_even(float value)
{
    const double input = (double)value;
    const int64_t truncated = (int64_t)input;
    const double fraction = input - (double)truncated;
    int64_t rounded = truncated;
    if (fraction > 0.5 || (fraction == 0.5 && (truncated & INT64_C(1)) != 0)) ++rounded;
    else if (fraction < -0.5 || (fraction == -0.5 && (truncated & INT64_C(1)) != 0)) --rounded;
    return (int32_t)rounded;
}

static vf2_status phase17_index10_decimal(vf2_model2a *machine,int32_t value,uint32_t destination)
{
    uint32_t magnitude=value<0?(uint32_t)(-(int64_t)value):(uint32_t)value;
    uint16_t tiles[6]; uint32_t i=0u; vf2_status status=VF2_OK;
    tiles[0]=value<0?UINT16_C(0x802d):UINT16_C(0x8020);
    if(magnitude<=UINT32_C(8191)){
        tiles[1]=UINT16_C(0x8020);
        for(i=0u;i<4u;++i){status=read_u16(machine,UINT32_C(0x02040000)+magnitude*UINT32_C(8)+i*UINT32_C(2),&tiles[i+2u]);if(status!=VF2_OK)return status;}
    }else{
        uint32_t n=magnitude; for(i=0u;i<5u;++i)tiles[i+1u]=UINT16_C(0x8020);
        i=5u; do{--i;tiles[i+1u]=(uint16_t)(UINT16_C(0x8030)+(uint16_t)(n%10u));n/=10u;}while(n!=0u&&i!=0u);
    }
    for(i=0u;status==VF2_OK&&i<6u;++i)status=write_u16(machine,destination+i*UINT32_C(2),tiles[i]);
    return status;
}

static vf2_status phase17_index10_tile(vf2_model2a *machine,uint32_t row,uint32_t col,uint16_t tile)
{
    return write_u16(machine,UINT32_C(0x01000000)+row*UINT32_C(0x80)+col*UINT32_C(2),(uint16_t)(UINT16_C(0x8000)|tile));
}

static vf2_status phase17_index10_restore_menu(vf2_model2a *machine)
{
    static const uint32_t extra[3]={UINT32_C(0x0005ff08),UINT32_C(0x0005ff14),UINT32_C(0x0005ff18)};
    const uint8_t phase_index=UINT8_C(10),spill=UINT8_C(0x56);
    uint32_t record=0u,destination=0u,last_source=0u,last_destination=0u;uint64_t chars=0u;size_t i=0u;
    vf2_status status=clear_tile_plane_64x48(machine,UINT32_C(0x01000000));
    if(status==VF2_OK)status=vf2_model2a_write(machine,UINT32_C(0x005000a4),&phase_index,sizeof(phase_index));
    if(status==VF2_OK)status=vf2_model2a_read_u32(machine,UINT32_C(0x0005feac)+UINT32_C(80),&record);
    if(status==VF2_OK)status=vf2_model2a_read_u32(machine,record,&destination);
    if(status==VF2_OK&&destination>=UINT32_C(4))status=write_u16(machine,destination-UINT32_C(4),UINT16_C(0x801c));else if(status==VF2_OK)status=VF2_ERROR_UNSUPPORTED;
    for(i=0u;status==VF2_OK&&i<12u;++i){status=vf2_model2a_read_u32(machine,UINT32_C(0x0005feac)+(uint32_t)i*UINT32_C(8),&record);if(status==VF2_OK)status=phase16_copy_text_record(machine,record,&last_source,&last_destination,&chars);}
    for(i=0u;status==VF2_OK&&i<3u;++i){status=vf2_model2a_read_u32(machine,extra[i],&record);if(status==VF2_OK)status=phase16_copy_text_record(machine,record,&last_source,&last_destination,&chars);}
    if(status==VF2_OK)status=vf2_model2a_write(machine,UINT32_C(0x005ff602),&spill,sizeof(spill));
    return status;
}

static void phase17_index10_post(vf2_i960_cpu *cpu,int exiting)
{
    cpu->registers[0]=0u;cpu->registers[1]=UINT32_C(0x005ff580);cpu->registers[2]=UINT32_C(0x0000a010);cpu->registers[3]=0u;cpu->registers[4]=UINT32_C(0x00515400);cpu->registers[5]=UINT32_C(0x3f800000);cpu->registers[6]=0u;cpu->registers[7]=0u;
    cpu->registers[8]=UINT32_MAX;cpu->registers[9]=UINT32_MAX;cpu->registers[10]=UINT32_MAX;cpu->registers[11]=UINT32_MAX;cpu->registers[12]=0u;cpu->registers[13]=0u;cpu->registers[14]=UINT32_C(5);cpu->registers[15]=UINT32_C(0x00008a00);
    cpu->registers[16]=exiting?UINT32_C(0x00078cb0):UINT32_C(0x12);cpu->registers[17]=exiting?0u:UINT32_C(0x005002a8);cpu->registers[18]=UINT32_C(0x005002a8);cpu->registers[19]=UINT32_C(0x00500270);cpu->registers[20]=UINT32_C(0x00560000);cpu->registers[21]=UINT32_C(1);cpu->registers[22]=0u;cpu->registers[23]=UINT32_C(0x00510980);cpu->registers[24]=UINT32_C(0x00512980);cpu->registers[25]=exiting?UINT32_C(0x010016ac):UINT32_C(0x010012fa);cpu->registers[26]=UINT32_C(0x00800000);cpu->registers[27]=UINT32_C(0x00880000);cpu->registers[28]=UINT32_C(0x00004000);cpu->registers[29]=UINT32_C(0x00516480);cpu->registers[30]=UINT32_C(0x00000220);cpu->registers[31]=UINT32_C(0x005ff500);set_equal_condition(cpu);
}

static vf2_status phase17_index10_render_state0(vf2_model2a *machine)
{
    static const char *abbr[11]={"AKI","JAC","SAR","KAG","LAU","JEF","PAI","WOL","SHU","DUR","LIO"};
    static const char *names[11]={"AKIRA","JACKY","SARAH","KAGE","LAU","JEFFRY","PAI","WOLF","SHUN","DURAL","LION"};
    static const uint16_t markers[6]={UINT16_C(0x02c1),UINT16_C(0x02d6),UINT16_C(0x02c7),UINT16_C(0x02d2),UINT16_C(0x02ce),UINT16_C(0x02cb)};
    static const uint8_t marker_cols[6]={54u,55u,56u,58u,59u,60u};
    size_t i=0u;vf2_status status=write_phase17_index0_text(machine,UINT32_C(13*0x80),UINT32_C(10),"LOSES(%)");
    for(i=0u;status==VF2_OK&&i<6u;++i)status=phase17_index10_tile(machine,UINT32_C(16),marker_cols[i],markers[i]);
    for(i=0u;status==VF2_OK&&i<11u;++i)status=write_phase17_index0_text(machine,UINT32_C(16*0x80),UINT32_C(10)+(uint32_t)i*UINT32_C(4),abbr[i]);
    if(status==VF2_OK)status=write_phase17_index0_text(machine,UINT32_C(14*0x80),UINT32_C(2),"WIN(%)");
    for(i=0u;status==VF2_OK&&i<11u;++i)status=write_phase17_index0_text(machine,(UINT32_C(18)+(uint32_t)i*UINT32_C(2))*UINT32_C(0x80),UINT32_C(2),names[i]);
    if(status==VF2_OK)status=write_phase17_index0_text(machine,UINT32_C(45*0x80),UINT32_C(19),"PUSH TEST BUTTON TO EXIT.");
    return status;
}

static vf2_status phase17_index10_render_state1(vf2_model2a *machine)
{
    static const uint8_t vertical_cols[15]={1u,9u,13u,17u,21u,25u,29u,33u,37u,41u,45u,49u,53u,57u,61u};
    static const uint8_t horizontal_rows[11]={17u,19u,21u,23u,25u,27u,29u,31u,33u,35u,37u};
    static const uint8_t horizontal_tiles[15]={19u,21u,21u,21u,21u,21u,21u,21u,15u,21u,21u,21u,15u,21u,21u};
    uint32_t base=0u;uint32_t fighters[11],scores[11];uint32_t i=0u,j=0u;vf2_status status=vf2_model2a_read_u32(machine,UINT32_C(0x0050016c),&base);
    for(i=0u;status==VF2_OK&&i<11u;++i){
        for(j=0u;status==VF2_OK&&j<11u;++j){
            uint16_t a=0u,b=0u;uint32_t sum=0u;int32_t value=0;uint32_t row=UINT32_C(18)+j*UINT32_C(2);uint32_t col=UINT32_C(9)+i*UINT32_C(4);uint32_t dst=UINT32_C(0x01000000)+row*UINT32_C(0x80)+col*UINT32_C(2);
            status=read_u16(machine,base+UINT32_C(0x36a4)+j*UINT32_C(17)+i*UINT32_C(4),&a);if(status==VF2_OK)status=read_u16(machine,base+UINT32_C(0x36a6)+j*UINT32_C(17)+i*UINT32_C(4),&b);if(status!=VF2_OK)break;
            if(a!=0u){sum=(uint32_t)a+(uint32_t)b;value=phase17_index10_round_even(((float)a/(float)sum)*10000.0f);status=phase17_index10_decimal(machine,value,dst);}
            else{sum=(uint32_t)a+(uint32_t)b;status=phase17_index10_decimal(machine,0,dst);if(status==VF2_OK){if(sum!=0u)status=phase17_index10_tile(machine,row,col+UINT32_C(2),UINT16_C(0x30));else status=phase17_index10_decimal(machine,-1,dst+UINT32_C(4));}}
        }
    }
    for(i=0u;status==VF2_OK&&i<12u;++i)status=write_phase17_index0_text(machine,(UINT32_C(18)+i*UINT32_C(2))*UINT32_C(0x80),UINT32_C(53),"        ");
    for(i=0u;status==VF2_OK&&i<11u;++i){
        uint32_t total_a=0u,total_b=0u;int32_t value=0;uint32_t row=UINT32_C(18)+i*UINT32_C(2);uint32_t dst=UINT32_C(0x01000000)+row*UINT32_C(0x80)+UINT32_C(53*2);
        for(j=0u;status==VF2_OK&&j<11u;++j){uint16_t a=0u,b=0u;status=read_u16(machine,base+UINT32_C(0x36a4)+j*UINT32_C(17)+i*UINT32_C(4),&a);if(status==VF2_OK)status=read_u16(machine,base+UINT32_C(0x36a6)+j*UINT32_C(17)+i*UINT32_C(4),&b);total_a+=(uint32_t)a;total_b+=(uint32_t)b;}
        if(status!=VF2_OK)break;
        if(total_a+total_b!=0u){value=phase17_index10_round_even(((float)total_b/(float)(total_a+total_b))*10000.0f);status=phase17_index10_decimal(machine,value,dst);}else{status=phase17_index10_decimal(machine,0,dst);if(status==VF2_OK)status=phase17_index10_decimal(machine,-1,dst+UINT32_C(4));value=-1;}
        fighters[i]=i;scores[i]=(uint32_t)value;if(status==VF2_OK)status=vf2_model2a_write_u32(machine,UINT32_C(0x00500280)+i*UINT32_C(4),(uint32_t)value);if(status==VF2_OK)status=vf2_model2a_write_u32(machine,UINT32_C(0x00500244)+i*UINT32_C(4),i);
    }
    for(i=1u;status==VF2_OK&&i<11u;++i)for(j=i;status==VF2_OK&&j<11u;++j)if((int32_t)scores[i-1u]>(int32_t)scores[j]){uint32_t t=scores[i-1u];scores[i-1u]=scores[j];scores[j]=t;t=fighters[i-1u];fighters[i-1u]=fighters[j];fighters[j]=t;status=vf2_model2a_write_u32(machine,UINT32_C(0x00500280)+(i-1u)*UINT32_C(4),scores[i-1u]);if(status==VF2_OK)status=vf2_model2a_write_u32(machine,UINT32_C(0x00500280)+j*UINT32_C(4),scores[j]);if(status==VF2_OK)status=vf2_model2a_write_u32(machine,UINT32_C(0x00500244)+(i-1u)*UINT32_C(4),fighters[i-1u]);if(status==VF2_OK)status=vf2_model2a_write_u32(machine,UINT32_C(0x00500244)+j*UINT32_C(4),fighters[j]);}
    for(i=0u;status==VF2_OK&&i<11u;++i){uint32_t row=UINT32_C(18)+fighters[i]*UINT32_C(2);status=phase17_index10_decimal(machine,(int32_t)((i+UINT32_C(1))*UINT32_C(1000)),UINT32_C(0x01000000)+row*UINT32_C(0x80)+UINT32_C(57*2));}
    for(i=0u;status==VF2_OK&&i<11u;++i)status=write_phase17_index0_text(machine,(UINT32_C(18)+i*UINT32_C(2))*UINT32_C(0x80),UINT32_C(60),"    ");
    if(status==VF2_OK)status=phase17_index10_tile(machine,UINT32_C(15),UINT32_C(1),UINT16_C(24));if(status==VF2_OK)status=phase17_index10_tile(machine,UINT32_C(15),UINT32_C(61),UINT16_C(25));
    for(i=2u;status==VF2_OK&&i<61u;++i)status=phase17_index10_tile(machine,UINT32_C(15),i,UINT16_C(21));
    if(status==VF2_OK)status=phase17_index10_tile(machine,UINT32_C(39),UINT32_C(1),UINT16_C(26));if(status==VF2_OK)status=phase17_index10_tile(machine,UINT32_C(39),UINT32_C(61),UINT16_C(27));
    for(i=2u;status==VF2_OK&&i<61u;++i)status=phase17_index10_tile(machine,UINT32_C(39),i,UINT16_C(21));
    for(i=0u;status==VF2_OK&&i<15u;++i)for(j=16u;status==VF2_OK&&j<39u;++j)status=phase17_index10_tile(machine,j,vertical_cols[i],UINT16_C(22));
    for(i=0u;status==VF2_OK&&i<11u;++i)for(j=1u;status==VF2_OK&&j<62u;++j)status=phase17_index10_tile(machine,horizontal_rows[i],j,horizontal_tiles[(j-1u)/UINT32_C(4) < 15u ? (j-1u)/UINT32_C(4) : 14u]);
    return status;
}

static vf2_status execute_frame_phase17_bit7_index10(
    vf2_model2a *machine,vf2_i960_cpu *cpu,vf2_hybrid_bridge_report *report,uint8_t flagged_phase_index)
{
    const uint32_t base_input=UINT32_C(0x0ff7f700);const uint8_t spill=UINT8_C(0x56);
    uint32_t target=0u,input=0u,navigation=0u,released=0u,previous=0u,mask=0u;uint8_t a5=0u,a6=0u,a7=0u;uint64_t instructions=0u,calls=0u;vf2_status status=VF2_OK;int exiting=0;
    if(flagged_phase_index!=UINT8_C(0x8a)||cpu->local_frame_depth==0u)return VF2_ERROR_UNSUPPORTED;
    status=vf2_model2a_read_u32(machine,UINT32_C(0x0005fef8),&target);if(status==VF2_OK)status=vf2_model2a_read_u32(machine,UINT32_C(0x00500700),&input);if(status==VF2_OK)status=vf2_model2a_read_u32(machine,UINT32_C(0x00500704),&navigation);if(status==VF2_OK)status=vf2_model2a_read_u32(machine,UINT32_C(0x00500708),&released);if(status==VF2_OK)status=vf2_model2a_read_u32(machine,UINT32_C(0x0050070c),&previous);if(status==VF2_OK)status=vf2_model2a_read_u32(machine,UINT32_C(0x0050002c),&mask);if(status==VF2_OK)status=vf2_model2a_read(machine,UINT32_C(0x005000a5),&a5,1u);if(status==VF2_OK)status=vf2_model2a_read(machine,UINT32_C(0x005000a6),&a6,1u);if(status==VF2_OK)status=vf2_model2a_read(machine,UINT32_C(0x005000a7),&a7,1u);
    if(status!=VF2_OK||target!=UINT32_C(0x0005f234)||input!=base_input||released!=0u||previous!=base_input||mask!=UINT32_C(0x00020000)||a5>UINT8_C(1)||a6!=UINT8_C(0xff)||a7!=UINT8_C(0xff))return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    if(a5==UINT8_C(0)){
        const uint8_t next=UINT8_C(1);if(navigation!=0u)return VF2_ERROR_UNSUPPORTED;status=phase17_index10_render_state0(machine);if(status==VF2_OK)status=vf2_model2a_write(machine,UINT32_C(0x005000a5),&next,sizeof(next));instructions=UINT64_C(1650);calls=UINT64_C(31);
    }else{
        if(navigation!=0u&&(navigation&UINT32_C(0x04000104))==0u)return VF2_ERROR_UNSUPPORTED;status=phase17_index10_render_state1(machine);if(status==VF2_OK&&(navigation&UINT32_C(0x04000104))!=0u){status=phase17_index10_restore_menu(machine);exiting=1;instructions=UINT64_C(51001);calls=UINT64_C(1452);}else if(status==VF2_OK){instructions=UINT64_C(36729);calls=UINT64_C(1436);}
    }
    if(status==VF2_OK&&!exiting)status=vf2_model2a_write(machine,UINT32_C(0x005ff602),&spill,sizeof(spill));if(status!=VF2_OK)return status;
    cpu->executed_instructions+=instructions;cpu->procedure_calls+=calls;cpu->procedure_returns+=calls;status=vf2_i960_cpu_return_procedure(cpu,machine);if(status!=VF2_OK||cpu->ip!=UINT32_C(0x0000a010))return status==VF2_OK?VF2_ERROR_UNSUPPORTED:status;
    if(a5==UINT8_C(0)){phase17_index7_post(cpu,UINT32_C(0x2e),UINT32_C(0x3f4f5c29),UINT32_C(0xc0a0a3d7),UINT32_C(0x01001726),UINT32_C(5),0);cpu->arithmetic_control=(cpu->arithmetic_control&~UINT32_C(7))|UINT32_C(1);cpu->compare_result=VF2_I960_COMPARE_GREATER;}else phase17_index10_post(cpu,exiting);
    report->kind=VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;report->entry_address=VF2_FRAME_DISPATCH_TICK_ENTRY;report->exit_address=cpu->ip;report->iterations=UINT64_C(1);report->recovered_instruction_count=instructions;report->recovered_procedure_calls=calls;report->recovered_procedure_returns=calls+UINT64_C(1);report->cpu_poststate_applied=1;return VF2_OK;
}

'''
p.write_text(s[:start] + replacement + s[end:])

note = Path('decomp/i960/notes/selector17_index10_vs_diagram.md')
note.write_text('''# Selector17 bit-7 index 10: VS DIAGRAM\n\nROM slot `0x0005fef8` selects entry `0x0005f234` (`a4 = 0x8a`). The handler has two states.\n\n## State 0\n\nBuilds the static VS diagram: LOSS/WIN headings, eleven fighter abbreviations and names, border glyphs, and the TEST-button exit prompt, then advances `a5` to 1. Measured handler corridor: 1,650 instructions, 31 calls, 32 returns.\n\n## State 1\n\nThe live body iterates the 11 x 11 matchup table rooted at `base + 0x36a4`. For each pair it reads the two 16-bit counters, computes the displayed percentage with the i960 floating conversion/divide/multiply/round sequence, and renders it with `0x7ff0`. It then sums each fighter row, stores totals at `0x00500280` and fighter ids at `0x00500244`, sorts those parallel arrays ascending by the computed score, renders rank values 1000..11000, and rebuilds the diagram borders. `0x8440` is the one-tile primitive `*(u16*)g9 = 0x8000 | g0`.\n\nMeasured handler corridors: normal refresh 36,729 instructions / 1,436 calls / 1,437 returns; TEST exit 51,001 / 1,452 / 1,453. The TEST check is at the end of the refresh, so exit still recomputes/redraws the full diagram before shared teardown at `0x5f140`.\n\nThe recovered bridge now implements both states, including the parallel-array sort, numeric formatting, border construction, parent-menu restoration, and measured CPU post-state.\n''')
