from pathlib import Path
p=Path('src/recovered/texture_bridge_match.c')
s=p.read_text()
marker='static vf2_status execute_frame_phase17_bit7_index6('
helper=r'''
static vf2_status phase17_index6_render_page5(
    vf2_model2a *machine,
    uint8_t state,
    uint8_t fighter,
    uint64_t *characters
)
{
    static const char *names[10] = {"AKIRA","JACKY","SARAH","KAGE","LAU","JEFFRY","PAI","WOLF","SHUN","LION"};
    static const struct { uint8_t row,col; const char *text; } fixed[] = {
        {2,26,"BOOKKEEPING 5/5"},{5,25,"VS GAME DATA 2"},{8,27,"VS DIAGRAM"},
        {10,20,"MY CHAR :"},{12,6,"V.S. CHAR.        WIN.    LOSE. 0%     RATE.    100%"},
        {14,6,"AKIRA"},{16,6,"JACKY"},{18,6,"SARAH"},{20,6,"KAGE"},{22,6,"LAU"},
        {24,6,"JEFFRY"},{26,6,"PAI"},{28,6,"WOLF"},{30,6,"SHUN"},{32,6,"LION"},
        {43,12,"SELECT BY PLAYER 1 SIDE LEVER UP/DOWN."},
        {44,15,"PUSH SERVICE BUTTON TO CONTINUE."},{45,18,"PUSH TEST BUTTON TO EXIT."}
    };
    static const uint8_t rows[10]={14,16,18,20,22,24,26,28,30,32};
    size_t i=0u;uint32_t row=0u,col=0u;uint64_t count=0u;
    vf2_status status=clear_tile_plane_64x48(machine,UINT32_C(0x01000000));
    if(fighter>UINT8_C(9))return VF2_ERROR_UNSUPPORTED;
    for(i=0u;status==VF2_OK&&i<sizeof(fixed)/sizeof(fixed[0]);++i){status=write_phase17_index0_text(machine,(uint32_t)fixed[i].row*UINT32_C(0x80),fixed[i].col,fixed[i].text);count+=(uint64_t)strlen(fixed[i].text);}
    if(state==UINT8_C(9)){
        status=write_phase17_index0_text(machine,UINT32_C(10*0x80),30u,names[fighter]);count+=(uint64_t)strlen(names[fighter]);
        for(i=0u;status==VF2_OK&&i<10u;++i){status=write_phase17_index0_text(machine,(uint32_t)rows[i]*UINT32_C(0x80),28u,"0");++count;if(status==VF2_OK){status=write_phase17_index0_text(machine,(uint32_t)rows[i]*UINT32_C(0x80),36u,"0");++count;}if(status==VF2_OK){status=write_phase17_index0_text(machine,(uint32_t)rows[i]*UINT32_C(0x80),38u,"-");++count;}}
    }
    for(row=0u;status==VF2_OK&&row<UINT32_C(48);++row)for(col=0u;status==VF2_OK&&col<UINT32_C(64);++col){uint16_t cell=0u;const uint32_t ad=UINT32_C(0x01000000)+row*UINT32_C(0x80)+col*UINT32_C(2);status=read_u16(machine,ad,&cell);if(status==VF2_OK)status=write_u16(machine,ad,cell&UINT16_C(0x7fff));}
    {static const struct{uint8_t row,first,last;} base[]={{2,26,36},{2,38,44},{5,25,38},{8,27,36},{10,20,29},{12,6,57},{14,6,10},{16,6,10},{18,6,10},{20,6,9},{22,6,8},{24,6,11},{26,6,8},{28,6,9},{30,6,9},{32,6,9},{43,12,49},{44,15,46},{45,18,42}};for(i=0u;status==VF2_OK&&i<sizeof(base)/sizeof(base[0]);++i)for(col=base[i].first;status==VF2_OK&&col<=base[i].last;++col){uint16_t cell=0u;const uint32_t ad=UINT32_C(0x01000000)+(uint32_t)base[i].row*UINT32_C(0x80)+col*UINT32_C(2);status=read_u16(machine,ad,&cell);if(status==VF2_OK)status=write_u16(machine,ad,cell|UINT16_C(0x8000));}}
    if(state==UINT8_C(9)&&status==VF2_OK){
        for(col=30u;status==VF2_OK&&col<=35u;++col){uint16_t cell=0u;const uint32_t ad=UINT32_C(0x01000000)+UINT32_C(10*0x80)+col*UINT32_C(2);status=read_u16(machine,ad,&cell);if(status==VF2_OK)status=write_u16(machine,ad,cell|UINT16_C(0x8000));}
        for(i=0u;status==VF2_OK&&i<10u;++i){static const uint8_t starts[3]={23,31,38};static const uint8_t ends[3]={28,36,57};size_t j=0u;for(j=0u;status==VF2_OK&&j<3u;++j)for(col=starts[j];status==VF2_OK&&col<=ends[j];++col){uint16_t cell=0u;const uint32_t ad=UINT32_C(0x01000000)+(uint32_t)rows[i]*UINT32_C(0x80)+col*UINT32_C(2);status=read_u16(machine,ad,&cell);if(status==VF2_OK)status=write_u16(machine,ad,cell|UINT16_C(0x8000));}}
    }
    if(status==VF2_OK&&characters!=NULL){*characters=count;}
    return status;
}

'''
if marker not in s:raise SystemExit('marker missing')
s=s.replace(marker,helper+marker,1)
a=s.index('static vf2_status execute_frame_phase17_bit7_index6(');b=s.index('static vf2_status execute_frame_phase17_bit7_index11(',a);f=s[a:b]
f=f.replace('phase_a5 > UINT8_C(7)','phase_a5 > UINT8_C(9)',1)
# state9 permits selected fighter 0; state8 begins with ff and initializes it.
f=f.replace('phase_a5 > UINT8_C(9) || phase_a6 != UINT8_C(0xff) || phase_a7 != UINT8_C(0xff))','''phase_a5 > UINT8_C(9) || phase_a6 != UINT8_C(0xff) ||
        ((phase_a5 < UINT8_C(9) && phase_a7 != UINT8_C(0xff)) ||
         (phase_a5 == UINT8_C(9) && phase_a7 > UINT8_C(9))))''',1)
old='''            : (phase_a5 <= UINT8_C(5)
                ? phase17_index6_render_page3(machine, phase_a5, &characters)
                : phase17_index6_render_page4(machine, phase_a5, &characters)));'''
new='''            : (phase_a5 <= UINT8_C(5)
                ? phase17_index6_render_page3(machine, phase_a5, &characters)
                : (phase_a5 <= UINT8_C(7)
                    ? phase17_index6_render_page4(machine, phase_a5, &characters)
                    : phase17_index6_render_page5(machine, phase_a5,
                        phase_a5 == UINT8_C(8) ? UINT8_C(0) : phase_a7,
                        &characters))));'''
if old not in f:raise SystemExit('selector anchor missing')
f=f.replace(old,new,1)
f=f.replace('phase_a5 == UINT8_C(0) || phase_a5 == UINT8_C(2) || phase_a5 == UINT8_C(4) || phase_a5 == UINT8_C(6)','phase_a5 == UINT8_C(0) || phase_a5 == UINT8_C(2) || phase_a5 == UINT8_C(4) || phase_a5 == UINT8_C(6) || phase_a5 == UINT8_C(8)',1)
old='''        else { instructions = UINT64_C(17251); calls = UINT64_C(117); }
    } else if (phase_a5 == UINT8_C(1)) {'''
new='''        else if (phase_a5 == UINT8_C(6)) { instructions = UINT64_C(17251); calls = UINT64_C(117); }
        else { instructions = UINT64_C(13562); calls = UINT64_C(21); }
        if (status == VF2_OK && phase_a5 == UINT8_C(8)) {
            const uint8_t fighter_zero = UINT8_C(0);
            status = vf2_model2a_write(machine, UINT32_C(0x005000a7), &fighter_zero, sizeof(fighter_zero));
        }
    } else if (phase_a5 == UINT8_C(1)) {'''
if old not in f:raise SystemExit('even accounting anchor missing')
f=f.replace(old,new,1)
old='''    } else {
        instructions = UINT64_C(1946); calls = UINT64_C(74);
    }'''
new='''    } else if (phase_a5 == UINT8_C(7)) {
        instructions = UINT64_C(1946); calls = UINT64_C(74);
    } else {
        instructions = UINT64_C(1904); calls = UINT64_C(34);
    }'''
if old not in f:raise SystemExit('odd accounting anchor missing')
f=f.replace(old,new,1)
# r25 state9 differs; r17 only state7.
f=f.replace('''                : (phase_a5 == UINT8_C(5) ? UINT32_C(0x0100135c) : UINT32_C(0x01001568))));''','''                : (phase_a5 == UINT8_C(5) ? UINT32_C(0x0100135c)
                    : (phase_a5 == UINT8_C(7) ? UINT32_C(0x01001568) : UINT32_C(0x01001074)))));''',1)
s=s[:a]+f+s[b:];p.write_text(s)
