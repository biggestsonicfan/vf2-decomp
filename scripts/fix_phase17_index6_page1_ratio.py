from pathlib import Path
p=Path('src/recovered/texture_bridge_match.c')
s=p.read_text()
a=s.index('static vf2_status phase17_index6_render_page1(')
b=s.index('static vf2_status execute_frame_phase17_bit7_index6(', a)
f=s[a:b]
old='{26,33,"----"}'
new='{26,35,"----"}'
if f.count(old) != 1:
    raise SystemExit('ratio anchor mismatch')
f=f.replace(old,new,1)
s=s[:a]+f+s[b:]
p.write_text(s)
