from pathlib import Path
p=Path('src/recovered/texture_bridge_match.c')
s=p.read_text()
marker='static vf2_status execute_frame_phase17_bit7_index6('
if marker not in s: raise SystemExit('index6 marker missing')
helper=r'''
static vf2_status phase17_index6_render_page2(
    vf2_model2a *machine,
    uint8_t state,
    uint64_t *characters
)
{
    static const struct { uint8_t row; uint8_t col; const char *text; } layout[] = {
        {2,26,"BOOKKEEPING 2/5"},{5,25,"GLOBAL DATA 2"},{6,25,"-TYPE-B DATA-"},
        {8,36,"START     CONTINUE"},{10,14,"1P PLAY COUNT"},{12,14,"VS PLAY COUNT"},
        {14,10,"1P AVG. PLAY TIME"},{16,10,"VS AVG. PLAY TIME"},
        {19,7,"TIME                        COUNT"},
        {20,14,"-1P START-  -VS START-  -1P CONT-  -VS CONT-"},
        {21,5,"0~   30S"},{22,6,"~ 1M"},{23,6,"~ 1M30S"},{24,6,"~ 2M"},
        {25,6,"~ 2M30S"},{26,6,"~ 3M"},{27,6,"~ 3M30S"},{28,6,"~ 4M"},
        {29,6,"~ 4M30S"},{30,6,"~ 5M"},{31,6,"~ 5M30S"},{32,6,"~ 6M"},
        {33,6,"~ 6M30S"},{34,6,"~ 7M"},{35,6,"~ 7M30S"},{36,6,"~ 8M"},
        {37,6,"~ 8M30S"},{38,6,"~ 9M"},{39,6,"~ 9M30S"},{40,6,"~10M"},
        {41,5,"10M~"},{44,15,"PUSH SERVICE BUTTON TO CONTINUE."},
        {45,18,"PUSH TEST BUTTON TO EXIT."}
    };
    static const struct { uint8_t row; uint8_t col; const char *text; } values[] = {
        {10,35,"0"},{10,48,"0"},{12,35,"0"},{12,48,"0"},
        {14,33,"--M --S"},{14,46,"--M --S"},{16,33,"--M --S"},{16,46,"--M --S"},
        {21,20,"0"},{21,32,"0"},{21,44,"0"},{21,55,"0"},
        {22,20,"0"},{22,32,"0"},{22,44,"0"},{22,55,"0"},
        {23,20,"0"},{23,32,"0"},{23,44,"0"},{23,55,"0"},
        {24,20,"0"},{24,32,"0"},{24,44,"0"},{24,55,"0"},
        {25,20,"0"},{25,32,"0"},{25,44,"0"},{25,55,"0"},
        {26,20,"0"},{26,32,"0"},{26,44,"0"},{26,55,"0"},
        {27,20,"0"},{27,32,"0"},{27,44,"0"},{27,55,"0"},
        {28,20,"0"},{28,32,"0"},{28,44,"0"},{28,55,"0"},
        {29,20,"0"},{29,32,"0"},{29,44,"0"},{29,55,"0"},
        {30,20,"0"},{30,32,"0"},{30,44,"0"},{30,55,"0"},
        {31,20,"0"},{31,32,"0"},{31,44,"0"},{31,55,"0"},
        {32,20,"0"},{32,32,"0"},{32,44,"0"},{32,55,"0"},
        {33,20,"0"},{33,32,"0"},{33,44,"0"},{33,55,"0"},
        {34,20,"0"},{34,32,"0"},{34,44,"0"},{34,55,"0"},
        {35,20,"0"},{35,32,"0"},{35,44,"0"},{35,55,"0"},
        {36,20,"0"},{36,32,"0"},{36,44,"0"},{36,55,"0"},
        {37,20,"0"},{37,32,"0"},{37,44,"0"},{37,55,"0"},
        {38,20,"0"},{38,32,"0"},{38,44,"0"},{38,55,"0"},
        {39,20,"0"},{39,32,"0"},{39,44,"0"},{39,55,"0"},
        {40,20,"0"},{40,32,"0"},{40,44,"0"},{40,55,"0"},
        {41,20,"0"},{41,32,"0"},{41,44,"0"},{41,55,"0"}
    };
    static const struct { uint8_t state,row,first,last; } spans[] = {
        {2,2,26,36},{2,2,38,44},{2,5,25,37},{2,6,25,37},{2,8,10,53},
        {2,10,10,26},{2,12,10,26},{2,14,10,26},{2,16,10,26},{2,19,5,39},
        {2,20,5,57},{2,21,5,12},{2,22,5,9},{2,23,5,12},{2,24,5,9},
        {2,25,5,12},{2,26,5,9},{2,27,5,12},{2,28,5,9},{2,29,5,12},
        {2,30,5,9},{2,31,5,12},{2,32,5,9},{2,33,5,12},{2,34,5,9},
        {2,35,5,12},{2,36,5,9},{2,37,5,12},{2,38,5,9},{2,39,5,12},
        {2,40,5,9},{2,41,5,8},{2,44,15,46},{2,45,18,42},
        {3,2,26,36},{3,2,38,44},{3,5,25,37},{3,6,25,37},{3,8,10,53},
        {3,10,10,26},{3,10,35,40},{3,10,48,53},{3,12,10,26},{3,12,35,40},{3,12,48,53},
        {3,14,10,26},{3,14,30,40},{3,14,43,53},{3,16,10,26},{3,16,30,40},{3,16,43,53},
        {3,19,5,39},{3,20,5,57},
        {3,21,5,12},{3,22,5,9},{3,23,5,12},{3,24,5,9},{3,25,5,12},{3,26,5,9},
        {3,27,5,12},{3,28,5,9},{3,29,5,12},{3,30,5,9},{3,31,5,12},{3,32,5,9},
        {3,33,5,12},{3,34,5,9},{3,35,5,12},{3,36,5,9},{3,37,5,12},{3,38,5,9},
        {3,39,5,12},{3,40,5,9},{3,41,5,8},
        {3,21,16,21},{3,21,28,33},{3,21,40,45},{3,21,51,56},
        {3,22,16,21},{3,22,28,33},{3,22,40,45},{3,22,51,56},
        {3,23,16,21},{3,23,28,33},{3,23,40,45},{3,23,51,56},
        {3,24,16,21},{3,24,28,33},{3,24,40,45},{3,24,51,56},
        {3,25,16,21},{3,25,28,33},{3,25,40,45},{3,25,51,56},
        {3,26,16,21},{3,26,28,33},{3,26,40,45},{3,26,51,56},
        {3,27,16,21},{3,27,28,33},{3,27,40,45},{3,27,51,56},
        {3,28,16,21},{3,28,28,33},{3,28,40,45},{3,28,51,56},
        {3,29,16,21},{3,29,28,33},{3,29,40,45},{3,29,51,56},
        {3,30,16,21},{3,30,28,33},{3,30,40,45},{3,30,51,56},
        {3,31,16,21},{3,31,28,33},{3,31,40,45},{3,31,51,56},
        {3,32,16,21},{3,32,28,33},{3,32,40,45},{3,32,51,56},
        {3,33,16,21},{3,33,28,33},{3,33,40,45},{3,33,51,56},
        {3,34,16,21},{3,34,28,33},{3,34,40,45},{3,34,51,56},
        {3,35,16,21},{3,35,28,33},{3,35,40,45},{3,35,51,56},
        {3,36,16,21},{3,36,28,33},{3,36,40,45},{3,36,51,56},
        {3,37,16,21},{3,37,28,33},{3,37,40,45},{3,37,51,56},
        {3,38,16,21},{3,38,28,33},{3,38,40,45},{3,38,51,56},
        {3,39,16,21},{3,39,28,33},{3,39,40,45},{3,39,51,56},
        {3,40,16,21},{3,40,28,33},{3,40,40,45},{3,40,51,56},
        {3,41,16,21},{3,41,28,33},{3,41,40,45},{3,41,51,56},
        {3,44,15,46},{3,45,18,42}
    };
    size_t i=0u; uint32_t row=0u,col=0u; uint64_t count=0u;
    vf2_status status=clear_tile_plane_64x48(machine,UINT32_C(0x01000000));
    for(i=0u;status==VF2_OK&&i<sizeof(layout)/sizeof(layout[0]);++i){
        status=write_phase17_index0_text(machine,(uint32_t)layout[i].row*UINT32_C(0x80),layout[i].col,layout[i].text);
        count+=(uint64_t)strlen(layout[i].text);
    }
    if(state==UINT8_C(3)) for(i=0u;status==VF2_OK&&i<sizeof(values)/sizeof(values[0]);++i){
        status=write_phase17_index0_text(machine,(uint32_t)values[i].row*UINT32_C(0x80),values[i].col,values[i].text);
        count+=(uint64_t)strlen(values[i].text);
    }
    for(row=0u;status==VF2_OK&&row<UINT32_C(48);++row) for(col=0u;status==VF2_OK&&col<UINT32_C(64);++col){
        uint16_t cell=0u; const uint32_t address=UINT32_C(0x01000000)+row*UINT32_C(0x80)+col*UINT32_C(2);
        status=read_u16(machine,address,&cell); if(status==VF2_OK) status=write_u16(machine,address,cell&UINT16_C(0x7fff));
    }
    for(i=0u;status==VF2_OK&&i<sizeof(spans)/sizeof(spans[0]);++i) if(spans[i].state==state){
        for(col=spans[i].first;status==VF2_OK&&col<=spans[i].last;++col){
            uint16_t cell=0u; const uint32_t address=UINT32_C(0x01000000)+(uint32_t)spans[i].row*UINT32_C(0x80)+col*UINT32_C(2);
            status=read_u16(machine,address,&cell); if(status==VF2_OK) status=write_u16(machine,address,cell|UINT16_C(0x8000));
        }
    }
    if(status==VF2_OK&&characters!=NULL)*characters=count;
    return status;
}

'''
s=s.replace(marker,helper+marker,1)
a=s.index('static vf2_status execute_frame_phase17_bit7_index6(')
b=s.index('static vf2_status execute_frame_phase17_bit7_index11(',a)
f=s[a:b]
f=f.replace('phase_a5 > UINT8_C(1)','phase_a5 > UINT8_C(3)',1)
f=f.replace('status = phase17_index6_render_page1(machine, phase_a5, &characters);','status = phase_a5 <= UINT8_C(1)\n        ? phase17_index6_render_page1(machine, phase_a5, &characters)\n        : phase17_index6_render_page2(machine, phase_a5, &characters);',1)
old='''    if (phase_a5 == UINT8_C(0)) {
        const uint8_t next = UINT8_C(1);
        status = vf2_model2a_write(machine, UINT32_C(0x005000a5), &next, sizeof(next));
        instructions = UINT64_C(15309);
        calls = UINT64_C(25);
    } else {
        instructions = UINT64_C(1815);
        calls = UINT64_C(79);
    }'''
new='''    if (phase_a5 == UINT8_C(0) || phase_a5 == UINT8_C(2)) {
        const uint8_t next = (uint8_t)(phase_a5 + UINT8_C(1));
        status = vf2_model2a_write(machine, UINT32_C(0x005000a5), &next, sizeof(next));
        if (phase_a5 == UINT8_C(0)) { instructions = UINT64_C(15309); calls = UINT64_C(25); }
        else { instructions = UINT64_C(15175); calls = UINT64_C(40); }
    } else if (phase_a5 == UINT8_C(1)) {
        instructions = UINT64_C(1815); calls = UINT64_C(79);
    } else {
        instructions = UINT64_C(3626); calls = UINT64_C(128);
    }'''
if old not in f: raise SystemExit('state accounting anchor missing')
f=f.replace(old,new,1)
f=f.replace('cpu->registers[14] = phase_a5 == 0u ? UINT32_C(3) : UINT32_C(4);','cpu->registers[14] = UINT32_C(3) + (uint32_t)phase_a5;',1)
f=f.replace('cpu->registers[16] = phase_a5 == 0u ? UINT32_C(0x2e) : UINT32_C(0x00532d2d);','''cpu->registers[16] = phase_a5 == UINT8_C(0) || phase_a5 == UINT8_C(2)
        ? UINT32_C(0x2e)
        : (phase_a5 == UINT8_C(1) ? UINT32_C(0x00532d2d) : 0u);''',1)
f=f.replace('cpu->registers[25] = phase_a5 == 0u ? UINT32_C(0x01001724) : UINT32_C(0x010013c2);','''cpu->registers[25] = phase_a5 == UINT8_C(0) || phase_a5 == UINT8_C(2)
        ? UINT32_C(0x01001724)
        : (phase_a5 == UINT8_C(1) ? UINT32_C(0x010013c2) : UINT32_C(0x010014e6));''',1)
f=f.replace('if (phase_a5 == 0u) {','if ((phase_a5 & UINT8_C(1)) == 0u) {',1)
s=s[:a]+f+s[b:]
p.write_text(s)
