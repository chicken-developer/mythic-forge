"""Read-only Unreal Python inspection of saved instancing, IDs, collision and material."""
import collections
import hashlib
import json
import pathlib
import unreal

root=pathlib.Path(unreal.Paths.project_dir()).resolve()
out=root/'Design/Environment/Generated'
config=json.loads((root/'Design/Environment/environment.json').read_text(encoding='utf-8'))
compiled=json.loads((out/'environment_compiled.json').read_text(encoding='utf-8'))
guard='-EnvironmentGuard' in unreal.SystemLibrary.get_command_line()
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert editor.load_level('/Game/Maps/Blockout/L_ThanhA_Blockout')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
groups=[];actual=collections.Counter();ids=[];errors=[]
for actor in actors:
    tags=[str(t) for t in actor.tags]
    if 'ThanhA.Environment.v1' not in tags:continue
    ids.extend(t for t in tags if t.startswith('ThanhA.Environment.Id.'))
    for component in actor.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
        mesh=component.get_editor_property('static_mesh').get_path_name()
        transforms=[]
        for i in range(component.get_instance_count()):
            t=component.get_instance_transform(i,world_space=True)
            p=t.translation;s=t.scale3d
            actual[(mesh,round(p.x/100,3),round(p.y/100,3))]+=1
            transforms.append([round(p.x,5),round(p.y,5),round(p.z,5),round(t.rotation.rotator().yaw,5),round(s.x,5),round(s.y,5),round(s.z,5)])
        groups.append(dict(id=actor.get_actor_label(),mesh=mesh,instances=len(transforms),transforms=sorted(transforms),collision=str(component.get_collision_enabled()),seed=component.instancing_random_seed,start_cull_cm=component.instance_start_cull_distance,end_cull_cm=component.instance_end_cull_distance))
expected=collections.Counter((x['mesh'],round(x['position_m'][0],3),round(x['position_m'][1],3)) for x in compiled['instances'])
if not guard and actual!=expected:errors.append('Saved instance positions/count differ from compiled data')
if len(ids)!=len(set(ids)):errors.append('Duplicate environment actor ID')
groups.sort(key=lambda x:x['id'])
signature=hashlib.sha256(json.dumps(groups,sort_keys=True).encode()).hexdigest()
map_path=root/'Content/Maps/Blockout/L_ThanhA_Blockout.umap'
map_signature=hashlib.sha256(map_path.read_bytes()).hexdigest()
asset_files={str(f.relative_to(root)).replace('\\','/'):hashlib.sha256(f.read_bytes()).hexdigest()
             for f in (root/'Content/Environment/ThanhA'/compiled['fingerprint']).glob('*.uasset')}
if guard and (out/'saved_state.json').exists():
    previous=json.loads((out/'saved_state.json').read_text(encoding='utf-8'))
    if signature!=previous['instance_signature_sha256']:errors.append('Saved vegetation has manual edits. Preserve them and reconcile environment.json before regeneration.')
    if previous.get('map_sha256',map_signature)!=map_signature:errors.append('Saved map has changed since the last authorized generation/capture. Back up and reconcile manual edits before regeneration.')
    for filename,digest in previous.get('owned_asset_sha256',{}).items():
        file=root/filename
        if not file.exists() or hashlib.sha256(file.read_bytes()).hexdigest()!=digest:errors.append('Owned environment asset changed: '+filename)
report=dict(status='fail' if errors else 'pass',errors=errors,execution='Read-only Unreal Python, saved level loaded in UE Editor world',fingerprint=compiled['fingerprint'],instanced_groups=len(groups),instances=sum(g['instances'] for g in groups),stable_id_count=len(ids),instance_signature_sha256=signature,map_sha256=map_signature,owned_asset_sha256=asset_files,groups=groups)
if not guard:
    (out/('inspection_errors.json' if errors else 'saved_state.json')).write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('ThanhA saved environment inspection: '+report['status']+'; '+str(report['instances'])+' instances')
if errors:raise RuntimeError('; '.join(errors))
