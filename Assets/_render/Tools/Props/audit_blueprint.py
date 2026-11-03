"""Read only: inspect saved component templates for the sample building."""
from pathlib import Path
import json
import unreal as u
bp=u.load_asset('/Game/Medieval/Blueprints/BP_Medieval_01_townhouse_base')
S=u.get_engine_subsystem(u.SubobjectDataSubsystem);L=u.SubobjectDataBlueprintFunctionLibrary;rows=[]
for h in S.k2_gather_subobject_data_for_blueprint(bp):
    data=L.get_data(h)
    for method in ('get_object','get_object_for_blueprint'):
        obj=L.get_object(data) if method=='get_object' else L.get_object_for_blueprint(data,bp)
        row={'method':method,'object':obj.get_path_name() if obj else None}
        if isinstance(obj,u.StaticMeshComponent):row.update(mesh=obj.static_mesh.get_path_name() if obj.static_mesh else None)
        if isinstance(obj,u.PointLightComponent):row.update(intensity=obj.intensity,location=list(obj.relative_location.to_tuple()))
        rows.append(row)
(Path(__file__).resolve().parents[4] / 'Assets/_render/Saved/PropsDetail/blueprint_audit.json').write_text(json.dumps(rows,indent=2))
