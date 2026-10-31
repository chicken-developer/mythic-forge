"""Detail views from exported OBJ, with the procedural material palette."""
import json,hashlib
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
import build_hda as B
from render_geometry import frame,render,compare
R=Path(__file__).resolve().parent

def read_mesh(record):
    c=B.normalize(record);seed=(c['seeds']['layout']*1000003+c['seeds']['decoration'])%2147483647;m=B.Mesh(c['parameters'],seed);vertices=[];name='';mat='stone'
    for line in (R/record['outputs']['geometry']).read_text().splitlines():
        if line.startswith('v '):vertices.append(tuple(map(float,line.split()[1:])))
        elif line.startswith('g '):name=line[2:]
        elif line.startswith('usemtl '):mat=line.split()[1]
        elif line.startswith('f '):
            m.face([vertices[int(v)-1] for v in line.split()[1:]],name.split('/',1)[-1],mat);m.names[-1]=name
    return m

def main():
    manifest=json.loads((R/'manifest.json').read_text());refs={c['reference_id']:c for c in manifest['references']};font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',22) if Path('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf').exists() else ImageFont.load_default();ids=['variant_06','variant_07','variant_08','variant_09','variant_10'];out=R/'details';out.mkdir(exist_ok=True);audit=[]
    for ident in ids:
        c=json.loads((R/'datasets'/(ident+'.json')).read_text());m=read_mesh(c);direction=[-.30,.24,-1.] if ident!='variant_09' else [.32,.22,-1.]
        cam=frame([m],direction);cam['scale_label']='individual detail framing';render(m,out/(ident+'_front.png'),cam,ident+' | facade detail (individual framing)')
        compare(R/refs[c['reference_ids'][0]]['path'],out/(ident+'_front.png'),out/(ident+'_reference.png'),ident+' | individual detail framing')
        audit.append(dict(variant_id=ident,source_geometry=c['outputs']['geometry'],obj_sha256=hashlib.sha256((R/c['outputs']['geometry']).read_bytes()).hexdigest(),camera=cam,image_provenance='offline_exported_obj_preview',image='details/'+ident+'_front.png'))
        print('detail',ident,flush=True)
    im=Image.new('RGB',(1500,1890),'#eeece6');draw=ImageDraw.Draw(im)
    for i,ident in enumerate(ids):
        x=i%2*750;y=i//2*630;pic=Image.open(out/(ident+'_front.png'));pic.thumbnail((750,610));im.paste(pic,(x+(750-pic.width)//2,y+20));draw.text((x+18,y+8),ident,fill='#25323c',font=font)
    im.save(out/'architecture_overview.png');
    sp=Image.new('RGB',(2250,630),'#eeece6')
    for i,ident in enumerate(['variant_09','variant_10','variant_06']):
        pic=Image.open(out/(ident+'_front.png'));pic.thumbnail((750,630));sp.paste(pic,(i*750+(750-pic.width)//2,0))
    sp.save(out/'spire_overview.png');(out/'render_manifest.json').write_text(json.dumps(audit,indent=2))
if __name__=='__main__':main()
