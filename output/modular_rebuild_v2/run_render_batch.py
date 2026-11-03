"""Isolate every Vulkan/OpenGL render in its own Houdini process."""
from pathlib import Path
import argparse
import json
import subprocess
import os
import sys
import math
import hashlib
from PIL import Image,ImageDraw

ROOT=Path(__file__).resolve().parent

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--phase',choices=['before','after'],required=True);ap.add_argument('--hython',type=Path,default=Path('E:/SideFX/Houdini 22.0.368/bin/hython.exe'));args=ap.parse_args()
    manifest=json.loads((ROOT/'manifest.json').read_text(encoding='utf-8'))
    records=[json.loads((ROOT/p).read_text(encoding='utf-8')) for p in ['datasets/baseline.json']+manifest['datasets']]
    failures=[]
    for record in records:
        ident=record['variant_id'];target=ROOT/f'renders/{args.phase}/{ident}_hero.png';sidecar=target.with_name(ident+'_render.json')
        geometry=ROOT/f'geometry/{args.phase}/{ident}.bgeo.sc'
        if target.exists() and sidecar.exists() and geometry.exists():
            previous=json.loads(sidecar.read_text(encoding='utf-8'))
            if previous.get('geometry_sha256')==hashlib.sha256(geometry.read_bytes()).hexdigest():continue
        log=ROOT/f'results/render_{args.phase}_{ident}.log'
        with log.open('w',encoding='utf-8') as stream:
            proc=subprocess.run([str(args.hython),str(ROOT/'render_artifacts.py'),'--phase',args.phase,'--ids',ident],stdout=stream,stderr=subprocess.STDOUT)
        okay=proc.returncode==0 and target.exists() and sidecar.exists()
        print('RENDER_PROCESS',args.phase,ident,'pass' if okay else 'fail',flush=True)
        if not okay:failures.append(ident)
    ids=[r['variant_id'] for r in records if r['variant_id'] not in failures]
    sheet=Image.new('RGB',(1200,math.ceil(len(ids)/4)*320),(225,220,209))
    for i,ident in enumerate(ids):
        im=Image.open(ROOT/f'renders/{args.phase}/{ident}_hero.png').convert('RGB');im.thumbnail((300,300));sheet.paste(im,((i%4)*300,(i//4)*320))
        ImageDraw.Draw(sheet).text(((i%4)*300+8,(i//4)*320+302),ident,fill=(20,20,20))
    sheet.save(ROOT/f'renders/{args.phase}_contact_sheet.png')
    (ROOT/f'results/render_{args.phase}.json').write_text(json.dumps({'rendered':len(ids),'failures':failures,'isolation':'one hython per dataset'},indent=2),encoding='utf-8')
    return bool(failures)

if __name__=='__main__':sys.exit(main())
