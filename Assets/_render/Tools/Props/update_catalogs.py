"""Retain the two prop catalogs and replace each building with its lit Blueprint."""
from pathlib import Path
import datetime
import json
import math
import shutil
import sys
import traceback
import unreal as u

P=(Path(__file__).resolve().parents[4] / 'Assets/_render');R=P/'Saved/PropsDetail';OUT=P/'RenderTests/PropsDetail';OUT.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(P/'Tools/Gothic'));import city_scene as C
EA=u.get_editor_subsystem(u.EditorActorSubsystem);LE=u.get_editor_subsystem(u.LevelEditorSubsystem)
KEEP={'/Game/Medieval/Maps/L_Medieval_Catalog','/Game/Gothic/Maps/L_Gothic_Catalog'}

def backup_map(level):
    source=P/'Content'/level.removeprefix('/Game/');source=source.with_suffix('.umap')
    if source.exists():
        folder=R/'MapBackups'/datetime.datetime.now().strftime('%Y%m%d_%H%M%S');folder.mkdir(parents=True,exist_ok=True)
        shutil.copy2(source,folder/source.name)

def world_position(actor,point):
    return u.MathLibrary.transform_location(actor.get_actor_transform(),u.Vector(*point))

def camera_for(actor,family,row):
    origin=row.get('source_origin_m',[0,0,0]);p=row['parameters']
    if family=='Medieval':
        w=p['width_m'];d=w if p['footprint']=='round' else p['depth_m'];base=p['foundation_height_m']
        if p['footprint']=='round':eye=(-w*.26,base+1.65,-w*.18);target=(-w*.10,base+1.4,w*.28)
        else:eye=(-w*.28,base+1.65,-d*.23);target=(-w*.28,base+1.3,d*.32)
    elif p['building_type']=='church':
        w=p['nave_width'];eye=(-w*.20,1.75,1.5);target=(0,1.5,p['length_bays']*p['bay_length']*.65)
    elif p['building_type'] in ('residence','public'):
        w=p['nave_width'];d=p['urban_depth']*(1.2 if p['urban_plan']=='deep' else 1)
        front=d if p['urban_plan'] in ('compact','deep') else min(5.5,d*.44)
        eye=(-w*.28,1.75,max(.9,front*.20));target=(-w*.22,1.35,front*.80)
    elif p['building_type']=='townhouse':
        w=p['nave_width']*.65;d=p['bay_length']*1.4;eye=(-w*.25,1.7,d*.2);target=(-w*.2,1.4,d*.8)
    elif p['building_type']=='tower':
        w=p['nave_width']*.9;eye=(-w*.22,.35+1.65,-w*.22);target=(-w*.22,1.4,w*.25)
    else:
        w=p['nave_width']*.9;eye=(-w*.28,1.7,.9);target=(0,1.5,p['bay_length']*.8)
    def convert(point):
        point=[point[i]-origin[i] for i in range(3)]
        return world_position(actor,(point[0]*100,point[2]*100,point[1]*100))
    location=convert(eye);target=convert(target);label='CAM_Interior_'+family+'_'+row['id']
    existing=next((a for a in EA.get_all_level_actors() if a.get_actor_label()==label),None)
    if existing:EA.destroy_actor(existing)
    camera=C.camera('Interior_'+family+'_'+row['id'],location.to_tuple(),target.to_tuple(),24.,1.6)
    camera.tags=['PropsDetailGenerated','InteriorReview',row['id']]
    return label

def catalog(family):
    level='/Game/'+family+'/Maps/L_'+family+'_Catalog'
    assert LE.load_level(level)
    LE.save_current_level();backup_map(level)
    rows=json.loads((P/'SourceAssets'/family/'mesh_manifest.json').read_text())['variants'];entries=[]
    for row in rows:
        actors=[a for a in EA.get_all_level_actors() if row['id'] in [str(t) for t in a.tags] and (family+'Catalog') in [str(t) for t in a.tags]]
        assert len(actors)==1,(family,row['id'],len(actors))
        old=actors[0];transform=old.get_actor_transform();label=old.get_actor_label();folder=old.get_folder_path();tags=list(old.tags)
        cls=u.EditorAssetLibrary.load_blueprint_class('/Game/'+family+'/Blueprints/BP_'+family+'_'+row['id']);assert cls,row['id']
        # Hold only transform/value data across destruction, and preserve the catalog layout.
        EA.destroy_actor(old);del old,actors
        a=EA.spawn_actor_from_class(cls,transform.translation)
        a.set_actor_transform(transform,False,False);a.set_actor_label(label);a.set_folder_path(folder);a.tags=tags+['PropsDetailGenerated']
        lights=a.get_components_by_class(u.PointLightComponent)
        assert len(lights)==len(row['interior_lights']),(family,row['id'],len(lights))
        assert all(light.intensity>0 and light.cast_shadows for light in lights)
        cams=camera_for(a,family,row)
        entries.append({'id':row['id'],'actor':a.get_path_name(),'blueprint':cls.get_path_name(),'interior_lights':len(lights),'interior_camera':cams})
    # Generated review fill from the old catalog must not replace the prop's own lighting.
    for a in EA.get_all_level_actors():
        if a.get_actor_label()=='Interior_ReviewFill':EA.destroy_actor(a)
    assert LE.save_current_level()
    return {'family':family,'map':level,'entries':entries,'building_count':len(entries),'point_lights':sum(e['interior_lights'] for e in entries)}

def main():
    report={'status':'running','catalogs':[],'retired_maps':[]}
    # Preserve unsaved work in the current map before switching maps.
    u.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
    for family in ('Medieval','Gothic'):report['catalogs'].append(catalog(family))
    assert LE.load_level('/Game/Medieval/Maps/L_Medieval_Catalog')
    retired=R/'RetiredMaps';retired.mkdir(parents=True,exist_ok=True)
    for family in ('Medieval','Gothic'):
        for path in sorted((P/'Content'/family).rglob('*.umap')):
            package='/Game/'+path.relative_to(P/'Content').with_suffix('').as_posix()
            if package in KEEP:continue
            destination=retired/path.relative_to(P/'Content');destination.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,destination)
            if u.EditorAssetLibrary.does_asset_exist(package):
                assert u.EditorAssetLibrary.delete_asset(package),package
            # Some editor package deletions retain the .umap on disk. Archive that
            # exact residual file after the object has been removed from the registry.
            if path.exists():
                residual=destination.with_name(destination.stem+'_residual_'+datetime.datetime.now().strftime('%H%M%S%f')+'.umap')
                assert path.resolve().is_relative_to((P/'Content'/family).resolve())
                assert residual.resolve().is_relative_to(retired.resolve())
                shutil.move(str(path.resolve()),str(residual.resolve()))
            assert not path.exists(),str(path)
            report['retired_maps'].append(package)
    remaining={'/Game/'+p.relative_to(P/'Content').with_suffix('').as_posix() for family in ('Medieval','Gothic') for p in (P/'Content'/family).rglob('*.umap')}
    assert remaining==KEEP,remaining
    u.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(['/Game/Medieval','/Game/Gothic'],True)
    report['status']='passed';(OUT/'catalogs.json').write_text(json.dumps(report,indent=2));(R/'catalog_error.txt').unlink(missing_ok=True)

if __name__=='__main__':
    try:main()
    except Exception:(R/'catalog_error.txt').write_text(traceback.format_exc());raise
