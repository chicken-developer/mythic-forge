"""Finite live PIE collision, CharacterMovement and screenshot acceptance test."""
from pathlib import Path
import json
import runpy
import time
import traceback
import unreal as u

ROOT=Path('E:/Repos/mythic-forge');TOOLS=ROOT/'TDKProps/Tools/Gothic'
R=ROOT/'TDKProps/Saved/GothicReview/City';OUT=ROOT/'TDKProps/RenderTests/Gothic/City'
LE=u.get_editor_subsystem(u.LevelEditorSubsystem)
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
if not world.get_path_name().startswith('/Game/Gothic/Maps/L_Gothic_City.'):
    assert LE.load_level('/Game/Gothic/Maps/L_Gothic_City')
(R/'validation_request.json').write_text(json.dumps({'mode':'play'}))
state={'phase':'starting','started':time.monotonic()}
target=OUT/'city_play.png';old_mtime=target.stat().st_mtime_ns if target.exists() else 0
report={'status':'running','method':'live PIE Character.add_movement_input'}

def status(phase):
    state['phase']=phase;state['started']=time.monotonic()
    report['phase']=phase;(R/'play_progress.json').write_text(json.dumps(report,indent=2))

def stop(error=None):
    if error:report.update(status='failed',error=error)
    else:report.update(status='passed')
    (OUT/'play_session.json').write_text(json.dumps(report,indent=2))
    (R/'play_progress.json').write_text(json.dumps(report,indent=2))
    u.unregister_slate_post_tick_callback(state['handle'])
    LE.editor_request_end_play()

def tick(delta):
    try:
        phase=state['phase'];elapsed=time.monotonic()-state['started']
        assert elapsed<90,'PIE test timed out during '+phase
        worlds=u.EditorLevelLibrary.get_pie_worlds(False)
        if phase=='starting':
            if not worlds:return
            pawn=u.GameplayStatics.get_player_pawn(worlds[0],0)
            if not isinstance(pawn,u.Character):return
            if elapsed<2:return
            runpy.run_path(str(TOOLS/'validate_city.py'))
            state['world']=worlds[0];state['pawn']=pawn
            pc=u.GameplayStatics.get_player_controller(worlds[0],0)
            pc.set_control_rotation(u.Rotator(pitch=-8,yaw=90,roll=0))
            state['before']=list(pawn.get_actor_location().to_tuple())
            state['walk_time']=u.GameplayStatics.get_time_seconds(worlds[0])
            status('walking')
        elif phase=='walking':
            pawn=state['pawn'];world=state['world']
            if u.GameplayStatics.get_time_seconds(world)-state['walk_time']<2:
                pawn.add_movement_input(u.Vector(0,1,0),1.,False);return
            after=list(pawn.get_actor_location().to_tuple());before=state['before']
            distance=sum((a-b)**2 for a,b in zip(after[:2],before[:2]))**.5
            movement={'status':'passed' if distance>50 else 'failed','method':report['method'],
                      'before_cm':before,'after_cm':after,'distance_cm':distance,
                      'pawn_class':pawn.get_class().get_path_name()}
            (OUT/'movement_validation.json').write_text(json.dumps(movement,indent=2))
            assert distance>50,'CharacterMovement did not move the pawn'
            report['movement_cm']=distance
            u.SystemLibrary.execute_console_command(world,'HighResShot 2560x1440 filename="'+target.as_posix()+'"')
            status('capture')
        elif phase=='capture':
            if elapsed>3 and target.exists() and target.stat().st_mtime_ns>old_mtime and target.stat().st_size>1024:
                report['screenshot']=target.name;stop()
    except Exception:stop(traceback.format_exc())

status('starting')
state['handle']=u.register_slate_post_tick_callback(tick)
LE.editor_request_begin_play()
