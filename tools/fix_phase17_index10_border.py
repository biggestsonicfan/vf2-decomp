from pathlib import Path
p=Path('src/recovered/texture_bridge_match.c')
s=p.read_text()
old='static const uint8_t horizontal_tiles[15]={19u,21u,21u,21u,21u,21u,21u,21u,15u,21u,21u,21u,15u,21u,21u};'
new='static const uint8_t horizontal_tiles[61]={19u,21u,21u,21u,21u,21u,21u,21u,15u,21u,21u,21u,15u,21u,21u,21u,15u,21u,21u,21u,15u,21u,21u,21u,15u,21u,21u,21u,15u,21u,21u,21u,15u,21u,21u,21u,15u,21u,21u,21u,15u,21u,21u,21u,15u,21u,21u,21u,15u,21u,21u,21u,15u,21u,21u,21u,15u,21u,21u,21u,18u};'
if old not in s: raise SystemExit('old table missing')
s=s.replace(old,new,1)
oldloop='for(i=0u;status==VF2_OK&&i<11u;++i)for(j=1u;status==VF2_OK&&j<62u;++j)status=phase17_index10_tile(machine,horizontal_rows[i],j,horizontal_tiles[(j-1u)/UINT32_C(4) < 15u ? (j-1u)/UINT32_C(4) : 14u]);'
newloop='for(i=0u;status==VF2_OK&&i<11u;++i)for(j=1u;status==VF2_OK&&j<62u;++j)status=phase17_index10_tile(machine,horizontal_rows[i],j,horizontal_tiles[j-1u]);'
if oldloop not in s: raise SystemExit('old loop missing')
s=s.replace(oldloop,newloop,1)
p.write_text(s)
n=Path('decomp/i960/notes/selector17_index10_vs_diagram.md')
t=n.read_text()
t += '\n## Border table\n\nThe horizontal border loop consumes the full 61-entry ROM table at `0x5fdb4..0x5fea4`, one tile per column 1..61. It begins with `0x13`, uses `0x0f` at the internal intersections, and ends with `0x12`; this is not a 15-entry pattern expanded in groups.\n'
n.write_text(t)
