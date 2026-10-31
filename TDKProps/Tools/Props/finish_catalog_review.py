"""Audit saved Unreal assets, actual Blueprint instances, and the two remaining maps."""
from pathlib import Path
import json
import sys
import traceback
import unreal as u

P=Path('E:/Repos/mythic-forge/TDKProps');R=P/'Saved/PropsDetail';OUT=P/'RenderTests/PropsDetail'
EA=u.get_editor_subsystem(u.EditorActorSubsystem);LE=u.get_editor_subsystem(u.LevelEditorSubsystem);SM=u.get_editor_subsystem(u.StaticMeshEditorSubsystem)

def main():
    report={'status':'running','meshes':[],'catalogs':[]}
    for family,qualities in [('Medieval',('normal','low','high')),('Gothic',('normal',))]:
        normal_rows=None
        for quality in qualities:
            name='mesh_manifest.json' if quality=='normal' else 'mesh_manifest_'+quality+'.json'
            rows=json.loads((P/'SourceAssets'/family/name).read_text())['variants']
            if quality=='normal':normal_rows=rows
            for row in rows:
                path='/Game/'+family+'/Buildings'+('' if quality=='normal' else '/'+quality.title())+'/SM_'+family+'_'+row['id']
                mesh=u.load_asset(path);assert mesh,path
                assert u.EditorAssetLibrary.get_metadata_tag(mesh,'GeneratorVersion')==row['generator_version'],path
                settings=SM.get_lod_build_settings(mesh,0);assert not settings.recompute_normals,path
                actual=mesh.get_num_triangles(0) if quality=='low' else mesh.get_num_nanite_triangles()
                collapsed=row['triangles']-actual;assert 0<=collapsed<=max(4,round(row['triangles']*.00001)),(path,actual,row['triangles'])
                assert SM.get_nanite_settings(mesh).enabled==(quality!='low')
                normals=[]
                for slot in mesh.static_materials:
                    mat=slot.material_interface;assert mat
                    tex=u.MaterialEditingLibrary.get_material_instance_texture_parameter_value(mat,'Normal');assert tex,(path,mat)
                    assert not tex.srgb and tex.compression_settings==u.TextureCompressionSettings.TC_NORMALMAP,tex
                    normals.append(tex.get_path_name())
                report['meshes'].append({'family':family,'id':row['id'],'quality':quality,'asset':path,'generator_version':row['generator_version'],'source_triangles':row['triangles'],'imported_triangles':mesh.get_static_mesh_description(0).get_triangle_count(),'engine_triangles':actual,'engine_collapsed_triangles':collapsed,'nanite_position_precision':SM.get_nanite_settings(mesh).position_precision,'imported_normals':True,'normal_maps':normals})
        level='/Game/'+family+'/Maps/L_'+family+'_Catalog';assert LE.load_level(level)
        actors=EA.get_all_level_actors();entries=[]
        for row in normal_rows:
            matched=[a for a in actors if family+'Catalog' in [str(t) for t in a.tags] and row['id'] in [str(t) for t in a.tags]]
            assert len(matched)==1,(family,row['id'],len(matched));actor=matched[0]
            meshes=actor.get_components_by_class(u.StaticMeshComponent);assert len(meshes)==1 and meshes[0].static_mesh
            assert (meshes[0].get_world_location()-actor.get_actor_location()).length()<.05,(family,row['id'],'detached mesh')
            assert actor.root_component.mobility==u.ComponentMobility.STATIC
            lights=actor.get_components_by_class(u.PointLightComponent);assert len(lights)==len(row['interior_lights'])
            assert all(l.intensity>0 and l.cast_shadows for l in lights)
            by_name={str(light.get_name()):light for light in lights}
            for i,source in enumerate(row['interior_lights']):
                x,y,z=source['position_m'];light=by_name['InteriorLight_'+str(i)]
                assert (light.relative_location-u.Vector(x*100,z*100,y*100)).length()<.05,(family,row['id'],i,'incorrect lamp position')
            cameras=[a for a in actors if a.get_actor_label()=='CAM_Interior_'+family+'_'+row['id']];assert len(cameras)==1
            entries.append({'id':row['id'],'lights':len(lights),'camera':cameras[0].get_actor_label()})
        report['catalogs'].append({'family':family,'map':level,'building_count':len(entries),'point_lights':sum(e['lights'] for e in entries),'entries':entries})
        del actors,matched,actor,meshes,lights,cameras
    maps=sorted('/Game/'+p.relative_to(P/'Content').with_suffix('').as_posix() for f in ('Medieval','Gothic') for p in (P/'Content'/f).rglob('*.umap'))
    assert maps==['/Game/Gothic/Maps/L_Gothic_Catalog','/Game/Medieval/Maps/L_Medieval_Catalog'],maps
    assert LE.load_level('/Game/Medieval/Maps/L_Medieval_Catalog')
    camera=next(a for a in EA.get_all_level_actors() if a.get_actor_label()=='CAM_Catalog_Overview')
    u.EditorLevelLibrary.set_level_viewport_camera_info(camera.get_actor_location(),camera.get_actor_rotation());EA.clear_actor_selection_set()
    LE.save_current_level();report.update(status='passed',remaining_maps=maps)
    (OUT/'unreal_validation.json').write_text(json.dumps(report,indent=2));(R/'final_error.txt').unlink(missing_ok=True)

try:main()
except Exception:(R/'final_error.txt').write_text(traceback.format_exc());raise
