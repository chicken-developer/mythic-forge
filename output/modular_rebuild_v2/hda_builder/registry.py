"""Single source of module identities, original members and public ports."""
from pathlib import Path
import re
import json

OWNER='mythic.medieval.modular_rebuild'
VERSION='2026.11.03-modular-v2'
TYPE='mythic::medieval_house::4.0'
ROOT=Path(__file__).resolve().parents[1]

# Preserve the 18 original typed functions; only caller-level composition changes.
SPECS=[
 ('layout.semantic_layout','LAYOUT','semantic_layout',['OPTIONAL_BLOCKOUT_INPUT','F_PLAN'],'F_PLAN','Resolve canonical role, floor, wall and roof guides'),
 ('shell.body_shell','SHELL','body_shell',['F_BODY_SHELL'],'F_BODY_SHELL','Construct thick body shell and solid host volumes'),
 ('shell.facade_layout','SHELL','facade_layout',['F_FACADE_LAYOUT'],'F_FACADE_LAYOUT','Resolve visible stable facade bays and hosted opening points'),
 ('openings.kits','OPENINGS','kits',['F_OPENING_KITS'],'F_OPENING_KITS','Construct shared window and door geometry and apertures'),
 ('openings.attachments','OPENINGS','attachments',['F_ATTACHMENTS'],'F_ATTACHMENTS','Resolve bay-window replacement hosts and attached opening cutters'),
 ('openings.windows','OPENINGS','windows',['F_WINDOWS'],'F_WINDOWS','Place window kits and export their matching cutters'),
 ('openings.doors','OPENINGS','doors',['F_DOORS'],'F_DOORS','Place entrances and export matching through-wall cutters'),
 ('openings.wall_cuts','OPENINGS','wall_cuts',['BAY_CONNECTOR_CUTTERS','ALL_OPENING_CUTTERS','F_WALL_FINISH'],'F_WALL_FINISH','Union aperture streams and cut the thick wall and gable shell'),
 ('roof.dormers','ROOF','dormers',['F_DORMERS'],'F_DORMERS','Build roof-hosted dormers and their aperture stream'),
 ('roof.surfaces_tiles','ROOF','surfaces_tiles',['F_ROOF'],'F_ROOF','Construct roof surfaces, thickness, tiles and body/dormer trimming'),
 ('roof.trim','ROOF','trim',['F_ROOF_TRIM'],'F_ROOF_TRIM','Construct eave boards, ridges and supported chimneys'),
 ('details.facade','DETAILS','facade',['F_FACADE_DETAILS'],'F_FACADE_DETAILS','Build facade timbers and reference-specific supported modules'),
 ('details.interior','DETAILS','interior',['F_INTERIOR'],'F_INTERIOR','Construct floors, stairs and interior structural frames'),
 ('details.balcony','DETAILS','balcony',['F_BALCONY'],'F_BALCONY','Construct supported upper access balconies'),
 ('details.utilities','DETAILS','utilities',['F_UTILITIES'],'F_UTILITIES','Construct the original wind and water utility grammar'),
 ('details.site','DETAILS','site',['F_SITE'],'F_SITE','Construct ground contact, entry steps and supported canopies'),
 ('details.furnishing','DETAILS','furnishing',['F_FURNISHING'],'F_FURNISHING','Construct the original interior and workshop furnishing'),
 ('uv_material.surfacing','UV_MATERIAL','surfacing',['MERGE_BUILDING','F_FINISH'],'F_FINISH','Assemble original part ordering and apply metric UV, normals and materials'),
]

def inventory():
    data=json.loads((ROOT/'results/source_inventory.json').read_text(encoding='utf-8'))
    return {r['relative_path']:r for r in data['nodes']}

def output_ports(rows, source):
    found=[]
    for path,row in rows.items():
        if path.startswith(source+'/') and path.count('/')==1 and row['type']=='output':
            index=int(row['parameters']['outputidx'])
            found.append((index,path.split('/')[-1].removeprefix('OUT_').lower()))
    return [port for _,port in sorted(found)]

def contracts():
    rows=inventory()
    membership={n:spec[0] for spec in SPECS for n in spec[3]}
    ports={spec[0]:output_ports(rows,spec[4]) for spec in SPECS}
    modules=[]
    for ident,stage,name,members,source,responsibility in SPECS:
        inputs={}; routes=[]
        for member in members:
            for c in rows[member]['inputs']:
                upstream=c['source']; producer=membership[upstream]
                if producer==ident:continue
                output=ports[producer][c['output']]
                port=f'{upstream.removeprefix("F_").lower()}_{output}'
                if port not in inputs:inputs[port]=producer+':'+output
                routes.append({'port':port,'member':member,'input':c['input']})
        params=sorted(set(re.findall(r'CTRL_BUILDING/([a-z0-9_]+)', '\n'.join(
            str(value) for path,row in rows.items() if path.split('/')[0] in members for value in row['parameters'].values()))))
        if ident=='shell.facade_layout':params+=['window_move_id','window_offset_u_m']
        if ident=='uv_material.surfacing':params+=['uv_tile_scale','uv_local_projection']
        modules.append({'module_id':ident,'stage':stage,'path':stage+'/'+name,'owner':OWNER,
                        'version':2,'responsibility':responsibility,'inputs':inputs,'outputs':ports[ident],
                        'params':params,'attributes':['name','material_family','module_id','part_role'],
                        'depends_on':list(dict.fromkeys(v.split(':')[0] for v in inputs.values())),
                        'debug_endpoint':stage+'/'+name+'/OUT_DEBUG',
                        'source_members':members,'source_output':source,'routes':routes,
                        'input_classes':{p:'semantic' if 'plan' in v or 'layout' in v else 'geometry' for p,v in inputs.items()}})
    modules.append({'module_id':'output.game_export','stage':'OUTPUT','path':'OUTPUT/game_export',
                    'owner':OWNER,'version':2,'responsibility':'Publish stable export metadata and final inspectable geometry',
                    'inputs':{'building':'uv_material.surfacing:building'},'outputs':['geometry'],
                    'params':[],'attributes':['name','material_family','module_id','part_role'],
                    'depends_on':['uv_material.surfacing'],'debug_endpoint':'OUTPUT/game_export/OUT_DEBUG'})
    return {'schema_version':1,'asset_name':'medieval_building_modular_v2','final_module':'output.game_export',
            'owner':OWNER,'version':VERSION,'modules':modules}

def validate(document):
    import importlib.util
    p=ROOT/'tooling/design-hda-building/scripts/validate_module_graph.py'
    spec=importlib.util.spec_from_file_location('module_validator',p)
    mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
    errors,order=mod.validate(document)
    if errors:raise ValueError(errors)
    return order
