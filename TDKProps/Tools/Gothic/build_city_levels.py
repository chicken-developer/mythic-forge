"""Create the playable Gothic city and a complete 28-type geometry catalog."""
from pathlib import Path
import json
import math
import sys
import traceback
import importlib
ROOT=Path('E:/Repos/mythic-forge');sys.path.insert(0,str(ROOT/'TDKProps/Tools/Gothic'))
import city_scene as C
C=importlib.reload(C)
import unreal as u

def build_catalog(records):
    level='/Game/Gothic/Maps/L_Gothic_Catalog';C.begin(level)
    floor=C.flat('M_Catalog_Floor',(.08,.10,.115));base=C.flat('M_Catalog_Plinth',(.15,.17,.18))
    clay=u.load_asset('/Game/Gothic/FacadeReview/Materials/M_Facade_Clay') or C.flat('M_Catalog_Clay',(.24,.24,.24),.52)
    C.cube('Catalog_Ground',(0,10000,-30),(59000,40000,60),floor,'Studio')
    entries=[]
    for i,row in enumerate(records):
        x=((i%7)-3)*7200;y=(i//7)*7800
        asset=u.load_asset('/Game/Gothic/Buildings/SM_Gothic_'+row['id']);assert asset,row['id']
        a=C.mesh(row['id']+' | '+row['label'],asset,(x,y,20),folder='Catalog/'+row['district'])
        a.tags=[C.TAG,'GothicCatalog',row['id']]
        for slot in range(len(asset.static_materials)):a.static_mesh_component.set_material(slot,clay)
        w,h,d=[s*100 for s in row['size_m']]
        C.cube('Plinth_'+row['id'],(x,y,8),(max(w+160,1500),d+180,24),base,'Studio')
        label=C.spawn(u.TextRenderActor,'Label_'+row['id'],(x,y-d/2-140,32),(0,-90,0),'Labels')
        tc=label.text_render;tc.set_text(row['id'][-2:]+' / '+row['label'].upper());tc.set_world_size(65.)
        tc.set_horizontal_alignment(u.HorizTextAligment.EHTA_CENTER);tc.set_text_render_color(u.Color(220,224,225,255))
        radius=max(w,d,h)*.8
        C.camera('Type_'+row['id'][-2:],(x+radius*1.1,y-radius*2.9,h*.6+radius*1.6),(x,y,h*.45+20),30.)
        entries.append({'id':row['id'],'actor':a.get_path_name(),'location_cm':[x,y,20]})
    C.lighting();C.spawn(u.PlayerStart,'PlayerStart_Catalog',(0,-4200,250),(0,90,0),'Gameplay')
    hero=C.camera('Catalog_Overview',(-8000,-42000,42000),(0,11000,1000),38.)
    result=C.finish(hero,level,{'status':'passed','types':28,'buildings':entries})
    (C.R/'catalog_scene.json').write_text(json.dumps(result,indent=2))

def build_city(records):
    data={r['id']:r for r in records};layout=json.loads((ROOT/'TDKProps/SourceAssets/Gothic/city_layout.json').read_text())
    level='/Game/Gothic/Maps/L_Gothic_City';C.begin(level)
    ground=C.flat('M_City_Ground',(.12,.145,.12),.95)
    roadmat=C.flat('M_City_Road',(.105,.115,.13),.88,paving=True)
    sidewalk=C.flat('M_City_Paving',(.27,.26,.235),.85,paving=True)
    grass=C.flat('M_City_CourtyardGrass',(.10,.15,.075),.95)
    C.cube('City_Terrain',(0,0,-30),(600000,600000,60),ground,'Terrain')
    for i,road in enumerate(layout['roads']):
        a,b=road['a'],road['b'];vertical=a[0]==b[0];length=math.dist(a,b)*100;w=road['width']*100
        x=(a[0]+b[0])*50;y=(a[1]+b[1])*50
        C.cube(road['name'],(x,y,1),(w if vertical else length,length if vertical else w,2),roadmat,'Streets/Roads')
        for sign in (-1,1):
            xx=x+sign*(w/2+100) if vertical else x;yy=y if vertical else y+sign*(w/2+100)
            C.cube(road['name']+'_Pavement_'+str(sign),(xx,yy,9),(200 if vertical else length,length if vertical else 200,18),sidewalk,'Streets/Sidewalks')
    # Broad pedestrian square, clear of carriageways, and a cathedral forecourt.
    C.cube('Civic_Square',(0,0,9),(6000,6000,18),sidewalk,'Public Spaces')
    C.cube('Cathedral_Forecourt',(0,14500,8),(10000,4500,16),sidewalk,'Public Spaces')
    for i,park in enumerate(layout['parks']):
        x,y=park['center_m'];w,d=park['size_m']
        C.cube('Courtyard_'+str(i),(x*100,y*100,8),(w*100,d*100,16),grass,'Public Spaces/Courtyards')
    actors=[]
    for i,b in enumerate(layout['buildings']):
        mesh=u.load_asset('/Game/Gothic/Buildings/SM_Gothic_'+b['id']);assert mesh,b['id']
        loc=tuple(x*100 for x in b['location_m'])
        a=C.mesh(f'{i+1:03d} | '+b['label'],mesh,loc,b['yaw'],'Buildings/'+b['district'])
        a.tags=[C.TAG,'GothicCityBuilding',b['id'],b['district'],b['role']]
        actors.append({'index':i,'id':b['id'],'actor':a.get_path_name(),'district':b['district'],'role':b['role'],'location_cm':list(loc),'yaw':b['yaw']})
    meshes={name:u.load_asset('/Game/Gothic/City/Context/SM_City_'+name) for name in ('Lamp','Bench','Fountain','Planter','Cypress','Wall')}
    assert all(meshes.values()),'Context kit incomplete'
    # Actors share the same mesh assets and remain individually editable in the sample.
    counts={name:0 for name in meshes}
    for prop in layout['props']+layout['walls']:
        pos=[v*100 for v in prop['location_m']]
        if prop['type']!='Wall':pos[2]-=2.
        name=prop['type'];counts[name]+=1
        a=C.mesh(name+'_'+str(counts[name]),meshes[name],tuple(pos),prop['yaw'],'Context/'+name)
        a.tags=[C.TAG,'GothicCityContext',name]
    for i,p in enumerate([p for p in layout['props'] if p['type']=='Lamp'][::8]):
        x,y,z=p['location_m'];light=C.spawn(u.PointLight,'Lantern_Glow_'+str(i),(x*100,y*100,(z+3.6)*100),folder='Lighting/Lanterns')
        light.light_component.set_editor_property('intensity',70.);light.light_component.set_editor_property('attenuation_radius',400.)
        light.light_component.set_editor_property('light_color',u.Color(255,175,93,255));light.light_component.set_editor_property('cast_shadows',False)
    C.lighting()
    C.spawn(u.PlayerStart,'PlayerStart_SouthAvenue',(0,-21200,240),(0,90,0),'Gameplay')
    hero=C.camera('City_Overview',(-48380,-55460,45804),(0,0,1200),32.)
    C.camera('City_Plan',(0,-1000,85000),(0,0,0),45.,1.)
    C.camera('Civic_Square',(-1000,-2550,440),(1200,1800,750),25.)
    C.camera('Craft_Street',(-14900,-12000,360),(-14900,-7600,600),28.)
    C.camera('Merchant_Street',(100,-12500,420),(100,-7200,650),28.)
    C.camera('Noble_Terraces',(14900,5500,650),(14900,10000,900),30.)
    C.camera('Cathedral_Close',(-6000,10000,2400),(0,18750,1700),38.)
    result=C.finish(hero,level,{'status':'passed','building_instances':len(actors),'building_types':28,'buildings':actors,'layout_seed':layout['seed'],'area_m':[550,550],
        'context_instances':counts,'district_counts':layout['district_counts'],'layout_validation':layout['validation']})
    (C.R/'city_scene.json').write_text(json.dumps(result,indent=2))

def run():
    records=json.loads((ROOT/'TDKProps/SourceAssets/Gothic/mesh_manifest.json').read_text())['variants']
    assert len(records)==28
    requested=json.loads((C.R/'scene_request.json').read_text()) if (C.R/'scene_request.json').exists() else {'build':'both'}
    if requested['build'] in ('both','catalog'):build_catalog(records)
    if requested['build'] in ('both','city'):build_city(records)
    (C.R/'scene_error.txt').unlink(missing_ok=True)
try:run()
except Exception:
    (C.R/'scene_error.txt').write_text(traceback.format_exc());raise
