"""Scene helpers scoped to Medieval; shared primitives stay in the project tools."""
from pathlib import Path
import importlib
import json
import sys
import unreal as u
ROOT=Path('E:/Repos/mythic-forge');P=ROOT/'TDKProps';R=P/'Saved/MedievalReview';OUT=P/'RenderTests/Medieval';OUT.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(P/'Tools/Gothic'))
import city_scene as C
C=importlib.reload(C);C.A='/Game/Medieval';C.TAG='MedievalGenerated';C.R=R
def render_ready():
    performance=u.get_default_object(u.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'))
    state=R/'capture_performance.json'
    if not state.exists():state.write_text(json.dumps({'bThrottleCPUWhenNotForeground':performance.get_editor_property('bThrottleCPUWhenNotForeground')}))
    performance.set_editor_property('bThrottleCPUWhenNotForeground',False)
    u.SystemLibrary.execute_console_command(None,'Slate.bAllowThrottling 0')
def nav_bounds(label,center,size):
    a=C.spawn(u.NavMeshBoundsVolume,label,center,folder='Gameplay/Navigation')
    _,extent=a.get_actor_bounds(False)
    assert min(extent.to_tuple())>0,('NavMesh brush has no geometry',list(extent.to_tuple()))
    a.set_actor_scale3d(u.Vector(*(s/(e*2) for s,e in zip(size,extent.to_tuple()))))
    return a
