from pathlib import Path
import unreal as u
P=(Path(__file__).resolve().parents[4] / 'Assets/_render/Saved/PropsDetail');P.mkdir(parents=True,exist_ok=True)
names=['SubobjectDataSubsystem','AddNewSubobjectParams','SubobjectDataBlueprintFunctionLibrary','BlueprintEditorLibrary','PointLightComponent']
lines=[]
for name in names:
    obj=getattr(u,name,None);lines.append(name+'\n'+str(getattr(obj,'__doc__','missing')))
    if name=='SubobjectDataSubsystem' and obj:
        for method in ('add_new_subobject','k2_gather_subobject_data_for_blueprint','rename_subobject','attach_subobject'):
            lines.append(method+'\n'+str(getattr(obj,method).__doc__))
for method in ('get_object','get_data'):
    lines.append(method+'\n'+str(getattr(u.SubobjectDataBlueprintFunctionLibrary,method).__doc__))
(P/'editor_capabilities.txt').write_text('\n'.join(lines),encoding='utf-8')
mesh=u.load_asset('/Game/Gothic/Buildings/SM_Gothic_variant_06')
sm=u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
settings=sm.get_nanite_settings(mesh)
lines=[str(settings),str(type(settings).__doc__),str(sm.get_lod_build_settings(mesh,0))]
for name in dir(mesh):
    if 'description' in name or 'triangle' in name:lines.append(name+'\n'+str(getattr(mesh,name).__doc__))
lines+=['lod0_triangles='+str(mesh.get_num_triangles(0)),'nanite_triangles='+str(mesh.get_num_nanite_triangles())]
description=mesh.get_static_mesh_description(0)
lines+=['source_triangles='+str(description.get_triangle_count())]
for name in dir(description):
    if 'triangle' in name or 'vertex_position' in name:lines.append(name+'\n'+str(getattr(description,name).__doc__))
(P/'nanite_diagnostic.txt').write_text('\n'.join(lines),encoding='utf-8')
