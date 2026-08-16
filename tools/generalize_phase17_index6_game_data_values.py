from pathlib import Path

p=Path('src/recovered/texture_bridge_match.c')
s=p.read_text()
start=s.index('static vf2_status phase17_index6_render_game_data_zero(')
end=s.index('static vf2_status phase17_index6_finish_game_data_pair(',start)
old=s[start:end]
new=r'''static vf2_status phase17_index6_write_total_time(
    vf2_model2a *machine, uint64_t ticks, uint32_t destination
)
{
    const uint64_t seconds=ticks>>6u;
    const uint64_t days=seconds/UINT64_C(86400);
    const uint64_t hours=(seconds/UINT64_C(3600))%UINT64_C(24);
    const uint64_t minutes=(seconds/UINT64_C(60))%UINT64_C(60);
    const uint64_t secs=seconds%UINT64_C(60);
    char text[32];
    (void)snprintf(text,sizeof(text),"%6lluD%3lluH%3lluM%3lluS",
        (unsigned long long)days,(unsigned long long)hours,
        (unsigned long long)minutes,(unsigned long long)secs);
    return write_phase17_index0_text(machine,destination-UINT32_C(0x01000000),UINT32_C(0),text);
}

static vf2_status phase17_index6_write_minute_time(
    vf2_model2a *machine, uint64_t ticks, uint32_t destination
)
{
    const uint64_t seconds=ticks>>6u;
    const uint64_t minutes=seconds/UINT64_C(60);
    const uint64_t secs=seconds%UINT64_C(60);
    char text[24];
    (void)snprintf(text,sizeof(text),"%6lluM%3lluS",
        (unsigned long long)minutes,(unsigned long long)secs);
    return write_phase17_index0_text(machine,destination-UINT32_C(0x01000000),UINT32_C(0),text);
}

static vf2_status phase17_index6_read_u64(
    const vf2_model2a *machine, uint32_t address, uint64_t *value
)
{
    uint32_t low=0u,high=0u;
    vf2_status status=vf2_model2a_read_u32(machine,address,&low);
    if(status==VF2_OK)status=vf2_model2a_read_u32(machine,address+UINT32_C(4),&high);
    if(status==VF2_OK&&value!=NULL)*value=(uint64_t)low|((uint64_t)high<<32u);
    return status;
}

static vf2_status phase17_index6_render_game_data_values(
    vf2_model2a *machine,
    uint8_t slot,
    uint64_t *characters,
    int64_t *instruction_delta,
    uint64_t *call_delta,
    uint32_t *final_g0
)
{
    const uint32_t record=UINT32_C(0x01d00000)+(uint32_t)slot*UINT32_C(0x200);
    static const uint32_t simple_offsets[12]={
        UINT32_C(0x20),UINT32_C(0x140),UINT32_C(0x24),UINT32_C(0x144),
        UINT32_C(0x28),UINT32_C(0x148),UINT32_C(0x2c),UINT32_C(0x14c),
        UINT32_C(0x30),UINT32_C(0x150),UINT32_C(0x34),UINT32_C(0x154)
    };
    static const uint8_t simple_rows[12]={16u,17u,18u,19u,20u,21u,22u,23u,24u,25u,26u,27u};
    uint32_t count1=0u,countvs=0u,value=0u,row=0u,index=0u;
    uint64_t total1=0u,totalvs=0u;
    uint64_t chars=0u;
    vf2_status status=vf2_model2a_read_u32(machine,record,&count1);

    if(slot>=UINT8_C(16))return VF2_ERROR_UNSUPPORTED;
    if(status==VF2_OK)status=vf2_model2a_read_u32(machine,record+UINT32_C(4),&countvs);
    if(status==VF2_OK)status=phase17_index6_decimal(machine,(int32_t)count1,UINT32_C(0x0100033e));
    if(status==VF2_OK)status=phase17_index6_decimal(machine,(int32_t)countvs,UINT32_C(0x010003be));
    if(status==VF2_OK&&instruction_delta!=NULL){
        *instruction_delta+=(int64_t)phase17_index6_decimal_instruction_delta((int32_t)count1);
        *instruction_delta+=(int64_t)phase17_index6_decimal_instruction_delta((int32_t)countvs);
    }
    if(status==VF2_OK)status=phase17_index6_read_u64(machine,record+UINT32_C(8),&total1);
    if(status==VF2_OK)status=phase17_index6_read_u64(machine,record+UINT32_C(0x10),&totalvs);
    if(status==VF2_OK)status=phase17_index6_write_total_time(machine,total1,UINT32_C(0x01000424));
    if(status==VF2_OK)status=phase17_index6_write_total_time(machine,totalvs,UINT32_C(0x010004a4));
    if(status==VF2_OK){
        if(count1==0u){status=write_phase17_index0_text(machine,UINT32_C(10*0x80),UINT32_C(18),"    --M --S");}
        else{status=phase17_index6_write_minute_time(machine,total1/(uint64_t)count1,UINT32_C(0x01000524));if(instruction_delta!=NULL)*instruction_delta-=INT64_C(27);if(call_delta!=NULL)*call_delta+=UINT64_C(4);}
    }
    if(status==VF2_OK){
        if(countvs==0u){status=write_phase17_index0_text(machine,UINT32_C(11*0x80),UINT32_C(18),"    --M --S");}
        else{status=phase17_index6_write_minute_time(machine,totalvs/(uint64_t)countvs,UINT32_C(0x010005a4));if(instruction_delta!=NULL)*instruction_delta-=INT64_C(27);if(call_delta!=NULL)*call_delta+=UINT64_C(4);}
    }
    if(status==VF2_OK)status=vf2_model2a_read_u32(machine,record+UINT32_C(0x18),&value);
    if(status==VF2_OK)status=phase17_index6_write_minute_time(machine,(uint64_t)value,UINT32_C(0x01000624));
    if(status==VF2_OK)status=vf2_model2a_read_u32(machine,record+UINT32_C(0x138),&value);
    if(status==VF2_OK)status=phase17_index6_write_minute_time(machine,(uint64_t)value,UINT32_C(0x010006a4));
    if(status==VF2_OK)status=vf2_model2a_read_u32(machine,record+UINT32_C(0x1c),&value);
    if(status==VF2_OK)status=phase17_index6_write_minute_time(machine,(uint64_t)value,UINT32_C(0x01000724));
    if(status==VF2_OK)status=vf2_model2a_read_u32(machine,record+UINT32_C(0x13c),&value);
    if(status==VF2_OK)status=phase17_index6_write_minute_time(machine,(uint64_t)value,UINT32_C(0x010007a4));

    for(index=0u;status==VF2_OK&&index<UINT32_C(12);++index){
        status=vf2_model2a_read_u32(machine,record+simple_offsets[index],&value);
        if(status==VF2_OK)status=phase17_index6_decimal(machine,(int32_t)value,
            UINT32_C(0x01000000)+(uint32_t)simple_rows[index]*UINT32_C(0x80)+UINT32_C(31*2));
        if(status==VF2_OK&&instruction_delta!=NULL)*instruction_delta+=(int64_t)phase17_index6_decimal_instruction_delta((int32_t)value);
    }

    for(index=0u;status==VF2_OK&&index<UINT32_C(11);++index){
        const uint32_t rr=record+UINT32_C(0x38)+index*UINT32_C(16);
        uint32_t total=0u,wins=0u;
        uint64_t time=0u;
        uint32_t seconds=0u,avg=0u,rate=0u;
        row=UINT32_C(31)+index;
        status=vf2_model2a_read_u32(machine,rr,&total);
        if(status==VF2_OK)status=vf2_model2a_read_u32(machine,rr+UINT32_C(4),&wins);
        if(status==VF2_OK)status=phase17_index6_read_u64(machine,rr+UINT32_C(8),&time);
        seconds=(uint32_t)(time>>6u);
        if(total!=0u){avg=seconds/total;rate=(uint32_t)(((uint64_t)wins*UINT64_C(1000))/(uint64_t)total);}
        if(status==VF2_OK)status=phase17_index6_decimal(machine,(int32_t)total,UINT32_C(0x01000000)+row*UINT32_C(0x80)+UINT32_C(6*2));
        if(status==VF2_OK)status=phase17_index6_decimal(machine,(int32_t)wins,UINT32_C(0x01000000)+row*UINT32_C(0x80)+UINT32_C(12*2));
        if(status==VF2_OK)status=phase17_index6_decimal(machine,(int32_t)seconds,UINT32_C(0x01000000)+row*UINT32_C(0x80)+UINT32_C(18*2));
        if(total==0u){if(status==VF2_OK)status=write_phase17_index0_text(machine,row*UINT32_C(0x80),UINT32_C(24),"  ----");}
        else{
            if(status==VF2_OK)status=phase17_index6_decimal(machine,(int32_t)avg,UINT32_C(0x01000000)+row*UINT32_C(0x80)+UINT32_C(24*2));
            if(status==VF2_OK)status=phase17_index6_decimal(machine,(int32_t)rate,UINT32_C(0x01000000)+row*UINT32_C(0x80)+UINT32_C(30*2));
            if(instruction_delta!=NULL)*instruction_delta-=INT64_C(24);
            if(call_delta!=NULL)*call_delta+=UINT64_C(1);
        }
        if(status==VF2_OK&&instruction_delta!=NULL){
            *instruction_delta+=(int64_t)phase17_index6_decimal_instruction_delta((int32_t)total);
            *instruction_delta+=(int64_t)phase17_index6_decimal_instruction_delta((int32_t)wins);
            *instruction_delta+=(int64_t)phase17_index6_decimal_instruction_delta((int32_t)seconds);
            if(total!=0u){
                *instruction_delta+=(int64_t)phase17_index6_decimal_instruction_delta((int32_t)avg);
                *instruction_delta+=(int64_t)phase17_index6_decimal_instruction_delta((int32_t)rate);
            }
        }
    }
    for(index=0u;status==VF2_OK&&index<UINT32_C(33);++index){
        status=vf2_model2a_read_u32(machine,record+UINT32_C(0x158)+index*UINT32_C(4),&value);
        if(status==VF2_OK)status=phase17_index6_decimal(machine,(int32_t)value,
            UINT32_C(0x01000000)+(UINT32_C(11)+index)*UINT32_C(0x80)+UINT32_C(52*2));
        if(status==VF2_OK&&instruction_delta!=NULL)*instruction_delta+=(int64_t)phase17_index6_decimal_instruction_delta((int32_t)value);
    }
    chars=UINT64_C(14)*UINT64_C(6)+UINT64_C(19)*UINT64_C(2)+UINT64_C(11)*UINT64_C(2)+UINT64_C(9)*UINT64_C(4)+UINT64_C(11)*UINT64_C(24)+UINT64_C(33)*UINT64_C(6);
    if(status==VF2_OK&&characters!=NULL)*characters=chars;
    if(status==VF2_OK&&final_g0!=NULL)*final_g0=value;
    return status;
}

'''
s=s[:start]+new+s[end:]
# Replace odd-state zero validation/render block in finisher.
start=s.index('static vf2_status phase17_index6_finish_game_data_pair(')
end=s.index('static vf2_status execute_frame_phase17_bit7_index6(',start)
seg=s[start:end]
seg=seg.replace('''    uint64_t instructions = 0u;
    uint64_t calls = 0u;
    vf2_status status = VF2_OK;''','''    uint64_t instructions = 0u;
    uint64_t calls = 0u;
    int64_t instruction_delta = 0;
    uint64_t call_delta = 0u;
    uint32_t final_g0 = 0u;
    vf2_status status = VF2_OK;''',1)
old='''        uint32_t offset = 0u;
        uint8_t value = 0u;
        for (offset = 0u; status == VF2_OK && offset < UINT32_C(0x200); ++offset) {
            status = vf2_model2a_read(
                machine, UINT32_C(0x01d00000) +
                    (uint32_t)slot * UINT32_C(0x200) + offset,
                &value, sizeof(value)
            );
            if (status == VF2_OK && value != 0u) {
                return VF2_ERROR_UNSUPPORTED;
            }
        }
        if (status == VF2_OK) {
            status = phase17_index6_render_game_data_zero(machine, &characters);
        }
        instructions = UINT64_C(4576);
        calls = UINT64_C(133);'''
new2='''        status = phase17_index6_render_game_data_values(
            machine, slot, &characters, &instruction_delta, &call_delta,
            &final_g0
        );
        if (instruction_delta < -INT64_C(4576)) {
            return VF2_ERROR_UNSUPPORTED;
        }
        instructions = (uint64_t)(INT64_C(4576) + instruction_delta);
        calls = UINT64_C(133) + call_delta;'''
if old not in seg: raise SystemExit('odd zero block not found')
seg=seg.replace(old,new2,1)
seg=seg.replace('''    cpu->registers[16]=(state & UINT8_C(1)) == 0u ? UINT32_C(0x2e) : 0u;''','''    cpu->registers[16]=(state & UINT8_C(1)) == 0u ? UINT32_C(0x2e) : final_g0;''',1)
s=s[:start]+seg+s[end:]
p.write_text(s)

note=Path('decomp/i960/notes/selector17_index6_bookkeeping_game_data.md')
n=note.read_text()
n += '''\n## Non-zero GAME DATA semantics\n\nControlled ROM probes on the AKIRA block establish the live formulas. Time\nfields use 1/64-second ticks. TOTAL renders `(ticks>>6)` as D/H/M/S; AVG divides\nticks by GAME COUNT before the same shift and renders M/S; MIN/MAX are 32-bit\ntick values rendered as M/S. Each 1P round record contains total, wins and a\n64-bit tick accumulator; displayed seconds are `ticks>>6`, average is\n`seconds/total`, and win rate is `1000*wins/total`. The 33 VS histogram words\nare direct counters.\n\nA non-zero GAME COUNT selects the numeric AVG helper, changing the measured\npath by -27 instructions and +4 nested calls. A non-zero round total selects\nthe AVG/WINRATE path, changing that record by -24 instructions and +1 nested\ncall. Values that leave the small ROM decimal table retain the already-recovered\ndecimal helper's dynamic instruction delta. The bridge now renders non-zero\nstatistics for all sixteen slots rather than accepting only zeroed records.\n'''
n=n.replace('This cut admits the measured zero-data path for all sixteen hidden GAME DATA slots. Non-zero statistics remain an explicit next extension.','This cut admits both zero and non-zero GAME DATA for all sixteen hidden slots.')
note.write_text(n)
