"""Import the six native context meshes used by the sample city."""
from pathlib import Path
import json
import traceback
import sys
import importlib
ROOT=Path(__file__).resolve().parents[4];sys.path.insert(0,str(ROOT/'Assets/_render/Tools/Gothic'))
import city_scene as C
C=importlib.reload(C)
import unreal as u

def run():
    manifest=json.loads((ROOT/'Assets/_render/SourceAssets/Gothic/Context/context_manifest.json').read_text())
    materials={'stone':u.load_asset('/Game/Gothic/Materials/City/MI_City_WarmStone'),'trim':u.load_asset('/Game/Gothic/Materials/City/MI_City_Ivory'),
        'wood':u.load_asset('/Game/Gothic/Materials/City/MI_City_Walnut'),'metal':u.load_asset('/Game/Gothic/Materials/City/MI_City_Iron'),
        'lampglass':C.flat('M_City_LampGlass',(.85,.47,.12),.4,glow=2.),'water':C.flat('M_City_Water',(.025,.11,.13),.16,.15),
        'soil':C.flat('M_City_Soil',(.055,.039,.026),.98),'foliage':C.flat('M_City_Cypress',(.035,.075,.039),.9)}
    report=[]
    for row in manifest['props']:
        task=u.AssetImportTask()
        for k,v in {'filename':str(ROOT/row['file']),'destination_path':'/Game/Gothic/City/Context','destination_name':'SM_City_'+row['id'],'automated':True,'replace_existing':True,'replace_existing_settings':True,'save':True}.items():task.set_editor_property(k,v)
        options=u.FbxImportUI();options.automated_import_should_detect_type=False;options.mesh_type_to_import=u.FBXImportType.FBXIT_STATIC_MESH
        options.import_materials=False;options.import_textures=False;options.import_as_skeletal=False
        data=options.static_mesh_import_data
        for k,v in {'combine_meshes':True,'convert_scene':True,'convert_scene_unit':True,'auto_generate_collision':False,'generate_lightmap_u_vs':False,'build_nanite':True,'normal_import_method':u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS}.items():data.set_editor_property(k,v)
        task.options=options;task.factory=u.FbxFactory();C.AT.import_asset_tasks([task])
        mesh=next(o for o in task.get_objects() if isinstance(o,u.StaticMesh))
        for i,slot in enumerate(mesh.static_materials):
            family=str(slot.material_slot_name).replace('Gothic_','');mat=materials[family];assert mat
            mesh.set_material(i,mat)
        mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        u.EditorAssetLibrary.save_loaded_asset(mesh)
        report.append({'id':row['id'],'asset':mesh.get_path_name(),'triangles':mesh.get_num_nanite_triangles()})
    (C.R/'context_import.json').write_text(json.dumps({'status':'passed','props':report},indent=2))
    (C.R/'context_error.txt').unlink(missing_ok=True)
try:run()
except Exception:
    (C.R/'context_error.txt').write_text(traceback.format_exc());raise
