"""Record runtime position around a brief actual keyboard walking test."""
from pathlib import Path
import json
import unreal as u
R=Path('E:/Repos/mythic-forge/TDKProps/Saved/GothicReview/City')
world=u.EditorLevelLibrary.get_pie_worlds(False)[0];pawn=u.GameplayStatics.get_player_pawn(world,0)
position=list(pawn.get_actor_location().to_tuple())
request=json.loads((R/'movement_request.json').read_text())
if request['phase']=='before':
    pc=u.GameplayStatics.get_player_controller(world,0);pc.set_control_rotation(u.Rotator(pitch=-8,yaw=90,roll=0))
    (R/'movement_before.json').write_text(json.dumps({'position_cm':position}))
else:
    before=json.loads((R/'movement_before.json').read_text())['position_cm']
    distance=sum((a-b)**2 for a,b in zip(position[:2],before[:2]))**.5
    report={'status':'passed' if distance>50 else 'failed','before_cm':before,'after_cm':position,'distance_cm':distance,'pawn_class':pawn.get_class().get_path_name()}
    (R/'movement_validation.json').write_text(json.dumps(report,indent=2))
    assert distance>50,'Character did not walk'
