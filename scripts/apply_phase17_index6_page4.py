from pathlib import Path
p=Path('src/recovered/texture_bridge_match.c')
s=p.read_text()
marker='static vf2_status execute_frame_phase17_bit7_index6('
helper=r'''
static vf2_status phase17_index6_render_page4(
    vf2_model2a *machine,
    uint8_t state,
    uint64_t *characters
)
{
    static const struct { uint8_t row,col; const char *text; } fixed[] = {
        {2,26,"BOOKKEEPING 4/5"},{5,26,"VS GAME DATA"},
        {8,20,"GAME COUNT"},{8,42,"GAMETIME   COUNT"},{9,45,"(sec)  (times)"},
        {10,7,"TOTAL TIME"},{11,7,"AVG TIME"},{12,7,"MIN TIME"},{13,7,"MAX TIME"},
        {16,16,"CONTINUE COUNT"},{17,21,"SET COUNT"},{18,20,"DRAW COUNT"},
        {19,13,"WIN BY K.O. COUNT"},{20,10,"WIN BY RINGOUT COUNT"},{21,12,"WIN BY JUDGE COUNT"},
        {44,15,"PUSH SERVICE BUTTON TO CONTINUE."},{45,18,"PUSH TEST BUTTON TO EXIT."}
    };
    static const char *ranges[33] = {
        "~ 10","~ 13","~ 16","~ 19","~ 22","~ 25","~ 28","~ 31","~ 34","~ 37","~ 40",
        "~ 43","~ 46","~ 49","~ 52","~ 55","~ 58","~ 61","~ 64","~ 67","~ 70","~ 73",
        "~ 76","~ 79","~ 82","~ 85","~ 88","~ 91","~ 94","~ 97","~100","~103","104~"
    };
    static const struct { uint8_t row,col; const char *text; } values[] = {
        {8,37,"0"},{10,24,"0D  0H  0M  0S"},{11,31,"--M --S"},{12,32,"0M  0S"},{13,32,"0M  0S"},
        {16,37,"0"},{17,37,"0"},{18,37,"0"},{19,37,"0"},{20,37,"0"},{21,37,"0"}
    };
    size_t i=0u; uint32_t row=0u,col=0u; uint64_t count=0u;
    vf2_status status=clear_tile_plane_64x48(machine,UINT32_C(0x01000000));
    for(i=0u;status==VF2_OK&&i<sizeof(fixed)/sizeof(fixed[0]);++i){status=write_phase17_index0_text(machine,(uint32_t)fixed[i].row*UINT32_C(0x80),fixed[i].col,fixed[i].text);count+=(uint64_t)strlen(fixed[i].text);}
    for(i=0u;status==VF2_OK&&i<33u;++i){uint8_t r=(uint8_t)(10u+i);uint8_t c=(i==32u)?42u:43u;status=write_phase17_index0_text(machine,(uint32_t)r*UINT32_C(0x80),c,ranges[i]);count+=(uint64_t)strlen(ranges[i]);}
    if(state==UINT8_C(7)){
        for(i=0u;status==VF2_OK&&i<sizeof(values)/sizeof(values[0]);++i){status=write_phase17_index0_text(machine,(uint32_t)values[i].row*UINT32_C(0x80),values[i].col,values[i].text);count+=(uint64_t)strlen(values[i].text);}
        for(i=0u;status==VF2_OK&&i<33u;++i){status=write_phase17_index0_text(machine,(uint32_t)(10u+i)*UINT32_C(0x80),57u,"0");++count;}
    }
    for(row=0u;status==VF2_OK&&row<UINT32_C(48);++row)for(col=0u;status==VF2_OK&&col<UINT32_C(64);++col){uint16_t cell=0u;const uint32_t ad=UINT32_C(0x01000000)+row*UINT32_C(0x80)+col*UINT32_C(2);status=read_u16(machine,ad,&cell);if(status==VF2_OK)status=write_u16(machine,ad,cell&UINT16_C(0x7fff));}
    { static const struct { uint8_t row,first,last; } base[] = {
        {2,26,36},{2,38,44},{5,26,37},{8,10,29},{8,42,57},{9,42,58},
        {10,7,16},{11,7,16},{12,7,16},{13,7,16},{16,10,29},{17,10,29},{18,10,29},{19,10,29},{20,10,29},{21,10,29},
        {44,15,46},{45,18,42}
      };
      for(i=0u;status==VF2_OK&&i<sizeof(base)/sizeof(base[0]);++i)for(col=base[i].first;status==VF2_OK&&col<=base[i].last;++col){uint16_t cell=0u;const uint32_t ad=UINT32_C(0x01000000)+(uint32_t)base[i].row*UINT32_C(0x80)+col*UINT32_C(2);status=read_u16(machine,ad,&cell);if(status==VF2_OK)status=write_u16(machine,ad,cell|UINT16_C(0x8000));}
    }
    for(row=10u;status==VF2_OK&&row<=41u;++row){for(col=41u;status==VF2_OK&&col<=46u;++col){uint16_t cell=0u;const uint32_t ad=UINT32_C(0x01000000)+row*UINT32_C(0x80)+col*UINT32_C(2);status=read_u16(machine,ad,&cell);if(status==VF2_OK)status=write_u16(machine,ad,cell|UINT16_C(0x8000));}{uint16_t cell=0u;const uint32_t ad=UINT32_C(0x01000000)+row*UINT32_C(0x80)+UINT32_C(48*2);status=read_u16(machine,ad,&cell);if(status==VF2_OK)status=write_u16(machine,ad,cell|UINT16_C(0x8000));}}
    if(status==VF2_OK){for(col=42u;col<=46u;++col){uint16_t cell=0u;const uint32_t ad=UINT32_C(0x01000000)+UINT32_C(42*0x80)+col*UINT32_C(2);status=read_u16(machine,ad,&cell);if(status==VF2_OK)status=write_u16(machine,ad,cell|UINT16_C(0x8000));}}
    if(state==UINT8_C(7)&&status==VF2_OK){
        static const struct{uint8_t row,first,last;} leftvals[]={{8,32,37},{10,19,37},{11,27,37},{12,27,37},{13,27,37},{16,32,37},{17,32,37},{18,32,37},{19,32,37},{20,32,37},{21,32,37}};
        for(i=0u;status==VF2_OK&&i<sizeof(leftvals)/sizeof(leftvals[0]);++i)for(col=leftvals[i].first;status==VF2_OK&&col<=leftvals[i].last;++col){uint16_t cell=0u;const uint32_t ad=UINT32_C(0x01000000)+(uint32_t)leftvals[i].row*UINT32_C(0x80)+col*UINT32_C(2);status=read_u16(machine,ad,&cell);if(status==VF2_OK)status=write_u16(machine,ad,cell|UINT16_C(0x8000));}
        for(row=10u;status==VF2_OK&&row<=42u;++row)for(col=52u;status==VF2_OK&&col<=57u;++col){uint16_t cell=0u;const uint32_t ad=UINT32_C(0x01000000)+row*UINT32_C(0x80)+col*UINT32_C(2);status=read_u16(machine,ad,&cell);if(status==VF2_OK)status=write_u16(machine,ad,cell|UINT16_C(0x8000));}
    }
    if(status==VF2_OK&&characters!=NULL){*characters=count;}
    return status;
}

'''
if marker not in s:raise SystemExit('marker missing')
s=s.replace(marker,helper+marker,1)
a=s.index('static vf2_status execute_frame_phase17_bit7_index6(');b=s.index('static vf2_status execute_frame_phase17_bit7_index11(',a);f=s[a:b]
f=f.replace('phase_a5 > UINT8_C(5)','phase_a5 > UINT8_C(7)',1)
old='''        : (phase_a5 <= UINT8_C(3)
            ? phase17_index6_render_page2(machine, phase_a5, &characters)
            : phase17_index6_render_page3(machine, phase_a5, &characters));'''
new='''        : (phase_a5 <= UINT8_C(3)
            ? phase17_index6_render_page2(machine, phase_a5, &characters)
            : (phase_a5 <= UINT8_C(5)
                ? phase17_index6_render_page3(machine, phase_a5, &characters)
                : phase17_index6_render_page4(machine, phase_a5, &characters)));'''
if old not in f:raise SystemExit('selector anchor missing')
f=f.replace(old,new,1)
f=f.replace('phase_a5 == UINT8_C(0) || phase_a5 == UINT8_C(2) || phase_a5 == UINT8_C(4)','phase_a5 == UINT8_C(0) || phase_a5 == UINT8_C(2) || phase_a5 == UINT8_C(4) || phase_a5 == UINT8_C(6)',1)
old='''        else { instructions = UINT64_C(14911); calls = UINT64_C(32); }
    } else if (phase_a5 == UINT8_C(1)) {'''
new='''        else if (phase_a5 == UINT8_C(4)) { instructions = UINT64_C(14911); calls = UINT64_C(32); }
        else { instructions = UINT64_C(17251); calls = UINT64_C(117); }
    } else if (phase_a5 == UINT8_C(1)) {'''
if old not in f:raise SystemExit('even accounting anchor missing')
f=f.replace(old,new,1)
old='''    } else if (phase_a5 == UINT8_C(5)) {
        instructions = UINT64_C(3122); calls = UINT64_C(99);
    } else {
        instructions = UINT64_C(1946); calls = UINT64_C(74);
    }'''
if old not in f:
    old='''    } else {
        instructions = UINT64_C(3122); calls = UINT64_C(99);
    }'''
    new='''    } else if (phase_a5 == UINT8_C(5)) {
        instructions = UINT64_C(3122); calls = UINT64_C(99);
    } else {
        instructions = UINT64_C(1946); calls = UINT64_C(74);
    }'''
    if old not in f:raise SystemExit('odd accounting anchor missing')
    f=f.replace(old,new,1)
f=f.replace('''            : (phase_a5 == UINT8_C(3) ? UINT32_C(0x010014e6) : UINT32_C(0x0100135c)));''','''            : (phase_a5 == UINT8_C(3) ? UINT32_C(0x010014e6)
                : (phase_a5 == UINT8_C(5) ? UINT32_C(0x0100135c) : UINT32_C(0x01001568))));''',1)
f=f.replace('cpu->registers[17] = 0u;','cpu->registers[17] = phase_a5 == UINT8_C(7) ? UINT32_C(0x01d0361c) : 0u;',1)
s=s[:a]+f+s[b:];p.write_text(s)
