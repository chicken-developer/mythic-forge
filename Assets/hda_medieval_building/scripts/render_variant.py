"""Isolated OpenGL review process; never mixes GPU baking and graphics contexts."""
from pathlib import Path
import argparse
import hashlib
import json
import sys
import hou
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
import build_hda as B
from render_review import plain_materials,setup,render

def compose(record,manifest):
    ident=record['variant_id'];hero=ROOT/record['outputs']['hero']
    im=Image.open(hero).convert('RGBA');bg=Image.new('RGBA',im.size,(224,219,207,255));bg.alpha_composite(im)
    labeled=bg.convert('RGB');ImageDraw.Draw(labeled).text((16,16),ident,fill=(30,28,24));labeled.save(hero)
    refpath=next(r['path'] for r in manifest['references'] if r['reference_id']==record['reference_ids'][0])
    ref=Image.open(ROOT/refpath).convert('RGB');ref.thumbnail((900,900))
    comp=Image.new('RGB',(1800,940),(224,219,207));comp.paste(ref,((900-ref.width)//2,(900-ref.height)//2))
    labeled.thumbnail((900,900));comp.paste(labeled,(900,0))
    ImageDraw.Draw(comp).text((20,914),'Supplied reference | Actual Houdini native geometry / base color',fill=(30,28,24))
    (ROOT/'comparisons').mkdir(exist_ok=True);comp.save(ROOT/record['outputs']['comparison'])

def main():
    ap=argparse.ArgumentParser();ap.add_argument('id');args=ap.parse_args()
    record=json.loads((ROOT/'datasets'/(args.id+'.json')).read_text(encoding='utf-8'))
    manifest=json.loads((ROOT/'manifest.json').read_text(encoding='utf-8'))
    hou.hipFile.load(str(ROOT/'medieval_building.hip'),suppress_save_prompt=True,ignore_load_warnings=True)
    b=hou.node('/obj/medieval_building');B.apply_config(b,record)
    plain_materials(b,record['parameters']['palette']=='storybook_red')
    rop,cam=setup(b,ortho=25,res=900);render(rop,ROOT/record['outputs']['hero'])
    compose(record,manifest)
    metadata={'variant_id':args.id,'effective_config':record,
              'hip_sha256':hashlib.sha256((ROOT/'medieval_building.hip').read_bytes()).hexdigest(),
              'houdini':hou.applicationVersionString(),'image_provenance':'houdini_geometry_render',
              'backend':'Houdini OpenGL ROP, separate process','camera':{'orthowidth_m':25,'resolution':[900,900],
              'position_m':list(cam.parmTuple('t').eval()),'rotation_deg':list(cam.parmTuple('r').eval())}}
    (ROOT/'previews'/(args.id+'_render.json')).write_text(json.dumps(metadata,indent=2),encoding='utf-8')
    p=ROOT/record['outputs']['result']
    if p.exists():
        result=json.loads(p.read_text(encoding='utf-8'));result['stages']['rendered']='pass'
        result['render_backend']='Houdini OpenGL ROP in isolated process';p.write_text(json.dumps(result,indent=2),encoding='utf-8')
    print('RENDER_OK',args.id,flush=True)

if __name__=='__main__':main()
