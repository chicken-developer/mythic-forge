"""Build the 24-type catalog and a terrain-aware 133-building Medieval village."""
from pathlib import Path
import json
import math
import sys
import traceback
import unreal as u
ROOT=Path('E:/Repos/mythic-forge');P=ROOT/'TDKProps';sys.path.insert(0,str(P/'Tools/Medieval'))
import scene as S
C=S.C;A='/Game/Medieval'

def context():
    result={};mapping={'stone':'stone','trim':'stone','wood':'wood','metal':'metal','lampglass':'light','water':'water','soil':'rock','foliage':'leaf'}
    for name in ('Lamp','Bench','Fountain','Planter','Cypress'):
        path=A+'/Context/SM_MD_'+name
        mesh=u.load_asset(path) or u.EditorAssetLibrary.duplicate_asset('/Game/Gothic/City/Context/SM_City_'+name,path)
        assert mesh,name
        for i,slot in enumerate(mesh.static_materials):
            family=str(slot.material_slot_name).removeprefix('Gothic_');mat=u.load_asset(A+'/Materials/MI_MD_'+mapping[family]);assert mat
            mesh.set_material(i,mat)
        u.EditorAssetLibrary.save_loaded_asset(mesh);result[name]=mesh
    return result

def catalog(rows):
    level=A+'/Maps/L_Medieval_Catalog';C.begin(level)
    floor=C.flat('M_MD_CatalogGround',(.13,.145,.13));plinth=C.flat('M_MD_CatalogPlinth',(.19,.20,.17))
    C.cube('Catalog_Ground',(0,11000,-40),(24000,31000,80),floor,'Studio')
    entries=[]
    for i,row in enumerate(rows):
        x=((i%4)-1.5)*4500;y=(i//4)*4500
        cls=u.EditorAssetLibrary.load_blueprint_class(A+'/Blueprints/BP_Medieval_'+row['id'])
        a=C.spawn(cls,row['id'],(x,y,15),folder='Catalog/'+row['family']) if cls else C.mesh(row['id'],u.load_asset(A+'/Buildings/SM_Medieval_'+row['id']),(x,y,15),folder='Catalog/'+row['family'])
        a.tags=[C.TAG,'MedievalCatalog',row['id']]
        lo,hi=row['min_m'],row['max_m'];w,h,d=[v*100 for v in row['size_m']]
        # Cliff and water-channel meshes retain their original source datum.
        z=-lo[1]*100+15
        a.set_actor_location(u.Vector(x,y,z),False,False)
        C.cube('Plinth_'+row['id'],(x+(lo[0]+hi[0])*50,y+(lo[2]+hi[2])*50,0),(w+120,d+120,30),plinth,'Studio')
        text=C.spawn(u.TextRenderActor,'Label_'+row['id'],(x,y+lo[2]*100-120,40),(0,-90,0),'Labels')
        text.text_render.set_text(row['label'].replace(' — ',' / '));text.text_render.set_world_size(48.);text.text_render.set_horizontal_alignment(u.HorizTextAligment.EHTA_CENTER)
        if row['variant']=='base':
            r=max(w,h,d)*.95;target=(x+(lo[0]+hi[0])*50,y+(lo[2]+hi[2])*50,z+(lo[1]+hi[1])*50)
            C.camera('Family_'+row['id'][:2],(x+r*1.35,y-r*2.6,z+h*.55+r*.8),target,32.,1.6)
        if row['id']=='01_townhouse_base':
            C.camera('Interior',(x-110,y-110,z+210),(x+110,y+150,z+225),22.,1.6)
            light=C.spawn(u.PointLight,'Interior_ReviewFill',(x-80,y-20,z+220),folder='Lighting')
            light.light_component.set_editor_property('intensity',15.);light.light_component.set_editor_property('attenuation_radius',700.)
        entries.append({'id':row['id'],'actor':a.get_path_name()})
    C.lighting();hero=C.camera('Catalog_Overview',(-9000,-22000,35000),(0,10500,1500),32.,1.6)
    C.spawn(u.PlayerStart,'Catalog_PlayerStart',(0,-4200,240),(0,90,0),'Gameplay')
    report=C.finish(hero,level,{'status':'passed','types':24,'entries':entries});(S.R/'catalog_scene.json').write_text(json.dumps(report,indent=2))

def volume(cls,label,center,size):
    a=C.spawn(cls,label,center,folder='Gameplay/Water Boundary');_,e=a.get_actor_bounds(False)
    assert min(e.to_tuple())>0
    a.set_actor_scale3d(u.Vector(*(v/(2*x) for v,x in zip(size,e.to_tuple()))));return a

def village(rows,props):
    layout=json.loads((P/'SourceAssets/Medieval/village_layout.json').read_text());level=A+'/Maps/L_Medieval_Village';C.begin(level)
    grass=C.flat('M_MD_Grass',(.13,.19,.075),.95);earth=C.flat('M_MD_Earth',(.24,.19,.115),.95)
    road=C.flat('M_MD_DirtRoad',(.27,.235,.17),.94);paving=C.flat('M_MD_Paving',(.31,.30,.25),.9,paving=True)
    wood=u.load_asset(A+'/Materials/MI_MD_wood');water=u.load_asset(A+'/Materials/MI_MD_water');stone=u.load_asset(A+'/Materials/MI_MD_stone')
    C.cube('Landscape_Background',(0,0,-30),(600000,600000,60),grass,'Terrain')
    hill=layout['hill'];x0,y0,x1,y1=hill['rect'];top=hill['top_z']
    # Leave a real approach notch; a full solid plateau would bury the ramp.
    C.cube('Hill_Plateau_East',((132+x1)*50,(y0+y1)*50,top*50),((x1-132)*100,(y1-y0)*100,top*100),earth,'Terrain')
    C.cube('Hill_Plateau_North',((x0+132)*50,(115+y1)*50,top*50),((132-x0)*100,(y1-115)*100,top*100),earth,'Terrain')
    river=layout['river'];C.cube('River_Water',(river['x']*100,0,river['water_z']*100-3),(river['width']*100,37000,6),water,'River').static_mesh_component.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
    for x in (48.4,61.6):C.cube('River_Bank_'+str(x),(x*100,0,8),(80,37000,16),earth,'River')
    for r in layout['roads']:
        a,b=r['a'],r['b'];length=math.dist(a,b);x=(a[0]+b[0])*50;y=(a[1]+b[1])*50
        if r['name']=='Hill_Ramp':
            ramp=C.cube('Hill_Access_Ramp',(x,y,250),(math.hypot(length,5)*100,r['width']*100,40),road,'Streets')
            ramp.set_actor_rotation(u.Rotator(pitch=math.degrees(math.atan2(5,length)),yaw=90,roll=0),False)
        else:C.cube(r['name'],(x,y,r['z']*100+1),(r['width']*100 if a[0]==b[0] else length*100,length*100 if a[0]==b[0] else r['width']*100,2),road,'Streets')
    C.cube('Village_Common',(-4500,0,8),(4400,4400,16),paving,'Public Spaces')
    for y in (-80,0,80):
        C.cube('Bridge_'+str(y),(5500,y*100,65),(1800,680,30),wood,'River/Bridges')
        for sign in (-1,1):
            # Ramps rise along X towards the river crossing.
            ramp=C.cube('Bridge_Ramp_'+str((y,sign)),((5500+sign*1300),y*100,43),(math.hypot(800,80),680,24),wood,'River/Bridges')
            ramp.set_actor_rotation(u.Rotator(pitch=-sign*math.degrees(math.atan2(80,800)),yaw=0,roll=0),False)
            C.cube('Bridge_Rail_'+str((y,sign)),(5500,y*100+sign*310,170),(1800,12,12),wood,'River/Bridges')
            for x in (4800,5500,6200):C.cube('Bridge_Post_'+str((x,y,sign)),(x,y*100+sign*310,118),(16,16,120),wood,'River/Bridges')
    # Invisible water barriers leave three bridge corridors open.
    intervals=[(-185,-84),(-76,-4),(4,76),(84,185)]
    for i,(a,b) in enumerate(intervals):volume(u.BlockingVolume,'River_NoWade_'+str(i),(5500,(a+b)*50,125),(1100,(b-a)*100,250))
    entries=[]
    for i,b in enumerate(layout['buildings']):
        a=C.mesh(f'{i+1:03d} | '+b['label'],u.load_asset(A+'/Buildings/SM_Medieval_'+b['id']),tuple(v*100 for v in b['location_m']),b['yaw'],'Buildings/'+b['district'])
        a.tags=[C.TAG,'MedievalVillageBuilding',b['id'],b['district']];entries.append({'id':b['id'],'actor':a.get_path_name(),'district':b['district']})
        if b['district']=='wind_farms':
            x,y,_=b['location_m'];C.cube('Farm_Plot_'+str(i),(x*100,y*100,-1),(2600,2600,2),earth,'Farms')
            for j in range(8):C.cube('Crop_Row_'+str((i,j)),(x*100-1100+j*310,y*100,8),(65,2500,16),grass,'Farms')
    context_count=0
    def prop(name,loc,yaw=0):
        nonlocal context_count
        context_count+=1;a=C.mesh(name+'_'+str(context_count),props[name],loc,yaw,'Context/'+name);a.tags=[C.TAG,'MedievalContext',name];return a
    prop('Fountain',(-4500,0,16))
    for x,y,yaw in [(-5700,-1100,0),(-3300,-1100,0),(-5700,1100,180),(-3300,1100,180)]:prop('Bench',(x,y,16),yaw)
    for y in range(-150,151,18):
        for x in (-49,-41,106,114):
            if abs(y)<23 and x<0:continue
            prop('Lamp',(x*100,y*100,2))
    for y in range(-165,166,23):
        if min(abs(y-v) for v in (-80,0,80))<12:continue
        prop('Cypress',(6500,y*100,0))
    for x,y in [(-178,-130),(-175,-30),(-175,35),(-175,130),(178,-140),(178,-45),(177,30),(9000/100,168)]:prop('Cypress',(x*100,y*100,0))
    C.lighting();C.spawn(u.PlayerStart,'PlayerStart_SouthLane',(-4500,-14500,230),(0,90,0),'Gameplay')
    S.nav_bounds('Village_NavMeshBounds',(0,0,1200),(40000,40000,4000))
    recast=next((a for a in C.EA.get_all_level_actors() if isinstance(a,u.RecastNavMesh)),None)
    if recast is None:recast=C.spawn(u.RecastNavMesh,'Village_RecastNavMesh',folder='Gameplay/Navigation')
    recast.set_editor_property('runtime_generation',u.RuntimeGenerationType.DYNAMIC)
    recast.set_editor_property('agent_radius',42.);recast.set_editor_property('agent_height',192.)
    hero=C.camera('Village_Overview',(-35000,-40000,33000),(-2500,-3500,0),35.,1.6)
    C.camera('Village_Plan',(0,0,65000),(0,0,0),42.,1.)
    C.camera('Village_Common',(-4500,-2000,450),(-4500,2000,650),28.,1.6)
    C.camera('Riverside',(2500,-6500,400),(5500,-3000,650),30.,1.6)
    C.camera('Wind_Farm',(-12000,-18500,600),(-14000,-16000,500),30.,1.6)
    C.camera('Hill_Hamlet',(11000,8000,1800),(15500,14000,1200),30.,1.6)
    C.camera('Craft_Lane',(1500,-13500,300),(1500,-8000,550),28.,1.6)
    report=C.finish(hero,level,{'status':'passed','building_instances':len(entries),'types':24,'entries':entries,'context_instances':context_count,'layout_seed':layout['seed'],'area_m':layout['area_m']})
    world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    nav=u.NavigationSystemV1.get_navigation_system(world)
    if nav:
        for bounds in C.EA.get_all_level_actors():
            if isinstance(bounds,u.NavMeshBoundsVolume):nav.on_navigation_bounds_updated(bounds)
    u.SystemLibrary.execute_console_command(world,'RebuildNavigation')
    (S.R/'village_scene.json').write_text(json.dumps(report,indent=2))

def run():
    rows=json.loads((P/'SourceAssets/Medieval/mesh_manifest.json').read_text())['variants'];assert len(rows)==24
    catalog(rows);(S.R/'scene_error.txt').unlink(missing_ok=True)
try:run()
except Exception:(S.R/'scene_error.txt').write_text(traceback.format_exc());raise
