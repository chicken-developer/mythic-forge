"""One native Unreal close-up for a real construction joint, at a repeatable camera."""
from pathlib import Path
import json
import sys
import unreal as u
P=Path('E:/Repos/mythic-forge/TDKProps');R=P/'Saved/JointReview';OUT=P/'RenderTests/BuildingJoints';R.mkdir(parents=True,exist_ok=True);OUT.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(P/'Tools/Props'));import update_catalogs as C
request=json.loads((R/'capture_request.json').read_text());family=request['family'];ident=request['id'];view=request['view'];stage=request.get('stage','after')
row=next(r for r in json.loads((P/'SourceAssets'/family/'mesh_manifest.json').read_text())['variants'] if r['id']==ident);p=row['parameters']
assert C.LE.load_level('/Game/'+family+'/Maps/L_'+family+'_Catalog')
actor=next(a for a in C.EA.get_all_level_actors() if ident in [str(t) for t in a.tags] and family+'Catalog' in [str(t) for t in a.tags])
origin=row.get('source_origin_m',[0,0,0]);scale=p.get('scale',p.get('global_scale',1.))
def world(point):
    point=[point[i]*scale-origin[i] for i in range(3)]
    return C.world_position(actor,(point[0]*100,point[2]*100,point[1]*100)).to_tuple()
if family=='Medieval':
    w=p['width_m'];d=p['depth_m'];base=p['foundation_height_m'];top=base+p['floor_count']*p['floor_height_m']
    if view=='attic':target=(0,top+1.30,-d/2-.05);eye=(.12,top+1.35,-d/2-1.85);focal=50.
    elif view=='roof_join':
        target=(w/2,top-.1,p['wing_offset_z_m']);eye=(w/2+4,top+3,p['wing_offset_z_m']-6);focal=38.
    else:
        rows=p['front_bays'].split('/');floor=max(i for i,text in enumerate(rows[:p['floor_count']]) if 'W' in text);text=rows[floor];bay=text.index('W')
        width=w+(2*p['upper_overhang_m'] if floor else 0);depth=d+(2*p['upper_overhang_m'] if floor else 0)
        x=-width/2+(bay+.5)*width/len(text);high=min(p['floor_height_m']-.48,2.08)
        target=(x,base+floor*p['floor_height_m']+high-.10,-depth/2-.04);eye=(x+.12,target[1]+.12,-depth/2-2.0);focal=48.
else:
    w=p['nave_width'];fh=p['floor_height']
    if view=='corner':target=(w/2,fh+.08,0);eye=(w/2+3,fh+1.2,-3);focal=45.
    else:
        x=-(p['front_bays']-1)/2*w/p['front_bays'];target=(x,fh*.57,0);eye=(x+.15,fh*.61,-3.7);focal=36.
label='CAM_Joint_'+family+'_'+ident+'_'+view
old=next((a for a in C.EA.get_all_level_actors() if a.get_actor_label()==label),None)
if old:C.EA.destroy_actor(old)
camera=C.C.camera('Joint_'+family+'_'+ident+'_'+view,world(eye),world(target),focal,1.6);camera.tags=['PropsDetailGenerated','JointReview',ident]
u.EditorLevelLibrary.set_level_viewport_camera_info(camera.get_actor_location(),camera.get_actor_rotation());C.LE.save_current_level()
u.get_default_object(u.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings')).set_editor_property('bThrottleCPUWhenNotForeground',False)
u.SystemLibrary.execute_console_command(None,'Slate.bAllowThrottling 0')
file=OUT/(family.lower()+'_'+ident+'_'+view+'_'+stage+'.png')
u.AutomationLibrary.take_high_res_screenshot(960,600,str(file),camera=camera,delay=1.)
(R/'capture_status.json').write_text(json.dumps({'file':str(file),'camera':label,'stage':stage},indent=2))
