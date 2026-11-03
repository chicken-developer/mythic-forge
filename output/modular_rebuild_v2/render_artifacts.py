"""Houdini OpenGL only: render the actual saved cook, then compose QA labels."""
from pathlib import Path
import argparse
import json
import math
import hashlib
import hou
from PIL import Image,ImageDraw,ImageFont
from hda_builder.evidence import ROOT,write_json,sha

def common_camera():
    rows=[json.loads(p.read_text(encoding='utf-8'))['metrics'] for p in (ROOT/'results/before').glob('*.json')]
    lo=[min(r['bounds_min'][i] for r in rows) for i in range(3)]
    hi=[max(r['bounds_max'][i] for r in rows) for i in range(3)]
    target=hou.Vector3([(a+b)*.5 for a,b in zip(lo,hi)])
    span=max(hi[i]-lo[i] for i in range(3))
    eye=target+hou.Vector3(15,11,-19)
    delta=target-eye
    rotation=(math.degrees(math.atan2(delta[1],math.hypot(delta[0],delta[2]))),math.degrees(math.atan2(-delta[0],-delta[2])),0.)
    return {'projection':'orthographic','common_bounds_min':lo,'common_bounds_max':hi,
            'target':list(target),'position':list(eye),'rotation':rotation,'orthowidth':max(25,span*1.5),'resolution':[900,900]}

def compose(ident, phase, record, manifest):
    path=ROOT/f'renders/{phase}/{ident}_hero.png'
    src=Image.open(path).convert('RGBA');canvas=Image.new('RGBA',src.size,(225,220,209,255));canvas.alpha_composite(src)
    labeled=canvas.convert('RGB');draw=ImageDraw.Draw(labeled)
    draw.rectangle((0,0,900,36),fill=(225,220,209));draw.text((12,12),ident+' | '+phase+' | Houdini 22 geometry',fill=(25,25,25))
    labeled.save(path)
    if phase=='before':return
    refrow=next(r for r in manifest['references'] if r['reference_id']==record['reference_ids'][0])
    reference=Image.open(ROOT/refrow['path']).convert('RGB');reference.thumbnail((900,900))
    before=Image.open(ROOT/f'renders/before/{ident}_hero.png').convert('RGB')
    comp=Image.new('RGB',(2700,940),(225,220,209))
    comp.paste(reference,((900-reference.width)//2,(900-reference.height)//2));comp.paste(before,(900,0));comp.paste(labeled,(1800,0))
    draw=ImageDraw.Draw(comp);draw.text((16,912),'Input reference | Original cooked geometry | Modular cooked geometry - common camera',fill=(25,25,25))
    comp.save(ROOT/f'comparisons/{ident}.png')
    # Original dataset outputs remain valid and point to genuine hero/comparison images.
    hero=ROOT/record['outputs']['hero'];hero.parent.mkdir(exist_ok=True);labeled.save(hero)
    comp.save(ROOT/record['outputs']['comparison'])

def contact_sheet(phase, ids):
    sheet=Image.new('RGB',(1200,math.ceil(len(ids)/4)*320),(225,220,209))
    for i,ident in enumerate(ids):
        im=Image.open(ROOT/f'renders/{phase}/{ident}_hero.png').convert('RGB');im.thumbnail((300,300))
        sheet.paste(im,((i%4)*300,(i//4)*320));ImageDraw.Draw(sheet).text(((i%4)*300+8,(i//4)*320+302),ident,fill=(20,20,20))
    sheet.save(ROOT/f'renders/{phase}_contact_sheet.png')

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--phase',choices=['before','after'],required=True);ap.add_argument('--ids',nargs='*');ap.add_argument('--checker',action='store_true');args=ap.parse_args()
    manifest=json.loads((ROOT/'manifest.json').read_text(encoding='utf-8'))
    records=[json.loads((ROOT/p).read_text(encoding='utf-8')) for p in ['datasets/baseline.json']+manifest['datasets']]
    if args.ids:records=[r for r in records if r['variant_id'] in args.ids]
    folder=ROOT/f'renders/{args.phase}';folder.mkdir(exist_ok=True,parents=True)
    settings=common_camera();write_json(ROOT/'results/camera.json',settings)
    obj=hou.node('/obj').createNode('geo','ACTUAL_COOK',run_init_scripts=False)
    file=obj.createNode('file','read_actual_cooked_geometry')
    material=obj.createNode('attribwrangle','review_materials');material.setInput(0,file);material.parm('class').set(1)
    # Geometry positions/topology are untouched; use stored base-color Cd without file textures.
    material.parm('snippet').set('s@shop_materialpath="";')
    if args.checker:
        checker=ROOT/'renders/checker_texture.png'
        texture=Image.new('RGB',(128,128),'white');draw=ImageDraw.Draw(texture)
        draw.rectangle((0,0,63,63),fill=(55,55,55));draw.rectangle((64,64,127,127),fill=(55,55,55));texture.save(checker)
        matnet=obj.createNode('matnet','QA_materials');shader=matnet.createNode('principledshader::2.0','uv_checker')
        shader.parmTuple('basecolor').set((1,1,1));shader.parm('basecolor_useTexture').set(1);shader.parm('basecolor_texture').set(str(checker))
        shader.parm('rough').set(.9);shader.parm('reflect').set(.05)
        material.parm('snippet').set('s@shop_materialpath="'+shader.path()+'";v@Cd={1,1,1};')
    material.setDisplayFlag(True);material.setRenderFlag(True)
    cam=hou.node('/obj').createNode('cam','QA_camera');cam.parmTuple('t').set(settings['position']);cam.parmTuple('r').set(settings['rotation'])
    cam.parm('projection').set('ortho');cam.parm('orthowidth').set(settings['orthowidth']);cam.parmTuple('res').set(settings['resolution'])
    for name,pos,power in [('key',(-8,15,-10),1.1),('fill',(10,10,-3),.7),('rim',(1,14,12),.7)]:
        light=hou.node('/obj').createNode('hlight','QA_'+name)
        light.parm('light_type').set('distant');light.parm('light_intensity').set(power);light.parmTuple('t').set(pos)
        light.parmTuple('r').set(hou.Vector3(0,0,-1).matrixToRotateTo(hou.Vector3(settings['target'])-hou.Vector3(pos)).extractRotates())
    rop=hou.node('/out').createNode('opengl','QA_render');rop.parm('camera').set(cam.path());rop.parm('vobjects').set(obj.path());rop.parm('alights').set('QA_*')
    rop.parm('usetextures').set(int(args.checker));rop.parm('hqlighting').set(1)
    failures=[]
    for record in records:
        ident=record['variant_id'];geometry=ROOT/f'geometry/{args.phase}/{ident}.bgeo.sc'
        if not geometry.exists():failures.append(ident);continue
        try:
            file.parm('file').set(str(geometry));material.cook(force=True)
            image=folder/f'{ident}_hero.png'
            if args.checker:
                (ROOT/'renders/checker').mkdir(exist_ok=True);image=ROOT/f'renders/checker/{ident}_uv.png'
            rop.parm('picture').set(str(image));rop.render()
            if not args.checker:compose(ident,args.phase,record,manifest)
            sidecar=folder/f'{ident}_render.json' if not args.checker else image.with_suffix('.json')
            write_json(sidecar,{'variant_id':ident,'effective_config':record,'phase':args.phase,'uv_checker':args.checker,
                     'image_provenance':'houdini_geometry_render','backend':'Houdini 22 OpenGL ROP',
                     'houdini':hou.applicationVersionString(),'geometry':str(geometry.relative_to(ROOT)).replace('\\','/'),
                     'geometry_sha256':sha(geometry),'image_sha256':sha(image),'camera':settings,
                     'lighting':'three fixed distant lights; textures disabled; original Cd'})
            print('RENDER',args.phase,ident,flush=True)
        except Exception as exc:
            failures.append(ident);print('RENDER_FAIL',ident,str(exc),flush=True)
    if not failures and not args.checker:contact_sheet(args.phase,[r['variant_id'] for r in records])
    if not args.checker:write_json(ROOT/f'results/render_{args.phase}.json',{'rendered':len(records)-len(failures),'failures':failures,'camera':settings})
    return bool(failures)

if __name__=='__main__':raise SystemExit(main())
