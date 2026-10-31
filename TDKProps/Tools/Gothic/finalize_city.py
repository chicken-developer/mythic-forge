"""Save the reviewed city view and update source metadata, without reimporting."""
from pathlib import Path
import json
import unreal as u
ROOT=Path('E:/Repos/mythic-forge');R=ROOT/'TDKProps/Saved/GothicReview/City';OUT=ROOT/'TDKProps/RenderTests/Gothic/City'
manifest=json.loads((ROOT/'TDKProps/SourceAssets/Gothic/mesh_manifest.json').read_text())
for row in manifest['variants']:
    mesh=u.load_asset('/Game/Gothic/Buildings/SM_Gothic_'+row['id']);assert mesh
    u.EditorAssetLibrary.set_metadata_tag(mesh,'SourceHIP',manifest['source_hip'])
    assert u.EditorAssetLibrary.save_loaded_asset(mesh)
LE=u.get_editor_subsystem(u.LevelEditorSubsystem)
assert LE.load_level('/Game/Gothic/Maps/L_Gothic_City')
actors=u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
camera=next(a for a in actors if a.get_actor_label()=='CAM_City_Overview')
u.get_editor_subsystem(u.EditorActorSubsystem).clear_actor_selection_set()
u.EditorLevelLibrary.set_level_viewport_camera_info(camera.get_actor_location(),camera.get_actor_rotation())
assert LE.save_current_level()
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
state=R/'capture_performance.json'
if state.exists():
    prior=json.loads(state.read_text())
    performance=u.get_default_object(u.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'))
    performance.set_editor_property('bThrottleCPUWhenNotForeground',prior['bThrottleCPUWhenNotForeground'])
    state.unlink()
u.SystemLibrary.execute_console_command(world,'Slate.bAllowThrottling 1')
report={'status':'passed','engine':u.SystemLibrary.get_engine_version(),'map':world.get_path_name(),
        'source_hip':manifest['source_hip'],'assets':len(manifest['variants']),
        'buildings':sum('GothicCityBuilding' in [str(t) for t in a.tags] for a in actors),
        'game_mode':world.get_world_settings().get_editor_property('default_game_mode').get_path_name(),
        'background_cpu_preference_restored':True}
assert report['buildings']==361
(OUT/'final_editor.json').write_text(json.dumps(report,indent=2))
