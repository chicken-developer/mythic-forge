"""Check actual PIE actors and both simple and complex collision queries."""
from pathlib import Path
import json,traceback
import unreal as u
R=(Path(__file__).resolve().parents[4] / 'Assets/_render/Saved/GothicReview')
report={'status':'failed','buildings':[]}
try:
    worlds=u.EditorLevelLibrary.get_pie_worlds(False)
    assert worlds,'PIE did not start'
    world=worlds[0]
    actors=u.GameplayStatics.get_all_actors_of_class(world,u.StaticMeshActor)
    buildings=[a for a in actors if 'GothicPreset' in [str(t) for t in a.tags]]
    assert len(buildings)==10,len(buildings)
    for a in buildings:
        variant=next(str(t) for t in a.tags if str(t).startswith('variant_'))
        center,extent=a.get_actor_bounds(False)
        results=[]
        for complex_trace in (False,True):
            start=u.Vector(center.x+extent.x*.08,center.y+extent.y*.07,center.z+extent.z+200.)
            end=u.Vector(start.x,start.y,5.)
            hit=u.SystemLibrary.line_trace_single(world,start,end,u.TraceTypeQuery.ECC_VISIBILITY,complex_trace,[],u.DrawDebugTrace.NONE)
            assert hit is not None,variant+' no collision'
            data=hit.to_dict()
            assert data['blocking_hit']
            owner=data['hit_actor']
            assert variant in [str(t) for t in owner.tags],(variant,owner.get_actor_label())
            results.append({'query':'complex' if complex_trace else 'simple','hit':owner.get_actor_label(),'height_cm':data['location'].z})
        report['buildings'].append({'id':variant,'collision':results})
    pawn=u.GameplayStatics.get_player_pawn(world,0)
    assert pawn is not None,'No playable pawn'
    report['pawn_class']=pawn.get_class().get_path_name()
    report['pawn_position_cm']=list(pawn.get_actor_location().to_tuple())
    report['world']=world.get_path_name()
    report['status']='passed'
    # Capture the runtime world through the same overview camera.
    cam=next(a for a in u.GameplayStatics.get_all_actors_of_class(world,u.CineCameraActor) if a.get_actor_label()=='CAM_Overview')
    pc=u.GameplayStatics.get_player_controller(world,0)
    pc.set_view_target_with_blend(cam,0.)
    u.SystemLibrary.execute_console_command(world,'HighResShot 3840x1536 filename="E:/Repos/mythic-forge/Assets/_render/Saved/GothicReview/gothic_pie.png"')
except Exception:
    report['error']=traceback.format_exc()
    raise
finally:(R/'play_validation.json').write_text(json.dumps(report,indent=2))
