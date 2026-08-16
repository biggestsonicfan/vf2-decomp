from pathlib import Path
p=Path('src/recovered/texture_bridge_match.c')
s=p.read_text()
old='if(status==VF2_OK&&characters!=NULL)*characters=count; return status;'
new='''if (status == VF2_OK && characters != NULL) {
        *characters = count;
    }
    return status;'''
if s.count(old) != 1:
    raise SystemExit('gcc cleanup anchor mismatch')
s=s.replace(old,new,1)
p.write_text(s)
