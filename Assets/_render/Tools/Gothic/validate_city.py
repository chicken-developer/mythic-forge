"""Validate saved/reloaded city assets and the runtime Third Person sample."""
from pathlib import Path
import json
import traceback
import unreal as u
ROOT=Path(__file__).resolve().parents[4];R=ROOT/'Assets/_render/Saved/GothicReview/City';OUT=ROOT/'Assets/_render/RenderTests/Gothic/City';OUT.mkdir(parents=True,exist_ok=True)
manifest=json.loads((ROOT/'Assets/_render/SourceAssets/Gothic/mesh_manifest.json').read_text())
request=json.loads((R/'validation_request.json').read_text()) if (R/'validation_request.json').exists() else {'mode':'editor'}
mode=request['mode'];report={'status':'failed','mode':mode,'meshes':[]}
try:
    sm=u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
    # StaticMeshEditorSubsystem collision queries are unavailable during PIE.
    # Runtime acceptance uses physical traces below; editor asset checks have
    # already been recorded after reopening the saved packages.
    if mode=='play':
        editor=json.loads((OUT/'validation_editor.json').read_text())
        assert editor['status']=='passed' and len(editor['meshes'])==28
        report['asset_validation']='validation_editor.json'
    for row in manifest['variants'] if mode=='editor' else []:
        mesh=u.load_asset('/Game/Gothic/Buildings/SM_Gothic_'+row['id']);assert mesh
        box=mesh.get_bounding_box();size=list((box.max-box.min).to_tuple())
        expected=[row['size_m'][0]*100,row['size_m'][2]*100,row['size_m'][1]*100]
        assert max(abs(a-b) for a,b in zip(size,expected))<.2
        assert mesh.get_num_nanite_triangles()==row['triangles']
        assert sm.get_nanite_settings(mesh).enabled
        assert sm.get_collision_complexity(mesh)==u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE
        assert all(s.material_interface is not None for s in mesh.static_materials)
        assert sm.get_num_uv_channels(mesh,0)>=1
        report['meshes'].append({'id':row['id'],'triangles':row['triangles'],'size_cm':size,'nanite':True,'materials':len(mesh.static_materials)})
    if mode=='editor':
        level=u.get_editor_subsystem(u.LevelEditorSubsystem)
        assert level.load_level('/Game/Gothic/Maps/L_Gothic_Catalog')
        ea=u.get_editor_subsystem(u.EditorActorSubsystem)
        catalog=[a for a in ea.get_all_level_actors() if 'GothicCatalog' in [str(t) for t in a.tags]]
        assert len(catalog)==28
        assert level.load_level('/Game/Gothic/Maps/L_Gothic_City')
        world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
        actors=ea.get_all_level_actors()
    else:
        worlds=u.EditorLevelLibrary.get_pie_worlds(False);assert worlds,'PIE did not start'
        world=worlds[0];actors=u.GameplayStatics.get_all_actors_of_class(world,u.Actor)
        pawn=u.GameplayStatics.get_player_pawn(world,0);assert pawn
        assert isinstance(pawn,u.Character),'Expected the walking Third Person character'
        report['pawn_class']=pawn.get_class().get_path_name();report['pawn_location_cm']=list(pawn.get_actor_location().to_tuple())
        capsule=pawn.get_component_by_class(u.CapsuleComponent);assert capsule and capsule.get_collision_enabled()!=u.CollisionEnabled.NO_COLLISION
    buildings=[a for a in actors if 'GothicCityBuilding' in [str(t) for t in a.tags]]
    layout=json.loads((ROOT/'Assets/_render/SourceAssets/Gothic/city_layout.json').read_text())
    assert len(buildings)==layout['building_instances'],(len(buildings),layout['building_instances'])
    present=set(next(str(t) for t in a.tags if str(t).startswith('variant_')) for a in buildings)
    assert present==set(r['id'] for r in manifest['variants'])
    report['building_instances']=len(buildings);report['building_types']=len(present)
    report['context_actors']=sum('GothicCityContext' in [str(t) for t in a.tags] for a in actors)
    rectangles=[]
    for a in buildings:
        c,e=a.get_actor_bounds(False);rectangles.append((a,[c.x-e.x,c.y-e.y,c.x+e.x,c.y+e.y]))
    def overlap(a,b):return a[0]<b[2]-.05 and a[2]>b[0]+.05 and a[1]<b[3]-.05 and a[3]>b[1]+.05
    for i,(a,rect) in enumerate(rectangles):
        assert not any(overlap(rect,other) for _,other in rectangles[i+1:]),a.get_actor_label()+' overlaps another building'
        if 'gate' not in [str(t) for t in a.tags]:
            assert not any(overlap(rect,[v*100 for v in road['rect']]) for road in layout['roads']),a.get_actor_label()+' obstructs a road'
    report['world_building_overlaps']=0;report['world_buildings_on_roads']=0
    if mode=='play':
        checks=[]
        # Confirm collision on one actual building instance of every type.
        for ident in sorted(present):
            a=next(a for a in buildings if ident in [str(t) for t in a.tags])
            center,extent=a.get_actor_bounds(False)
            d=None
            # Courtyard plans deliberately leave their AABB centre open.
            for fx,fy in [(.10,.07),(-.70,-.65),(.70,-.65),(-.70,.65),(.70,.65)]:
                start=u.Vector(center.x+extent.x*fx,center.y+extent.y*fy,center.z+extent.z+150.)
                hit=u.SystemLibrary.line_trace_single(world,start,u.Vector(start.x,start.y,5.),u.TraceTypeQuery.ECC_VISIBILITY,False,[],u.DrawDebugTrace.NONE)
                if hit is not None:
                    result=hit.to_dict()
                    if result['blocking_hit'] and result['hit_actor']==a:d=result;break
            assert d is not None,(ident,'No collision on roof or perimeter masses')
            checks.append({'id':ident,'hit_height_cm':d['location'].z})
        report['building_collision']=checks
        ground=[]
        for x,y in [(0,-21200),(0,-10000),(0,10000),(-15000,-10000),(15000,10000),(-4000,0),(0,2500)]:
            hit=u.SystemLibrary.line_trace_single(world,u.Vector(x,y,120.),u.Vector(x,y,-100.),u.TraceTypeQuery.ECC_VISIBILITY,False,[],u.DrawDebugTrace.NONE)
            assert hit is not None
            d=hit.to_dict();assert d['blocking_hit']
            assert d['location'].z<50.,(x,y,'street obstructed',str(d))
            ground.append({'xy_cm':[x,y],'surface_height_cm':d['location'].z})
        report['street_surfaces']=ground
    report['status']='passed'
except Exception:
    report['error']=traceback.format_exc();raise
finally:
    (OUT/('validation_'+mode+'.json')).write_text(json.dumps(report,indent=2),encoding='utf-8')
