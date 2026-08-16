from pathlib import Path

p=Path('src/recovered/texture_bridge_match.c')
s=p.read_text()
a=s.index('static vf2_status execute_frame_phase17_bit7_index7(')
b=s.index('static vf2_status execute_frame_phase17_bit7_index11(',a)
new=r'''static vf2_status phase17_index7_zero(vf2_model2a *m,uint32_t a,size_t n){
    static const uint8_t z[256]={0};
    vf2_status st=VF2_OK;
    while(st==VF2_OK&&n){size_t k=n>sizeof(z)?sizeof(z):n;st=vf2_model2a_write(m,a,z,k);a+=(uint32_t)k;n-=k;}
    return st;
}

static vf2_status phase17_index7_rng(vf2_model2a *m,uint32_t *value){
    uint32_t seed=0u,x=0u,base=0u;vf2_status st=vf2_model2a_read_u32(m,UINT32_C(0x00500098),&seed);size_t i=0u;
    base=seed<<20u;
    for(i=0u;st==VF2_OK&&i<4u;++i){st=vf2_model2a_read_u32(m,base+(uint32_t)i*4u,&x);if(st==VF2_OK)seed+=x<<(4u+(uint32_t)i*4u);}
    if(st==VF2_OK)st=vf2_model2a_write_u32(m,UINT32_C(0x00500098),seed);
    if(st==VF2_OK&&value)*value=(seed>>4u)&UINT32_C(0xffff);
    return st;
}

static vf2_status phase17_index7_rebuild_backup(vf2_model2a *m){
    vf2_status st=phase17_index7_zero(m,UINT32_C(0x01d03100),UINT32_C(0x200));
    uint32_t used=0u,row=0u,idx=0u,rnd=0u,ptr=0u,name=0u,meta=0u;uint8_t klass=0u;
    for(row=0u;st==VF2_OK&&row<18u;++row){
        if(row<6u)idx=row;else do{st=phase17_index7_rng(m,&rnd);idx=rnd&31u;}while(st==VF2_OK&&(used&(UINT32_C(1)<<idx))!=0u);
        used|=UINT32_C(1)<<idx;
        if(st==VF2_OK)st=vf2_model2a_read_u32(m,UINT32_C(0x0201eafc)+idx*4u,&ptr);
        if(st==VF2_OK)st=vf2_model2a_read_u32(m,ptr,&name);
        if(st==VF2_OK)st=vf2_model2a_read_u32(m,UINT32_C(0x0201ea24)+row*12u,&meta);
        if(st==VF2_OK)st=vf2_model2a_read(m,UINT32_C(0x0201ea2a)+row*12u,&klass,1u);
        if(st==VF2_OK)st=vf2_model2a_write_u32(m,UINT32_C(0x01d03100)+row*16u,meta);
        if(st==VF2_OK)st=vf2_model2a_write_u32(m,UINT32_C(0x01d03104)+row*16u,name);
        if(st==VF2_OK)st=vf2_model2a_write(m,UINT32_C(0x01d0310a)+row*16u,&klass,1u);
    }
    return st;
}

static vf2_status phase17_index7_clear_backup(vf2_model2a *m){
    static const uint32_t offs[]={UINT32_C(0x3318),UINT32_C(0x331c),UINT32_C(0x3390),UINT32_C(0x3394),UINT32_C(0x3398),UINT32_C(0x339c),UINT32_C(0x3380),UINT32_C(0x3384),UINT32_C(0x3388),UINT32_C(0x338c)};
    vf2_status st=VF2_OK;size_t i=0u;uint32_t base=0u,crc=0u,j=0u;uint8_t byte=0u,t[2]={0u,0u};
    for(i=0u;st==VF2_OK&&i<sizeof(offs)/sizeof(offs[0]);++i){st=vf2_model2a_write_u32(m,UINT32_C(0x01d00000)+offs[i],0u);if(st==VF2_OK)st=vf2_model2a_write_u32(m,UINT32_C(0x00599000)+offs[i],0u);}
    if(st==VF2_OK)st=phase17_index7_zero(m,UINT32_C(0x01d03380),UINT32_C(0xc80));
    if(st==VF2_OK)st=phase17_index7_zero(m,UINT32_C(0x0059c380),UINT32_C(0xc60));
    if(st==VF2_OK)st=vf2_model2a_write(m,UINT32_C(0x01d03000),t,1u);
    if(st==VF2_OK)st=vf2_model2a_write(m,UINT32_C(0x0059c000),t,1u);
    if(st==VF2_OK)st=phase17_index7_rebuild_backup(m);
    if(st==VF2_OK)st=vf2_model2a_read_u32(m,UINT32_C(0x0050016c),&base);
    for(j=0u;st==VF2_OK&&j<UINT32_C(0x644);++j){
        st=vf2_model2a_read(m,base+UINT32_C(0x33a8)+j,&byte,1u);
        if(st==VF2_OK){uint32_t k=((crc>>8u)^byte)&255u;uint16_t tab=0u;st=vf2_model2a_read(m,UINT32_C(0x02000000)+k*2u,&tab,2u);if(st==VF2_OK)crc=((crc<<8u)&UINT32_C(0xff00))^(uint32_t)tab;}
    }
    if(st==VF2_OK){uint16_t c=(uint16_t)crc;st=vf2_model2a_write(m,UINT32_C(0x01d03304),&c,2u);}
    return st;
}

static vf2_status phase17_index7_restore_menu(vf2_model2a *m){
    static const uint32_t extra[3]={UINT32_C(0x0005ff08),UINT32_C(0x0005ff14),UINT32_C(0x0005ff18)};
    uint8_t a4=UINT8_C(7),spill=UINT8_C(0x56);uint32_t rec=0u,dst=0u,ls=0u,ld=0u;uint64_t chars=0u;size_t i=0u;vf2_status st=clear_tile_plane_64x48(m,UINT32_C(0x01000000));
    if(st==VF2_OK)st=vf2_model2a_write(m,UINT32_C(0x005000a4),&a4,1u);
    if(st==VF2_OK)st=vf2_model2a_read_u32(m,UINT32_C(0x0005feac)+UINT32_C(56),&rec);
    if(st==VF2_OK)st=vf2_model2a_read_u32(m,rec,&dst);
    if(st==VF2_OK&&dst>=4u)st=write_u16(m,dst-4u,UINT16_C(0x801c));else if(st==VF2_OK)st=VF2_ERROR_UNSUPPORTED;
    for(i=0u;st==VF2_OK&&i<12u;++i){st=vf2_model2a_read_u32(m,UINT32_C(0x0005feac)+(uint32_t)i*8u,&rec);if(st==VF2_OK)st=phase16_copy_text_record(m,rec,&ls,&ld,&chars);}
    for(i=0u;st==VF2_OK&&i<3u;++i){st=vf2_model2a_read_u32(m,extra[i],&rec);if(st==VF2_OK)st=phase16_copy_text_record(m,rec,&ls,&ld,&chars);}
    if(st==VF2_OK)st=vf2_model2a_write(m,UINT32_C(0x005ff602),&spill,1u);
    return st;
}

static void phase17_index7_post(vf2_i960_cpu *c,uint32_t g0,uint32_t g1,uint32_t g2,uint32_t g9,uint32_t r14,int less){
    c->registers[0]=0u;c->registers[1]=UINT32_C(0x005ff580);c->registers[2]=UINT32_C(0x0000a010);c->registers[3]=0u;c->registers[4]=UINT32_C(0x00515400);c->registers[5]=UINT32_C(0x3f800000);c->registers[6]=0u;c->registers[7]=0u;
    c->registers[8]=UINT32_MAX;c->registers[9]=UINT32_MAX;c->registers[10]=UINT32_MAX;c->registers[11]=UINT32_MAX;c->registers[12]=0u;c->registers[13]=0u;c->registers[14]=r14;c->registers[15]=UINT32_C(0x00008a00);
    c->registers[16]=g0;c->registers[17]=g1;c->registers[18]=g2;c->registers[19]=0u;c->registers[20]=UINT32_C(0x00560000);c->registers[21]=UINT32_C(0x0050e850);c->registers[22]=UINT32_C(0x000055b6);c->registers[23]=UINT32_C(0x00510980);c->registers[24]=UINT32_C(0x00512980);c->registers[25]=g9;c->registers[26]=UINT32_C(0x00800000);c->registers[27]=UINT32_C(0x00880000);c->registers[28]=UINT32_C(0x00004000);c->registers[29]=UINT32_C(0x00516480);c->registers[30]=UINT32_C(0x00000220);c->registers[31]=UINT32_C(0x005ff500);
    if(less){c->arithmetic_control=(c->arithmetic_control&~UINT32_C(7))|UINT32_C(4);c->compare_result=VF2_I960_COMPARE_LESS;}else set_equal_condition(c);
}

static vf2_status execute_frame_phase17_bit7_index7(vf2_model2a *machine,vf2_i960_cpu *cpu,vf2_hybrid_bridge_report *report,uint8_t flagged_phase_index){
    const uint32_t base_input=UINT32_C(0x0ff7f700);uint32_t target=0u,in=0u,nav=0u,rel=0u,prev=0u,mask=0u,seed=0u,counter=0u,rec=0u,dst=0u;uint8_t a5=0u,a6=0u,a7=0u,spill=UINT8_C(0x56);uint64_t ins=0u,calls=0u,chars=0u;int equal=1;vf2_status st=VF2_OK;
    if(flagged_phase_index!=UINT8_C(0x87)||cpu->local_frame_depth==0u)return VF2_ERROR_UNSUPPORTED;
    st=vf2_model2a_read_u32(machine,UINT32_C(0x0005fee0),&target);if(st==VF2_OK)st=vf2_model2a_read_u32(machine,UINT32_C(0x00500700),&in);if(st==VF2_OK)st=vf2_model2a_read_u32(machine,UINT32_C(0x00500704),&nav);if(st==VF2_OK)st=vf2_model2a_read_u32(machine,UINT32_C(0x00500708),&rel);if(st==VF2_OK)st=vf2_model2a_read_u32(machine,UINT32_C(0x0050070c),&prev);if(st==VF2_OK)st=vf2_model2a_read_u32(machine,UINT32_C(0x0050002c),&mask);if(st==VF2_OK)st=vf2_model2a_read(machine,UINT32_C(0x005000a5),&a5,1u);if(st==VF2_OK)st=vf2_model2a_read(machine,UINT32_C(0x005000a6),&a6,1u);if(st==VF2_OK)st=vf2_model2a_read(machine,UINT32_C(0x005000a7),&a7,1u);
    if(st!=VF2_OK||target!=UINT32_C(0x0005e848)||in!=base_input||rel!=0u||prev!=base_input||mask!=UINT32_C(0x00020000)||a5>UINT8_C(4)||a6!=UINT8_C(0xff)||a7!=UINT8_C(0xff))return st==VF2_OK?VF2_ERROR_UNSUPPORTED:st;
    if(a5==0u){uint8_t n=1u;if(nav!=0u)return VF2_ERROR_UNSUPPORTED;st=phase17_index7_render_choices(machine,0,&chars);if(st==VF2_OK)st=vf2_model2a_write(machine,UINT32_C(0x005000a5),&n,1u);ins=711u;calls=7u;}
    else if(a5==1u){if((nav&(UINT32_C(0x04000104)|UINT32_C(0x200)))!=0u){st=phase17_index7_restore_menu(machine);ins=14313u;calls=19u;if(st==VF2_OK)phase17_index7_post(cpu,UINT32_C(0x00078cb0),0u,UINT32_C(0xc0a0a3d7),UINT32_C(0x010016ac),5u,0);}
        else if(nav==0u){ins=49u;calls=4u;cpu->registers[16]=0u;}else if(nav==UINT32_C(0x1000)||nav==UINT32_C(0x2000)){uint8_t n=2u;st=vf2_model2a_write(machine,UINT32_C(0x005000a5),&n,1u);ins=nav==UINT32_C(0x1000)?51u:48u;calls=4u;cpu->registers[16]=nav==UINT32_C(0x1000)?1u:UINT32_MAX;equal=0;}else return VF2_ERROR_UNSUPPORTED;}
    else if(a5==2u){uint8_t n=3u;if(nav!=0u)return VF2_ERROR_UNSUPPORTED;st=vf2_model2a_read_u32(machine,UINT32_C(0x0005ff24),&rec);if(st==VF2_OK)st=vf2_model2a_read_u32(machine,rec,&dst);if(st==VF2_OK&&dst>=4u)st=write_u16(machine,dst-4u,UINT16_C(0x8020));else if(st==VF2_OK)st=VF2_ERROR_UNSUPPORTED;if(st==VF2_OK)st=vf2_model2a_read_u32(machine,UINT32_C(0x0005ff20),&rec);if(st==VF2_OK)st=vf2_model2a_read_u32(machine,rec,&dst);if(st==VF2_OK&&dst>=4u)st=write_u16(machine,dst-4u,UINT16_C(0x801c));else if(st==VF2_OK)st=VF2_ERROR_UNSUPPORTED;if(st==VF2_OK)st=vf2_model2a_write(machine,UINT32_C(0x005000a5),&n,1u);ins=43u;calls=2u;cpu->registers[16]=0u;cpu->registers[25]=UINT32_C(0x01000bb0);}
    else if(a5==3u){if(nav==0u){ins=41u;calls=2u;cpu->registers[16]=0u;}else if((nav&UINT32_C(0x08001008))!=0u){uint8_t n=0u;st=vf2_model2a_write(machine,UINT32_C(0x005000a5),&n,1u);ins=38u;calls=2u;cpu->registers[16]=0u;equal=0;}else if((nav&UINT32_C(0x04000104))!=0u){uint8_t n=4u;st=vf2_model2a_read_u32(machine,UINT32_C(0x00500098),&seed);if(st==VF2_OK&&seed!=UINT32_C(0xcbf33340))return VF2_ERROR_UNSUPPORTED;if(st==VF2_OK)st=phase17_index7_clear_backup(machine);if(st==VF2_OK)st=write_phase17_index0_text(machine,UINT32_C(27*0x80),UINT32_C(27),"COMPLETED");if(st==VF2_OK)st=vf2_model2a_write_u32(machine,UINT32_C(0x00500024),UINT32_C(100));if(st==VF2_OK)st=vf2_model2a_write(machine,UINT32_C(0x005000a5),&n,1u);ins=40712u;calls=21u;if(st==VF2_OK)phase17_index7_post(cpu,9u,1u,UINT32_C(0x8180),UINT32_C(0x01000e36),5u,0);}else return VF2_ERROR_UNSUPPORTED;}
    else {if(nav!=0u)return VF2_ERROR_UNSUPPORTED;st=vf2_model2a_read_u32(machine,UINT32_C(0x00500024),&counter);if(st!=VF2_OK||counter==0u)return st==VF2_OK?VF2_ERROR_UNSUPPORTED:st;--counter;st=vf2_model2a_write_u32(machine,UINT32_C(0x00500024),counter);if(st==VF2_OK&&counter==0u){st=phase17_index7_restore_menu(machine);ins=14308u;calls=18u;if(st==VF2_OK)phase17_index7_post(cpu,UINT32_C(0x00078cb0),0u,UINT32_C(0xc0a0a3d7),UINT32_C(0x010016ac),6u,0);}else if(st==VF2_OK){ins=35u;calls=2u;phase17_index7_post(cpu,0u,UINT32_C(0x3f4f5c29),UINT32_C(0xc0a0a3d7),UINT32_C(0x01000e36),6u,1);}}
    if(st==VF2_OK&&a5!=1u&&a5!=4u&&!(a5==3u&&(nav&UINT32_C(0x04000104))!=0u))st=vf2_model2a_write(machine,UINT32_C(0x005ff602),&spill,1u);
    if(st!=VF2_OK)return st;
    cpu->executed_instructions+=ins;cpu->procedure_calls+=calls;cpu->procedure_returns+=calls;st=vf2_i960_cpu_return_procedure(cpu,machine);if(st!=VF2_OK||cpu->ip!=UINT32_C(0x0000a010))return st==VF2_OK?VF2_ERROR_UNSUPPORTED:st;
    if(a5<=3u&&!((a5==1u&&(nav&(UINT32_C(0x04000104)|UINT32_C(0x200)))!=0u)||(a5==3u&&(nav&UINT32_C(0x04000104))!=0u))){cpu->registers[0]=0u;cpu->registers[1]=UINT32_C(0x005ff580);cpu->registers[2]=UINT32_C(0x0000a010);cpu->registers[3]=0u;cpu->registers[4]=UINT32_C(0x00515400);cpu->registers[5]=UINT32_C(0x3f800000);cpu->registers[6]=0u;cpu->registers[7]=0u;cpu->registers[8]=UINT32_MAX;cpu->registers[9]=UINT32_MAX;cpu->registers[10]=UINT32_MAX;cpu->registers[11]=UINT32_MAX;cpu->registers[12]=0u;cpu->registers[13]=0u;cpu->registers[14]=5u;cpu->registers[15]=UINT32_C(0x00008a00);cpu->registers[17]=0u;cpu->registers[18]=UINT32_C(0xc0a0a3d7);cpu->registers[19]=0u;cpu->registers[20]=UINT32_C(0x00560000);cpu->registers[21]=UINT32_C(0x0050e850);cpu->registers[22]=UINT32_C(0x000055b6);cpu->registers[23]=UINT32_C(0x00510980);cpu->registers[24]=UINT32_C(0x00512980);cpu->registers[26]=UINT32_C(0x00800000);cpu->registers[27]=UINT32_C(0x00880000);cpu->registers[28]=UINT32_C(0x00004000);cpu->registers[29]=UINT32_C(0x00516480);cpu->registers[30]=UINT32_C(0x00000220);cpu->registers[31]=UINT32_C(0x005ff500);if(equal)set_equal_condition(cpu);}
    report->kind=VF2_HYBRID_BRIDGE_FRAME_DISPATCH_TICK;report->entry_address=VF2_FRAME_DISPATCH_TICK_ENTRY;report->exit_address=cpu->ip;report->iterations=1u;report->recovered_instruction_count=ins;report->recovered_procedure_calls=calls;report->recovered_procedure_returns=calls+1u;report->diagnostic_text_copies=chars;report->cpu_poststate_applied=1;return VF2_OK;
}

'''
s=s[:a]+new+s[b:]
p.write_text(s)

n=Path('decomp/i960/notes/selector17_index7_backup_ram_clear.md')
t=n.read_text()
t += '''\n\n## Destructive confirmation and completion\n\nThe `0x04000104` confirmation from state 3 is now recovered for the measured live RNG seed `0xcbf33340`. `0x6001c` clears both backup bookkeeping windows, `0x5427c` rebuilds eighteen 16-byte records (six fixed selections plus twelve unique RNG-selected names), and `0x5ff7c` reproduces the table-driven 0x644-byte checksum. The measured confirmation frame is 40,712 instructions / 21 calls / 22 returns, leaves RNG seed `0xd7925570`, sets state 4 and countdown 100, and draws `COMPLETED`.\n\nState 4 idle decrements the countdown at 35 / 2 / 3. Its terminal 1 -> 0 path restores the TEST MENU at 14,308 / 18 / 19. State-1 cancellation through `0x60b84` restores the same menu at 14,313 / 19 / 20. The destructive branch deliberately rejects other RNG seeds until independently measured; the algorithm itself is ported rather than replaced with a snapshot dump.\n'''
n.write_text(t)
