"""Make Unreal editing HDAs with catalog materials/UV scale; preserve source geometry."""
from pathlib import Path
import argparse
import hashlib
import json
import hou

P=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser();parser.add_argument('--family',choices=['Medieval','Gothic']);options=parser.parse_args()
report=[]
for family in ('Medieval','Gothic'):
    if options.family and options.family!=family:continue
    folder=P/'SourceAssets/Houdini'/family
    source=folder/(family.lower()+'_building.hda')
    target=folder/'Editor'/(family.lower()+'_building_editable.hda');target.parent.mkdir(parents=True,exist_ok=True)
    hou.hda.installFile(str(source));original=hou.hda.definitionsInFile(str(source))[0]
    name='quynh::'+family.lower()+'_catalog_edit::1.0'
    original.copyToHDAFile(str(target),new_name=name,new_menu_name=family+' Catalog / Editable')
    hou.hda.installFile(str(target));definition=hou.hda.definitionsInFile(str(target))[0];definition.setIsPreferred(True)
    obj=hou.node('/obj').createNode('geo','editor_'+family,run_init_scripts=False)
    asset=obj.createNode(name,'generator');asset.allowEditingOfContents()
    if family=='Gothic':
        group=asset.parmTemplateGroup();toggle=group.find('auto_rebuild');toggle.setDefaultValue(False);group.replace('auto_rebuild',toggle)
        # HAPI invokes parameter callbacks during uploads. Use explicit buttons
        # so a batch of shape edits builds the native SOP graph only once.
        for template in group.entriesWithoutFolders():
            template.setScriptCallback('');group.replace(template.name(),template)
        group.hide('rebuild_native',True)
        group.append(hou.ButtonParmTemplate('load_editor_preset','Load Chosen Preset',script_callback="hou.phm().apply_preset(kwargs)",script_callback_language=hou.scriptLanguage.Python))
        group.append(hou.ButtonParmTemplate('apply_editor_parameters','Apply Params / Rebuild',script_callback="hou.phm().rebuild(kwargs['node'])",script_callback_language=hou.scriptLanguage.Python))
        asset.setParmTemplateGroup(group);asset.parm('auto_rebuild').set(False)
    display=asset.displayNode();assert display
    output=asset.createNode('attribwrangle','UNREAL_CATALOG_OUTPUT');output.setInput(0,display);output.parm('class').set(1)
    if family=='Medieval':
        code='''string f=s@material_family;
s@unreal_material="/Game/Medieval/Materials/MI_MD_"+f+".MI_MD_"+f;
float scale=f=="plaster"?7.:f=="wood"?1.6:f=="tile"?.8:f=="stone"?1.1:f=="rock"?3.:1.;
'''
    else:
        rows=json.loads((P/'SourceAssets/Gothic/mesh_manifest.json').read_text())['variants']
        walls=[]
        for row in rows:
            number=int(row['id'][-2:]);district=row.get('district','landmark')
            old=['Limestone','RosePlaster','SagePlaster','PaleStone','Plaster','DarkStone','BluePlaster','BluePlaster','WarmStone','PaleStone']
            wall=old[number-1] if number<=10 else ['WarmStone','RosePlaster','Plaster','SagePlaster'][number%4] if district=='working' else ['Limestone','Plaster','SagePlaster'][number%3] if district=='middle' else 'PaleStone' if district=='upper' else 'Limestone'
            walls.append(wall)
        code='''string walls[]={WALLS};
string preset=chs("../building_preset");string pieces[]=split(preset,"_");int number=atoi(pieces[len(pieces)-1]);
string wall=walls[clamp(number-1,0,len(walls)-1)];
string f=s@material_id;string roof=chs("../roof_material");
string material=f=="stone"?wall:f=="trim"?"Ivory":f=="roof"?(roof=="copper"?"Copper":roof=="oxblood"?"Oxblood":"Slate"):f=="glass"?"AmberGlass":f=="metal"?"Iron":f=="gold"?"Brass":f=="interior"?"Plaster":"Walnut";
s@unreal_material="/Game/Gothic/Materials/City/MI_City_"+material+".MI_City_"+material;
float scale=(f=="roof"||f=="wood"||f=="glass")?2.:4.;
'''.replace('WALLS',','.join(json.dumps(w) for w in walls))
    code+='''
if(startswith(f,"skytown_")) {
    string name=replace(f,"skytown_","");
    s@unreal_material="/Game/SkyTown/Materials/"+name+"."+name;
    scale=1.;
}
s@shop_materialpath="";
s@unreal_generated_mesh_name="SM_Edit_"+chs("../building_preset");
foreach(int vertex;primvertices(0,@primnum)){
    vector uv=vertex(0,"uv",vertex);setvertexattrib(0,"uv",-1,vertex,uv/scale);
}
'''
    output.parm('snippet').set(code)
    # A terminal Output SOP takes precedence over display flags in HAPI.
    terminals=[n for n in asset.children() if n.type().name()=='output']
    for terminal in terminals:
        if terminal==display:output.setInput(0,terminal.input(0))
        terminal.setInput(0,output)
    if not terminals:output.setDisplayFlag(True);output.setRenderFlag(True)
    try:asset.cook(force=True)
    except hou.OperationFailed:
        raise RuntimeError([(n.path(),n.errors()) for n in (asset,*asset.allSubChildren()) if n.errors()])
    errors=[(n.path(),n.errors()) for n in (asset,*asset.allSubChildren()) if n.errors()];assert not errors,errors
    geo=asset.geometry();assert geo.findPrimAttrib('unreal_material') and len(geo.prims())>0
    definition.updateFromNode(asset)
    definition.setParmTemplateGroup(group if family=='Gothic' else asset.parmTemplateGroup())
    definition.addSection('EditableCatalog.json',json.dumps({'source':source.name,'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'purpose':'Unreal parameter editing and manual baking','geometry_changed':False}))
    definition.save(str(target));asset.matchCurrentDefinition()
    fresh=obj.createNode(name,'verify_saved_interface')
    if family=='Gothic':
        assert fresh.parm('apply_editor_parameters') and not fresh.evalParm('auto_rebuild')
        assert not fresh.parm('nave_width').parmTemplate().scriptCallback()
    report.append({'family':family,'type':name,'file':target.relative_to(P).as_posix(),'sha256':hashlib.sha256(target.read_bytes()).hexdigest(),'polygons':len(geo.prims()),'unreal_materials':sorted(set(geo.primStringAttribValues('unreal_material')))})
    obj.destroy()
out=P/'RenderTests/EditableHDA';out.mkdir(parents=True,exist_ok=True)
if options.family and (out/'editor_hdas.json').exists():
    report=[r for r in json.loads((out/'editor_hdas.json').read_text())['assets'] if r['family']!=options.family]+report
(out/'editor_hdas.json').write_text(json.dumps({'status':'passed','assets':report},indent=2))
print(json.dumps(report,indent=2))
