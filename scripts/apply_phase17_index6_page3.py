from pathlib import Path
p=Path('src/recovered/texture_bridge_match.c')
s=p.read_text()
marker='static vf2_status execute_frame_phase17_bit7_index6('
helper=r'''
static vf2_status phase17_index6_render_page3(
    vf2_model2a *machine,
    uint8_t state,
    uint64_t *characters
)
{
    static const struct { uint8_t row,col; const char *text; } layout[] = {
        {2,26,"BOOKKEEPING 3/5"},{5,26,"1P GAME DATA"},{8,20,"GAME COUNT"},
        {10,7,"TOTAL TIME"},{11,9,"AVG TIME"},{12,9,"MIN TIME"},{13,9,"MAX TIME"},
        {16,16,"CONTINUE COUNT"},{17,21,"SET COUNT"},{18,20,"DRAW COUNT"},
        {19,13,"WIN BY K.O. COUNT"},{20,10,"WIN BY RINGOUT COUNT"},{21,12,"WIN BY JUDGE COUNT"},
        {25,16,"----COUNT---- ----TIME----"},{26,10,"ROUND TOTAL   WIN   TOTAL  AVG.   WIN RATE"},
        {27,11,"(th) (times)(times) (sec)  (sec)  (*1000)"},
        {28,12,"1"},{29,12,"2"},{30,12,"3"},{31,12,"4"},{32,12,"5"},{33,12,"6"},
        {34,12,"7"},{35,12,"8"},{36,12,"9"},{37,11,"10"},{38,11,"11"},
        {44,15,"PUSH SERVICE BUTTON TO CONTINUE."},{45,18,"PUSH TEST BUTTON TO EXIT."}
    };
    static const struct { uint8_t row,col; const char *text; } values[] = {
        {8,37,"0"},{10,23,"0D  0H  0M  0S"},{11,31,"--M --S"},{12,31,"0M  0S"},{13,31,"0M  0S"},
        {16,37,"0"},{17,37,"0"},{18,37,"0"},{19,37,"0"},{20,37,"0"},{21,37,"0"},
        {28,21,"0"},{28,28,"0"},{28,35,"0"},{28,39,"----"},
        {29,21,"0"},{29,28,"0"},{29,35,"0"},{29,39,"----"},
        {30,21,"0"},{30,28,"0"},{30,35,"0"},{30,39,"----"},
        {31,21,"0"},{31,28,"0"},{31,35,"0"},{31,39,"----"},
        {32,21,"0"},{32,28,"0"},{32,35,"0"},{32,39,"----"},
        {33,21,"0"},{33,28,"0"},{33,35,"0"},{33,39,"----"},
        {34,21,"0"},{34,28,"0"},{34,35,"0"},{34,39,"----"},
        {35,21,"0"},{35,28,"0"},{35,35,"0"},{35,39,"----"},
        {36,21,"0"},{36,28,"0"},{36,35,"0"},{36,39,"----"},
        {37,21,"0"},{37,28,"0"},{37,35,"0"},{37,39,"----"},
        {38,21,"0"},{38,28,"0"},{38,35,"0"},{38,39,"----"}
    };
    static const struct { uint8_t state,row,first,last; } spans[] = {
        {4,2,26,36},{4,2,38,44},{4,5,26,37},{4,8,10,29},{4,10,7,16},{4,11,9,16},{4,12,9,16},{4,13,9,16},
        {4,16,10,29},{4,17,10,29},{4,18,10,29},{4,19,10,29},{4,20,10,29},{4,21,10,29},
        {4,25,10,51},{4,26,10,51},{4,27,10,51},
        {4,28,10,12},{4,29,10,12},{4,30,10,12},{4,31,10,12},{4,32,10,12},{4,33,10,12},{4,34,10,12},{4,35,10,12},{4,36,10,12},{4,37,10,12},{4,38,10,12},
        {4,44,15,46},{4,45,18,42},
        {5,2,26,36},{5,2,38,44},{5,5,26,37},{5,8,10,29},{5,8,32,37},
        {5,10,7,16},{5,10,19,37},{5,11,9,16},{5,11,27,37},{5,12,9,16},{5,12,27,37},{5,13,9,16},{5,13,27,37},
        {5,16,10,29},{5,16,32,37},{5,17,10,29},{5,17,32,37},{5,18,10,29},{5,18,32,37},
        {5,19,10,29},{5,19,32,37},{5,20,10,29},{5,20,32,37},{5,21,10,29},{5,21,32,37},
        {5,25,10,51},{5,26,10,51},{5,27,10,51},
        {5,28,10,12},{5,29,10,12},{5,30,10,12},{5,31,10,12},{5,32,10,12},{5,33,10,12},{5,34,10,12},{5,35,10,12},{5,36,10,12},{5,37,10,12},{5,38,10,12},
        {5,28,16,21},{5,28,23,28},{5,28,30,35},{5,28,37,42},
        {5,29,16,21},{5,29,23,28},{5,29,30,35},{5,29,37,42},
        {5,30,16,21},{5,30,23,28},{5,30,30,35},{5,30,37,42},
        {5,31,16,21},{5,31,23,28},{5,31,30,35},{5,31,37,42},
        {5,32,16,21},{5,32,23,28},{5,32,30,35},{5,32,37,42},
        {5,33,16,21},{5,33,23,28},{5,33,30,35},{5,33,37,42},
        {5,34,16,21},{5,34,23,28},{5,34,30,35},{5,34,37,42},
        {5,35,16,21},{5,35,23,28},{5,35,30,35},{5,35,37,42},
        {5,36,16,21},{5,36,23,28},{5,36,30,35},{5,36,37,42},
        {5,37,16,21},{5,37,23,28},{5,37,30,35},{5,37,37,42},
        {5,38,16,21},{5,38,23,28},{5,38,30,35},{5,38,37,42},
        {5,44,15,46},{5,45,18,42}
    };
    size_t i=0u; uint32_t row=0u,col=0u; uint64_t count=0u;
    vf2_status status=clear_tile_plane_64x48(machine,UINT32_C(0x01000000));
    for(i=0u;status==VF2_OK&&i<sizeof(layout)/sizeof(layout[0]);++i){ status=write_phase17_index0_text(machine,(uint32_t)layout[i].row*UINT32_C(0x80),layout[i].col,layout[i].text); count+=(uint64_t)strlen(layout[i].text); }
    if(state==UINT8_C(5)) for(i=0u;status==VF2_OK&&i<sizeof(values)/sizeof(values[0]);++i){ status=write_phase17_index0_text(machine,(uint32_t)values[i].row*UINT32_C(0x80),values[i].col,values[i].text); count+=(uint64_t)strlen(values[i].text); }
    for(row=0u;status==VF2_OK&&row<UINT32_C(48);++row) for(col=0u;status==VF2_OK&&col<UINT32_C(64);++col){ uint16_t cell=0u; const uint32_t ad=UINT32_C(0x01000000)+row*UINT32_C(0x80)+col*UINT32_C(2); status=read_u16(machine,ad,&cell); if(status==VF2_OK) status=write_u16(machine,ad,cell&UINT16_C(0x7fff)); }
    for(i=0u;status==VF2_OK&&i<sizeof(spans)/sizeof(spans[0]);++i) if(spans[i].state==state) for(col=spans[i].first;status==VF2_OK&&col<=spans[i].last;++col){ uint16_t cell=0u; const uint32_t ad=UINT32_C(0x01000000)+(uint32_t)spans[i].row*UINT32_C(0x80)+col*UINT32_C(2); status=read_u16(machine,ad,&cell); if(status==VF2_OK) status=write_u16(machine,ad,cell|UINT16_C(0x8000)); }
    if(status==VF2_OK&&characters!=NULL)*characters=count; return status;
}

'''
if marker not in s: raise SystemExit('marker missing')
s=s.replace(marker,helper+marker,1)
a=s.index('static vf2_status execute_frame_phase17_bit7_index6('); b=s.index('static vf2_status execute_frame_phase17_bit7_index11(',a); f=s[a:b]
f=f.replace('phase_a5 > UINT8_C(3)','phase_a5 > UINT8_C(5)',1)
old='''status = phase_a5 <= UINT8_C(1)
        ? phase17_index6_render_page1(machine, phase_a5, &characters)
        : phase17_index6_render_page2(machine, phase_a5, &characters);'''
new='''status = phase_a5 <= UINT8_C(1)
        ? phase17_index6_render_page1(machine, phase_a5, &characters)
        : (phase_a5 <= UINT8_C(3)
            ? phase17_index6_render_page2(machine, phase_a5, &characters)
            : phase17_index6_render_page3(machine, phase_a5, &characters));'''
if old not in f: raise SystemExit('renderer selector missing')
f=f.replace(old,new,1)
old2='''        else { instructions = UINT64_C(15175); calls = UINT64_C(40); }
    } else if (phase_a5 == UINT8_C(1)) {
        instructions = UINT64_C(1815); calls = UINT64_C(79);
    } else {
        instructions = UINT64_C(3626); calls = UINT64_C(128);
    }'''
new2='''        else if (phase_a5 == UINT8_C(2)) { instructions = UINT64_C(15175); calls = UINT64_C(40); }
        else { instructions = UINT64_C(14911); calls = UINT64_C(32); }
    } else if (phase_a5 == UINT8_C(1)) {
        instructions = UINT64_C(1815); calls = UINT64_C(79);
    } else if (phase_a5 == UINT8_C(3)) {
        instructions = UINT64_C(3626); calls = UINT64_C(128);
    } else {
        instructions = UINT64_C(3122); calls = UINT64_C(99);
    }'''
# expand even condition 0 or2 to also4
f=f.replace('if (phase_a5 == UINT8_C(0) || phase_a5 == UINT8_C(2)) {','if (phase_a5 == UINT8_C(0) || phase_a5 == UINT8_C(2) || phase_a5 == UINT8_C(4)) {',1)
if old2 not in f: raise SystemExit('accounting missing')
f=f.replace(old2,new2,1)
f=f.replace('''cpu->registers[16] = phase_a5 == UINT8_C(0) || phase_a5 == UINT8_C(2)
        ? UINT32_C(0x2e)
        : (phase_a5 == UINT8_C(1) ? UINT32_C(0x00532d2d) : 0u);''','''cpu->registers[16] = (phase_a5 & UINT8_C(1)) == 0u
        ? UINT32_C(0x2e)
        : (phase_a5 == UINT8_C(1) ? UINT32_C(0x00532d2d)
            : (phase_a5 == UINT8_C(5) ? UINT32_C(0x00002d2d) : 0u));''',1)
f=f.replace('''cpu->registers[25] = phase_a5 == UINT8_C(0) || phase_a5 == UINT8_C(2)
        ? UINT32_C(0x01001724)
        : (phase_a5 == UINT8_C(1) ? UINT32_C(0x010013c2) : UINT32_C(0x010014e6));''','''cpu->registers[25] = (phase_a5 & UINT8_C(1)) == 0u
        ? UINT32_C(0x01001724)
        : (phase_a5 == UINT8_C(1) ? UINT32_C(0x010013c2)
            : (phase_a5 == UINT8_C(3) ? UINT32_C(0x010014e6) : UINT32_C(0x0100135c)));''',1)
s=s[:a]+f+s[b:]; p.write_text(s)
