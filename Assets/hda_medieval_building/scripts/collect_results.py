"""Verify on-disk evidence and produce a review sheet and current batch manifest."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import hou
import numpy as np
from PIL import Image,ImageDraw,ImageFont
ROOT=Path(__file__).resolve().parents[1];P=ROOT.parent/'_render';DEST=P/'SourceAssets/Medieval15';OUT=P/'RenderTests/Medieval15'

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def sheet(records):
    font=ImageFont.truetype('C:/Windows/Fonts/arial.ttf',20)
    image=Image.new('RGB',(2500,1650),(224,219,207));draw=ImageDraw.Draw(image)
    for i,r in enumerate(records):
        im=Image.open(ROOT/r['outputs']['hero']).convert('RGB');im.thumbnail((500,500))
        x=i%5*500;y=i//5*550;image.paste(im,(x+(500-im.width)//2,y))
        draw.text((x+12,y+500),r['variant_id'],fill=(30,28,24),font=font)
        p=r['parameters'];draw.text((x+12,y+525),f"{p['floor_count']}F | {p['width_m']} x {p['depth_m']} m | {p['footprint']}",fill=(60,55,48),font=font)
    OUT.mkdir(parents=True,exist_ok=True);image.save(OUT/'houdini_contact_sheet.png')

def inspect_geometry(path):
    geo=hou.Geometry();geo.loadFromFile(str(path))
    normals=np.asarray(geo.vertexFloatAttribValues('N')).reshape(-1,3)
    uv=np.asarray(geo.vertexFloatAttribValues('uv')).reshape(-1,3)
    points=np.asarray(geo.pointFloatAttribValues('P')).reshape(-1,3)
    lengths=np.linalg.norm(normals,axis=1)
    nonpacked=geo.findPrimGroup('nonpacked')
    return {'primitives':len(geo.prims()),'invalid_normals':int(np.count_nonzero(np.abs(lengths-1)>1e-3)),
            'finite_attributes':bool(np.isfinite(normals).all() and np.isfinite(uv).all() and np.isfinite(points).all()),
            'uv_bounds':[uv[:,:2].min(axis=0).tolist(),uv[:,:2].max(axis=0).tolist()],
            'nonpacked_primitives':len(nonpacked.prims()) if nonpacked else 0}

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--previews-only',action='store_true');args=ap.parse_args()
    manifest=json.loads((ROOT/'manifest.json').read_text(encoding='utf-8'))
    records=[json.loads((ROOT/p).read_text(encoding='utf-8')) for p in manifest['datasets']]
    sheet(records)
    if args.previews_only:return
    current_sha=sha(ROOT/'medieval_building.hip');rows=[];failures=[]
    equivalence_path=ROOT/'results/basecolor_scene_equivalence.json'
    equivalence=json.loads(equivalence_path.read_text()) if equivalence_path.exists() else {}
    def equivalent(export_sha,record):
        proof=equivalence.get('geometry_equivalence',{}).get(record['variant_id'],{})
        cfg=hashlib.sha256(json.dumps(record,sort_keys=True).encode()).hexdigest()
        return (equivalence.get('status')=='pass' and equivalence.get('after_hip_sha256')==current_sha and
                equivalence.get('before_hip_sha256')==export_sha and proof.get('config_sha256')==cfg and
                proof.get('before')==proof.get('after') and bool(proof.get('before')))
    engine_path=OUT/'unreal_import_test.json'
    engine=json.loads(engine_path.read_text()) if engine_path.exists() else {}
    imported={r['variant_id']:r for r in engine.get('assets',[])} if engine.get('status')=='pass' else {}
    for rec in [json.loads((ROOT/manifest['baseline']).read_text(encoding='utf-8'))]+records:
        ident=rec['variant_id'];path=ROOT/rec['outputs']['result']
        try:
            r=json.loads(path.read_text(encoding='utf-8'))
            if (r['hip_sha256']!=current_sha and not equivalent(r['hip_sha256'],rec)) or r['effective_config']!=rec:raise ValueError('Stale HIP/config evidence')
            render=json.loads((ROOT/'previews'/(ident+'_render.json')).read_text(encoding='utf-8'))
            if (render['hip_sha256']!=current_sha and not equivalent(render['hip_sha256'],rec)) or render['effective_config']!=rec:raise ValueError('Stale render')
            if r['hip_sha256']!=current_sha:
                r['current_authoring_hip_sha256']=current_sha;r['material_only_equivalence_report']='results/basecolor_scene_equivalence.json'
            if Image.open(r['files']['normal']).size!=(4096,4096):raise ValueError('Normal is not 4K')
            if sha(r['files']['normal'])!=r['bake']['normal_sha256']:raise ValueError('Normal hash differs')
            for key in ('mesh','high_mesh'):
                if not Path(r['files'][key]).is_file():raise ValueError('Missing '+key)
            low=inspect_geometry(ROOT/rec['outputs']['geometry']);high=inspect_geometry(DEST/'High'/(ident+'.bgeo.sc'))
            if low['primitives']!=r['low']['primitives'] or high['primitives']!=r['high']['primitives']:raise ValueError('Geometry count mismatch')
            if any(g['invalid_normals'] or not g['finite_attributes'] for g in (low,high)):raise ValueError(('Invalid attributes',low,high))
            if low['nonpacked_primitives']:raise ValueError('UV islands not packed')
            r['stages']['rendered']='pass';r['geometry_readback']={'low':low,'high':high}
            r['fbx_sha256']=sha(r['files']['mesh']);r['high_fbx_sha256']=sha(r['files']['high_mesh'])
            if ident!='baseline' and ident in imported:
                if imported[ident]['triangles']!=r['low']['primitives']:raise ValueError('Unreal count differs')
                r['stages']['unreal_imported']='pass'
            path.write_text(json.dumps(r,indent=2),encoding='utf-8');rows.append(r)
            print('VERIFIED',ident,low['primitives'],high['primitives'],flush=True)
        except Exception as exc:failures.append({'variant_id':ident,'error':str(exc)})
    summary={'status':'pass' if not failures else 'fail','requested_variants':15,'baseline_separate':True,
             'verified_variants':len([r for r in rows if r['variant_id']!='baseline']),'hip_sha256':current_sha,
             'hda_sha256':sha(ROOT/'asset/medieval_house_layout.hda'),'failures':failures,
             'material_only_source_equivalence':equivalence.get('status')=='pass' and equivalence.get('after_hip_sha256')==current_sha,
             'variants':[{'id':r['variant_id'],'low_triangles':r['low']['primitives'],'high_triangles':r['high']['primitives'],
                          'stages':r['stages'],'mesh':Path(r['files']['mesh']).relative_to(P.parents[1]).as_posix(),
                          'normal':Path(r['files']['normal']).relative_to(P.parents[1]).as_posix()} for r in rows]}
    (ROOT/'results/summary.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
    (DEST/'mesh_manifest.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
    (DEST/'Houdini').mkdir(exist_ok=True);shutil.copy2(ROOT/'asset/medieval_house_layout.hda',DEST/'Houdini/medieval_house_layout.hda')
    (OUT/'houdini_bake_test.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
    if failures:print(json.dumps(failures,indent=2));raise SystemExit(1)

if __name__=='__main__':main()
