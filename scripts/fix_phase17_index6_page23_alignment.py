from pathlib import Path
p=Path('src/recovered/texture_bridge_match.c')
s=p.read_text()
# Page 2 average-time placeholders are one column left of the first approximation.
a=s.index('static vf2_status phase17_index6_render_page2('); b=s.index('static vf2_status phase17_index6_render_page3(',a); f=s[a:b]
old='{14,35,"--M --S"},{14,48,"--M --S"},{16,35,"--M --S"},{16,48,"--M --S"}'
new='{14,34,"--M --S"},{14,47,"--M --S"},{16,34,"--M --S"},{16,47,"--M --S"}'
if f.count(old)!=1: raise SystemExit('page2 placeholder anchor mismatch')
f=f.replace(old,new,1); s=s[:a]+f+s[b:]
# Page 3 TOTAL/MIN/MAX zero-time values use distinct descriptor alignment.
a=s.index('static vf2_status phase17_index6_render_page3('); b=s.index('static vf2_status execute_frame_phase17_bit7_index6(',a); f=s[a:b]
for old,new in [('{10,23,"0D  0H  0M  0S"}','{10,24,"0D  0H  0M  0S"}'),('{12,31,"0M  0S"},{13,31,"0M  0S"}','{12,32,"0M  0S"},{13,32,"0M  0S"}')]:
    if f.count(old)!=1: raise SystemExit('page3 alignment anchor mismatch: '+old)
    f=f.replace(old,new,1)
s=s[:a]+f+s[b:]
p.write_text(s)
