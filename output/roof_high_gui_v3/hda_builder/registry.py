"""Single source of module identities, original members and public ports."""
from pathlib import Path
import re
import json

OWNER='mythic.medieval.modular_rebuild'
VERSION='2026.11.03-roof-high-gui-v3'
TYPE='mythic::medieval_house::4.1'
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
    def new(ident,stage,name,inputs,outputs,params,responsibility):
        return {'module_id':ident,'stage':stage,'path':stage+'/'+name,'owner':OWNER,'version':3,
                'responsibility':responsibility,'inputs':inputs,'outputs':outputs,'params':params,
                'attributes':['name','material_family','roof_patch_id','module_id','part_role'],
                'depends_on':list(dict.fromkeys(v.split(':')[0] for v in inputs.values())),
                'debug_endpoint':stage+'/'+name+'/OUT_DEBUG'}
    modules.extend([
        new('roof.patch_surfaces','ROOF','patch_surfaces',{'plans':'layout.semantic_layout:roof_plans'},['raw_surfaces'],
            ['roof_shape','tile_size_m','mesh_quality'],'Skin each roof role independently; never bridge different buildings'),
        new('roof.junctions','ROOF','ridge_valley_solver',{'patches':'roof.patch_surfaces:raw_surfaces'},
            ['roof_top','roof_volume','roof_shell','junction_trim'],[],
            'Union closed downward roof volumes, retain the upper envelope and publish actual junction geometry'),
        new('output.detail_levels','OUTPUT','detail_levels',{'building':'uv_material.surfacing:building'},
            ['low','high','selected'],['preview_quality','bevel_width_m'],
            'Publish distinct LOW/HIGH geometry with local feature-limited native bevel')])
    by_id={m['module_id']:m for m in modules}
    def extra(ident,port,target,member,index):
        m=by_id[ident];m['inputs'][port]=target;m.setdefault('routes',[]).append({'port':port,'member':member,'input':index})
        m['depends_on']=list(dict.fromkeys(v.split(':')[0] for v in m['inputs'].values()))
    extra('roof.dormers','roof_interior','roof.junctions:roof_volume','F_DORMERS',3)
    extra('roof.dormers','roof_host','roof.junctions:roof_shell','F_DORMERS',4)
    extra('roof.surfaces_tiles','resolved_shell','roof.junctions:roof_shell','F_ROOF',3)
    extra('roof.surfaces_tiles','roof_interior','roof.junctions:roof_volume','F_ROOF',4)
    extra('roof.surfaces_tiles','raw_surfaces','roof.patch_surfaces:raw_surfaces','F_ROOF',5)
    extra('roof.trim','roof_interior','roof.junctions:roof_volume','F_ROOF_TRIM',1)
    extra('details.site','solid_body','shell.body_shell:solid_body','F_SITE',1)
    extra('details.site','overhead_access','details.balcony:balcony','F_SITE',2)
    extra('openings.attachments','solid_body','shell.body_shell:solid_body','F_ATTACHMENTS',4)
    extra('uv_material.surfacing','roof_junction_trim','roof.junctions:junction_trim','MERGE_BUILDING',15)
    modules.append({'module_id':'output.game_export','stage':'OUTPUT','path':'OUTPUT/game_export',
                    'owner':OWNER,'version':2,'responsibility':'Publish stable export metadata and final inspectable geometry',
                    'inputs':{'building':'output.detail_levels:selected'},'outputs':['geometry'],
                    'params':[],'attributes':['name','material_family','module_id','part_role'],
                    'depends_on':['output.detail_levels'],'debug_endpoint':'OUTPUT/game_export/OUT_DEBUG'})
    return {'schema_version':1,'asset_name':'medieval_building_roof_high_v3','final_module':'output.game_export',
            'owner':OWNER,'version':VERSION,'modules':modules}

def validate(document):
    import importlib.util
    p=ROOT/'tooling/design-hda-building/scripts/validate_module_graph.py'
    spec=importlib.util.spec_from_file_location('module_validator',p)
    mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
    errors,order=mod.validate(document)
    if errors:raise ValueError(errors)
    return order
