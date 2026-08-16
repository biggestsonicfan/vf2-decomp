from pathlib import Path
p=Path('src/recovered/texture_bridge_match.c')
s=p.read_text()
a=s.index('static vf2_status phase17_index6_render_page1(')
b=s.index('static vf2_status execute_frame_phase17_bit7_index6(', a)
f=s[a:b]
old='''    if (status == VF2_OK && characters != NULL) *characters = count;\n    return status;\n}\n\n'''
new=r'''    if (status == VF2_OK) {
        static const struct {
            uint8_t state;
            uint8_t row;
            uint8_t first;
            uint8_t last;
        } spans[] = {
            {0,2,26,36},{0,2,38,44},{0,5,26,36},
            {0,8,10,31},{0,10,10,31},{0,12,10,31},{0,15,10,31},
            {0,17,10,31},{0,19,10,31},{0,22,10,31},{0,24,10,31},
            {0,26,10,31},{0,29,10,31},{0,31,10,31},{0,33,10,31},
            {0,35,10,31},{0,36,10,31},{0,37,10,31},{0,38,10,31},
            {0,39,10,31},{0,40,10,31},{0,44,15,46},{0,45,18,42},
            {1,2,26,36},{1,2,38,44},{1,5,26,36},
            {1,8,10,31},{1,8,33,38},{1,10,10,31},{1,10,33,38},
            {1,12,10,31},{1,12,33,38},{1,15,10,31},{1,15,33,38},
            {1,17,10,31},{1,17,33,38},{1,19,10,31},{1,19,33,38},
            {1,22,10,31},{1,22,33,51},{1,24,10,31},{1,24,33,51},
            {1,26,10,31},{1,26,33,38},{1,29,10,31},{1,29,33,38},
            {1,31,10,31},{1,31,33,38},{1,33,10,31},{1,33,33,38},
            {1,35,10,31},{1,35,33,51},{1,36,10,31},{1,36,33,51},
            {1,37,10,31},{1,37,33,51},{1,38,10,31},{1,38,33,43},
            {1,39,10,31},{1,39,33,43},{1,40,10,31},{1,40,33,43},
            {1,44,15,46},{1,45,18,42}
        };
        uint32_t row = 0u;
        uint32_t col = 0u;
        for (row = 0u; status == VF2_OK && row < UINT32_C(48); ++row) {
            for (col = 0u; status == VF2_OK && col < UINT32_C(64); ++col) {
                uint16_t cell = 0u;
                const uint32_t address = UINT32_C(0x01000000) +
                    row * UINT32_C(0x80) + col * UINT32_C(2);
                status = read_u16(machine, address, &cell);
                if (status == VF2_OK) {
                    status = write_u16(machine, address, cell & UINT16_C(0x7fff));
                }
            }
        }
        for (i = 0u; status == VF2_OK && i < sizeof(spans)/sizeof(spans[0]); ++i) {
            if (spans[i].state == state) {
                for (col = spans[i].first; status == VF2_OK && col <= spans[i].last; ++col) {
                    uint16_t cell = 0u;
                    const uint32_t address = UINT32_C(0x01000000) +
                        (uint32_t)spans[i].row * UINT32_C(0x80) + col * UINT32_C(2);
                    status = read_u16(machine, address, &cell);
                    if (status == VF2_OK) {
                        status = write_u16(machine, address, cell | UINT16_C(0x8000));
                    }
                }
            }
        }
    }
    if (status == VF2_OK && characters != NULL) *characters = count;
    return status;
}

'''
if old not in f:
    raise SystemExit('renderer tail missing')
f=f.replace(old,new,1)
s=s[:a]+f+s[b:]
p.write_text(s)
