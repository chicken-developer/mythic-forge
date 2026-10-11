"""Compile actual audited pack assets, fixed trees and masks; never regenerate the blockout."""
import array
import collections
import hashlib
import json
import math
import pathlib
import random
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / 'Design/Environment/Generated'
CONFIG = ROOT / 'Design/Environment/environment.json'
sys.path.insert(0, str(ROOT / 'Tools/Blockout'))
from prepare import inside, project, png as write_png


def load(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def save(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding='utf-8')


def seed_config(audit):
    """One-time transcription of reference relationships; asset paths come only from Registry."""
    catalog = {x['path'].split('.')[-1]: x['path'] for x in audit['catalog']}
    sample = next(x for x in audit['samples'] if x['landscapes'])
    landscape = sample['landscapes'][0]
    cfg = dict(version=1, seed=42871, estimated=True, sample_map=sample['map'],
               landscape_instance=landscape['material'],
               layers={x['name']: x['info'] for x in landscape['layers'] if x['name'] != '__LANDSCAPE_VISIBILITY__'},
               grass_types={f'Grass_{i:02}': catalog[f'LandscapeGrassType_Grass_{i:02}'] for i in range(1, 5)},
               ground_basecolor=catalog['T_ground_basecolor'], ground_normal=catalog['T_ground_normal'],
               cliff_material=catalog['MI_stone_cliff_Inst'],
               grass_density_multiplier=.20, grass_start_cull_cm=3500, grass_end_cull_cm=6500,
               vegetation_start_cull_cm=65000, vegetation_end_cull_cm=100000,
               exclusion_margin_m=1.5, tree_building_margin_m=1.5, road_margin_m=1.3,
               texture_scale_m=4, sun_rotation_deg=[-31.341522, 36.412563, 0],
               capture_exposure_bias=-12, sample_exposure_bias=0,
               trial_min_m=[-140,-85,-20], trial_max_m=[-35,25,60],
               fixed_trees=[],
               zones=[dict(id='north_copse', polygon_m=[[-390,220],[365,220],[410,460],[-430,460]], attempts=500, min_spacing_m=13, density=.65),
                      dict(id='west_silhouette', polygon_m=[[-460,-340],[-300,-340],[-280,210],[-450,210]], attempts=300,min_spacing_m=12,density=.70),
                      dict(id='east_quarry_edge', polygon_m=[[300,-170],[455,-170],[455,255],[300,255]], attempts=250,min_spacing_m=15,density=.45),
                      dict(id='south_coast',polygon_m=[[-310,-440],[180,-440],[180,-275],[-310,-275]],attempts=300,min_spacing_m=14,density=.60)],
               sample_views=[dict(id='sample_overview',location_m=[-110,-140,70],target_m=[-25,-35,5],fov=65),
                             dict(id='sample_eye',location_m=[-45,-28,4],target_m=[-18,-12,3],fov=75)],
               trial_views=[dict(id='trial_patch',location_m=[-90,-3,11],target_m=[-65,-36,9],fov=75),
                            dict(id='trial_eye',location_m=[-93,-27,10],target_m=[-72,-36,9],fov=75),
                            dict(id='trial_top',location_m=[-87,-30,120],target_m=[-87,-30,8],ortho_width_m=140)],
               capture_views=[dict(id='top_down',location_m=[0,0,950],target_m=[0,0,0],ortho_width_m=980),
                              dict(id='south',location_m=[0,-690,350],target_m=[0,-70,18],fov=65),
                              dict(id='gate_to_plaza',location_m=[-12,240,2.2],target_m=[-5,135,6],fov=75),
                              dict(id='plaza',location_m=[-39,41,11],target_m=[22,-20,10],fov=75),
                              dict(id='upper_tier',location_m=[-110,-110,21],target_m=[-15,-170,20],fov=70),
                              dict(id='port_descent',location_m=[330,-310,55],target_m=[190,-200,12],fov=70),
                              dict(id='outside_vegetation',location_m=[-280,230,11],target_m=[-245,270,10],fov=75)])
    # Positions inferred from the illustration relative to plazas, roads and tier boundaries.
    anchors=[(-56,35),(54,34),(-55,-34),(55,-33),(-33,58),(31,58),(-38,-58),(37,-58),
             (-92,59),(-112,84),(-163,62),(-191,14),(-229,-73),(-271,55),(-294,115),
             (82,44),(148,48),(188,66),(254,25),(282,-50),(250,-84),
             (-70,180),(70,184),(-114,213),(112,214),(-230,153),(221,162),
             (-143,-101),(-115,-119),(-57,-155),(51,-153),(-90,-204),(73,-222),(-48,-281),(45,-287),
             (-253,-213),(-184,-267),(204,-156),(-94,-14),(-82,-51)]
    for i,p in enumerate(anchors):
        cfg['fixed_trees'].append(dict(id=f'hero_{i:03}',position_m=list(p),mesh=catalog[f'SM_tree_{[1,3,6][i%3]:02}'],scale=[.60,.52,.80][i%3],yaw_deg=(i*137.508)%360,estimated=True))
    return cfg


def main():
    audit=load(OUT/'forest_audit.json')
    if not CONFIG.exists(): save(CONFIG,seed_config(audit))
    cfg=load(CONFIG); scene=load(ROOT/'Design/Blockout/Generated/scene.json'); layout=load(ROOT/'Design/Blockout/layout.json')
    baseline=load(OUT/'baseline_manifest.json')
    assets={x['path'] for x in audit['catalog']}; meshinfo={x['path']:x for x in audit['meshes']}
    for p in [cfg['landscape_instance'],cfg['ground_basecolor'],cfg['ground_normal'],cfg['cliff_material'],*cfg['layers'].values(),*cfg['grass_types'].values()]:
        assert p in assets,p
    natural={x['id'] for x in scene['actors'] if (x['folder']=='Terrain' and x['id']!='water_datum') or (x['folder']=='Roads' and not x['id'].startswith('pier_') and x['id'] not in {s['id'] for s in scene['stairs']})}
    protected=[]
    for a in baseline['actors']:
        if not any('mesh' in c for c in a['components']): continue
        ids=[t.removeprefix('ThanhA.Id.') for t in a['tags'] if t.startswith('ThanhA.Id.')]
        if any(x in natural or x=='water_datum' for x in ids): continue
        lo=a.get('bounds_min_cm'); hi=a.get('bounds_max_cm')
        if lo and hi: protected.append((lo[0]/100,lo[1]/100,hi[0]/100,hi[1]/100,a['label']))
    n=1009; origin=-504; heights=array.array('H'); heights.frombytes((ROOT/'Design/Blockout/Generated/terrain.r16').read_bytes())
    def height(x,y):
        ix=max(0,min(n-1,round(x-origin))); iy=max(0,min(n-1,round(y-origin)))
        return (heights[iy*n+ix]-32768)*12.5/128/100
    def slope(x,y): return math.hypot((height(x+1,y)-height(x-1,y))/2,(height(x,y+1)-height(x,y-1))/2)
    routes=scene['routes']+scene['stairs']; segments=[]
    for r in routes:
        for a,b in zip(r['points_m'],r['points_m'][1:]): segments.append((a,b,r['width_m']/2))
    def clearance(x,y,radius=0,allow_slope=False):
        p=(x,y)
        if not any(inside(p,z['polygon_m']) for z in layout['land']) or height(x,y)<-.2: return False
        if any(inside(p,z['polygon_m']) for z in layout['water']+layout['no_build']): return False
        for x0,y0,x1,y1,_ in protected:
            dx=max(x0-x,0,x-x1);dy=max(y0-y,0,y-y1)
            if math.hypot(dx,dy)<radius+cfg['tree_building_margin_m']: return False
        for a,b,w in segments:
            if project(p,a,b)[0]<w+radius+cfg['road_margin_m']: return False
        for q in layout['plazas']:
            if math.dist(p,q['center_m'])<q['radius_m']+radius+cfg['road_margin_m']:return False
        return allow_slope or slope(x,y)<.48
    def radius(mesh,scale):
        m=meshinfo[mesh]; return max(abs(v) for v in m['bounds_min_cm'][:2]+m['bounds_max_cm'][:2])*scale/100
    # Fixed IDs/coordinates are an independent editable input. Resolve the initial sketch once only.
    if not cfg.get('fixed_trees_resolved'):
        valid=[]; omitted=[]
        for t in cfg['fixed_trees']:
            x,y=t['position_m']; rr=radius(t['mesh'],t['scale']); placed=False
            for dist in [0,3,6,9,12,16,20]:
                for k in range(1 if dist==0 else 16):
                    p=[x+dist*math.cos(k*math.tau/16),y+dist*math.sin(k*math.tau/16)]
                    if clearance(*p,rr) and all(math.dist(p,v['position_m'])>rr+radius(v['mesh'],v['scale'])+1 for v in valid):
                        t['sketch_position_m']=t['position_m'];t['position_m']=[round(v,3) for v in p];valid.append(t);placed=True;break
                if placed:break
            if not placed:omitted.append(t['id'])
        cfg['fixed_trees']=valid;cfg['fixed_trees_resolved']=True;cfg['omitted_sketch_ids']=omitted;save(CONFIG,cfg)
    fixed=cfg['fixed_trees']; instances=[]; centers=[]; rng=random.Random(cfg['seed'])
    def add(id,p,mesh,scale,yaw,kind,collision,embed=.15):
        assert mesh in meshinfo
        instances.append(dict(id=id,position_m=[*p,height(*p)],mesh=mesh,scale=scale,yaw_deg=yaw,kind=kind,collision=collision,embed_fraction=embed))
    for t in fixed:
        assert clearance(*t['position_m'],radius(t['mesh'],t['scale'])),f"Fixed tree violates mask: {t['id']}"
        add(t['id'],t['position_m'],t['mesh'],t['scale'],t['yaw_deg'],'hero_tree',True)
        centers.append((t['position_m'],radius(t['mesh'],t['scale'])))
    names={x['path'].split('.')[-1]:x['path'] for x in audit['catalog']}
    for zone in cfg['zones']:
        poly=zone['polygon_m'];xs=[p[0] for p in poly];ys=[p[1] for p in poly]
        for i in range(zone['attempts']):
            x=rng.uniform(min(xs),max(xs));y=rng.uniform(min(ys),max(ys))
            # Broad copses with openings; deliberately avoid a uniform forest carpet.
            if not inside((x,y),poly) or rng.random()>zone['density'] or math.sin(x*.032)+math.cos(y*.043)<-.30:continue
            mesh=names[f'SM_tree_{rng.choice([1,2,3,6]):02}'];scale=rng.uniform(.55,.85);rr=radius(mesh,scale)
            if not clearance(x,y,rr) or any(math.dist((x,y),p)<max(zone['min_spacing_m'],rr+r+1) for p,r in centers):continue
            p=[round(x,3),round(y,3)]; add(f"{zone['id']}_{i:03}",p,mesh,scale,rng.uniform(0,360),'secondary_tree',True);centers.append((p,rr))
    # Smaller growth is concentrated around curated clusters, with individual mesh radii respected.
    for i,(p,rr) in enumerate(centers):
        for j in range(14):
            a=rng.random()*math.tau;dist=rng.uniform(2,rr+10);q=[p[0]+dist*math.cos(a),p[1]+dist*math.sin(a)]
            if j<3: mesh=names['SM_tree_shrub'];scale=rng.uniform(.65,1.1);kind='small_tree'
            elif j<10:mesh=names[rng.choice(['SM_shrub_01','SM_shrub_02','SM_shrub_flower_01'])];scale=rng.uniform(.6,1);kind='shrub'
            else:mesh=names[f'SM_small_stone_{rng.randrange(1,5):02}'];scale=rng.uniform(.9,1.6);kind='small_rock'
            r=radius(mesh,scale)
            if clearance(*q,r) and all(math.dist(q,v['position_m'][:2])>r+radius(v['mesh'],v['scale'])+.3 for v in instances if v['kind'] in ['small_tree','shrub','small_rock']):
                add(f'cluster_{i:03}_{j:02}',[round(v,3) for v in q],mesh,scale,rng.random()*360,kind,False)
    # Natural coastline/cliff accents. Retaining proxies remain collision substrates and gain pack rock material.
    rock_count=0
    for edge in layout['retaining_walls']+[dict(id='coastline',points_m=layout['land'][0]['polygon_m']+[layout['land'][0]['polygon_m'][0]])]:
        for a,b in zip(edge['points_m'],edge['points_m'][1:]):
            steps=max(1,round(math.dist(a[:2],b[:2])/7))
            for j in range(steps+1):
                t=j/steps;x=a[0]+(b[0]-a[0])*t;y=a[1]+(b[1]-a[1])*t
                mesh=names[f'SM_stone_{rng.randrange(1,7):02}'];scale=rng.uniform(1.1,1.8);r=radius(mesh,scale)
                # Offset the accent onto exposed ground instead of intersecting structural walls.
                found=None
                for off in [0,4,-4,8,-8]:
                    length=math.dist(a[:2],b[:2]);q=[x-(b[1]-a[1])/length*off,y+(b[0]-a[0])/length*off]
                    if clearance(*q,r,allow_slope=True):found=q;break
                if found and all(math.dist(found,v['position_m'][:2])>r+radius(v['mesh'],v['scale'])*.5 for v in instances if v['kind']=='cliff_rock'):
                    add(f'cliff_{rock_count:04}',[round(v,3) for v in found],mesh,scale,rng.uniform(0,360),'cliff_rock',True,.25);rock_count+=1
    # Rasterize a signed distance to walkable/protected surfaces at the actual Landscape resolution.
    exclusion=bytearray(n*n);pathdistance=array.array('f',[1000])*(n*n)
    for x0,y0,x1,y1,_ in protected:
        margin=cfg['exclusion_margin_m']+1
        for iy in range(max(0,math.floor(y0-margin-origin)),min(n,math.ceil(y1+margin-origin)+1)):
            for ix in range(max(0,math.floor(x0-margin-origin)),min(n,math.ceil(x1+margin-origin)+1)):
                if math.hypot(max(x0-(ix+origin),0,ix+origin-x1),max(y0-(iy+origin),0,iy+origin-y1))<=margin:exclusion[iy*n+ix]=255
    for a,b,w in segments:
        margin=w+cfg['road_margin_m']+3
        for iy in range(max(0,math.floor(min(a[1],b[1])-margin-origin)),min(n,math.ceil(max(a[1],b[1])+margin-origin)+1)):
            for ix in range(max(0,math.floor(min(a[0],b[0])-margin-origin)),min(n,math.ceil(max(a[0],b[0])+margin-origin)+1)):
                dist=project((ix+origin,iy+origin),a,b)[0]-w;idx=iy*n+ix
                if dist<pathdistance[idx]:pathdistance[idx]=dist
                if dist<cfg['road_margin_m']+1:exclusion[idx]=255
    for plaza in layout['plazas']:
        x,y=plaza['center_m'];r=plaza['radius_m']+cfg['road_margin_m']+2
        for iy in range(max(0,math.floor(y-r-origin)),min(n,math.ceil(y+r-origin)+1)):
            for ix in range(max(0,math.floor(x-r-origin)),min(n,math.ceil(x+r-origin)+1)):
                if math.dist((ix+origin,iy+origin),(x,y))<=r:exclusion[iy*n+ix]=255
    tree_dirt=bytearray(n*n)
    for t in instances:
        if t['kind'] not in ['hero_tree','secondary_tree']:continue
        x,y=t['position_m'][:2]
        for iy in range(max(0,math.floor(y-2-origin)),min(n,math.ceil(y+2-origin)+1)):
            for ix in range(max(0,math.floor(x-2-origin)),min(n,math.ceil(x+2-origin)+1)):
                blend=max(0,min(1,(2-math.dist((ix+origin,iy+origin),(x,y)))/1.15))
                tree_dirt[iy*n+ix]=max(tree_dirt[iy*n+ix],round(blend*255))
    layers={k:bytearray(n*n) for k in cfg['layers']}; grass_pixels=0; exclusions_with_grass=0
    for iy in range(n):
        for ix in range(n):
            idx=iy*n+ix;x=ix+origin;y=iy+origin;h=height(x,y);sl=slope(x,y)
            blocked=bool(exclusion[idx]) or h<-.2 or any(inside((x,y),q['polygon_m']) for q in layout['water']+layout['no_build'])
            cliff=round(255*max(0,min(1,(sl-.35)/.8)))
            dirt=255 if blocked else max(tree_dirt[idx],round(255*max(0,min(1,(4-pathdistance[idx])/3))))
            if not blocked and h<1.5:dirt=max(dirt,round(155*(1-h/1.5)))
            grass=0 if blocked else 255-max(dirt,cliff); ground=255-grass-cliff
            if ground<0:cliff=255-dirt;ground=dirt
            layers['Grass_Cliff'][idx]=grass;layers['Ground'][idx]=ground;layers['Cliff'][idx]=cliff;layers['Grass'][idx]=0
            if grass>0:grass_pixels+=1
            if blocked and grass:exclusions_with_grass+=1
    assert exclusions_with_grass==0
    for name,values in layers.items():(OUT/f'{name}.raw').write_bytes(values)
    (OUT/'exclusion.raw').write_bytes(exclusion)
    fingerprint=hashlib.sha256(CONFIG.read_bytes()+pathlib.Path(__file__).read_bytes()+(ROOT/'Source/ThanhABlockoutEditor/ThanhAEnvironment.cpp').read_bytes()+(OUT/'baseline_manifest.json').read_bytes()).hexdigest()[:16]
    compiled=dict(version=1,fingerprint=fingerprint,config_md5=hashlib.md5(CONFIG.read_bytes()).hexdigest(),seed=cfg['seed'],landscape_resolution=n,instances=instances,natural_proxy_ids=sorted(natural),baseline_sha256=hashlib.sha256((OUT/'baseline_manifest.json').read_bytes()).hexdigest(),heightmap_md5=hashlib.md5((ROOT/'Design/Blockout/Generated/terrain.r16').read_bytes()).hexdigest())
    save(OUT/'environment_compiled.json',compiled)
    # An exported mask is an engineering visualization, not a substitute for Unreal captures.
    rows=[]
    for iy in reversed(range(n)):
        row=bytearray()
        for ix in range(n):
            i=iy*n+ix;g=layers['Grass_Cliff'][i];d=layers['Ground'][i];c=layers['Cliff'][i]
            row.extend((int(.26*g+.63*d+.38*c),int(.48*g+.48*d+.40*c),int(.12*g+.28*d+.44*c)))
        rows.append(bytes(row))
    write_png(OUT/'surface_mask.png',n,n,rows,8,2)
    report=dict(status='pass',instances=dict(collections.Counter(v['kind'] for v in instances)),grass_weight_pixels=grass_pixels,excluded_grass_pixels=exclusions_with_grass,protected_footprints=len(protected),estimated=True,omitted_sketch_ids=cfg.get('omitted_sketch_ids',[]),fingerprint=fingerprint)
    save(OUT/'design_validation.json',report);print(json.dumps(report,ensure_ascii=False))


if __name__=='__main__':main()
