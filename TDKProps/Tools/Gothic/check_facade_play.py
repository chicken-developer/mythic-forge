"""Check the prototype in a real PIE world, including detail collision queries."""
from pathlib import Path
import json
import traceback
import unreal as u

OUT=Path('E:/Repos/mythic-forge/TDKProps/RenderTests/Gothic/FacadeReview')
report={'status':'failed','collision':[]}
try:
    worlds=u.EditorLevelLibrary.get_pie_worlds(False)
    assert worlds,'PIE did not start'
    world=worlds[0]
    facades={a.get_actor_label():a for a in u.GameplayStatics.get_all_actors_of_class(world,u.StaticMeshActor) if a.get_actor_label().startswith('Facade_')}
    assert set(facades)=={'Facade_Legacy','Facade_Game'},list(facades)
    game=facades['Facade_Game'];x=game.get_actor_location().x
    for label,dx,z,expected_y in [('wall',120.,350.,0.),('recessed_glazing',30.,300.,27.),('pinnacle_lantern',172.,710.,-29.)]:
        for complex_trace in (False,True):
            hit=u.SystemLibrary.line_trace_single(world,u.Vector(x+dx,-240,z),u.Vector(x+dx,100,z),u.TraceTypeQuery.ECC_VISIBILITY,complex_trace,[],u.DrawDebugTrace.NONE)
            assert hit is not None,(label,'no hit')
            data=hit.to_dict()
            assert data['blocking_hit'] and data['hit_actor']==game,(label,str(data))
            assert abs(data['location'].y-expected_y)<.2,(label,str(data['location']))
            report['collision'].append({'part':label,'complex_query':complex_trace,'location_cm':list(data['location'].to_tuple())})
    pawn=u.GameplayStatics.get_player_pawn(world,0)
    assert pawn is not None,'No playable pawn'
    report['world']=world.get_path_name();report['pawn_class']=pawn.get_class().get_path_name()
    report['status']='passed'
except Exception:
    report['error']=traceback.format_exc();raise
finally:
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/'play_validation.json').write_text(json.dumps(report,indent=2))
