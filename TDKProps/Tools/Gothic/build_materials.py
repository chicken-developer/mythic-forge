"""Import Designer outputs and bind portable PBR material instances."""
from pathlib import Path
import json,traceback
import unreal as u
ROOT=Path('E:/Repos/mythic-forge')
REVIEW=ROOT/'TDKProps/Saved/GothicReview'
S=ROOT/'SubstanceWorkspace/src'
ASSET='/Game/Gothic'
AT=u.AssetToolsHelpers.get_asset_tools()
M=u.MaterialEditingLibrary

def asset(name,folder,cls,factory):
    return u.load_asset(folder+'/'+name) or AT.create_asset(name,folder,cls,factory)

def expression(mat,cls,x,y,**props):
    n=M.create_material_expression(mat,cls,x,y)
    for k,v in props.items():n.set_editor_property(k,v)
    return n

def connect(a,out,b,port):
    assert M.connect_material_expressions(a,out,b,port),(a,port)

def param(mat,name,x,y,texture,kind):
    return expression(mat,u.MaterialExpressionTextureSampleParameter2D,x,y,parameter_name=name,texture=texture,sampler_type=kind,sampler_source=u.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)

def main():
    assert (S/'build_complete.txt').exists(),'Designer texture build incomplete'
    report={'textures':[],'materials':[],'assignments':[]}
    textures={}
    specs=json.loads((S/'material_manifest.json').read_text())
    assert len(specs['graphs'])==10
    for spec in specs['graphs']:
        for channel in ['BaseColor','Normal','ORM']:
            name='T_Gothic_'+spec['name']+'_'+channel
            task=u.AssetImportTask()
            for k,v in {'filename':str(S/'exports'/(name+'.png')),'destination_path':ASSET+'/Textures','destination_name':name,'automated':True,'replace_existing':True,'save':False}.items():task.set_editor_property(k,v)
            AT.import_asset_tasks([task])
            tex=next(t for t in task.get_objects() if isinstance(t,u.Texture2D))
            tex.set_editor_property('srgb',channel=='BaseColor')
            tex.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_NORMALMAP if channel=='Normal' else (u.TextureCompressionSettings.TC_MASKS if channel=='ORM' else u.TextureCompressionSettings.TC_DEFAULT))
            tex.set_editor_property('lod_group',u.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP if channel=='Normal' else u.TextureGroup.TEXTUREGROUP_WORLD)
            tex.set_editor_property('max_texture_size',2048)
            u.EditorAssetLibrary.save_loaded_asset(tex)
            textures[(spec['name'],channel)]=tex
            report['textures'].append({'path':tex.get_path_name(),'srgb':tex.srgb,'compression':str(tex.compression_settings)})
    master=asset('M_Gothic_PBR',ASSET+'/Materials',u.Material,u.MaterialFactoryNew())
    M.delete_all_material_expressions(master)
    bc=param(master,'BaseColor',-700,-350,textures[('Stone_Limestone','BaseColor')],u.MaterialSamplerType.SAMPLERTYPE_COLOR)
    normal=param(master,'Normal',-700,0,textures[('Stone_Limestone','Normal')],u.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    orm=param(master,'ORM',-700,300,textures[('Stone_Limestone','ORM')],u.MaterialSamplerType.SAMPLERTYPE_MASKS)
    tint=expression(master,u.MaterialExpressionVectorParameter,-700,-600,parameter_name='Tint',default_value=u.LinearColor(1.,1.,1.,1.))
    mul=expression(master,u.MaterialExpressionMultiply,-350,-350)
    connect(bc,'RGB',mul,'A');connect(tint,'RGB',mul,'B')
    M.connect_material_property(mul,'',u.MaterialProperty.MP_BASE_COLOR)
    M.connect_material_property(normal,'RGB',u.MaterialProperty.MP_NORMAL)
    M.connect_material_property(orm,'R',u.MaterialProperty.MP_AMBIENT_OCCLUSION)
    M.connect_material_property(orm,'G',u.MaterialProperty.MP_ROUGHNESS)
    M.connect_material_property(orm,'B',u.MaterialProperty.MP_METALLIC)
    glow=expression(master,u.MaterialExpressionScalarParameter,-350,-650,parameter_name='Glow',default_value=0.)
    em=expression(master,u.MaterialExpressionMultiply,-100,-500)
    connect(mul,'',em,'A');connect(glow,'',em,'B')
    M.connect_material_property(em,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
    master.set_editor_property('two_sided',False)
    M.recompile_material(master);u.EditorAssetLibrary.save_loaded_asset(master)
    instances={}
    def instance(name,spec,tint=(1.,1.,1.),glow=0.):
        mi=asset('MI_Gothic_'+name,ASSET+'/Materials/Instances',u.MaterialInstanceConstant,u.MaterialInstanceConstantFactoryNew())
        M.set_material_instance_parent(mi,master)
        for channel in ['BaseColor','Normal','ORM']:M.set_material_instance_texture_parameter_value(mi,channel,textures[(spec,channel)])
        M.set_material_instance_vector_parameter_value(mi,'Tint',u.LinearColor(*tint,1.))
        M.set_material_instance_scalar_parameter_value(mi,'Glow',glow)
        M.update_material_instance(mi);u.EditorAssetLibrary.save_loaded_asset(mi)
        instances[name]=mi
        report['materials'].append({'path':mi.get_path_name(),'graph':spec,'tint':tint,'glow':glow})
        return mi
    for name,spec in [('Limestone','Stone_Limestone'),('Plaster','Wall_Plaster'),('Ivory','Trim_Ivory'),('Slate','Roof_Slate'),('Copper','Roof_Copper'),('Oxblood','Roof_Oxblood'),('Brass','Metal_Brass'),('Iron','Metal_Iron'),('Walnut','Wood_Walnut')]:instance(name,spec)
    instance('AmberGlass','Glass_Amber',glow=.18)
    instance('RosePlaster','Wall_Plaster',(1.,.78,.72))
    instance('SagePlaster','Wall_Plaster',(.72,.88,.77))
    instance('BluePlaster','Wall_Plaster',(.72,.85,.95))
    instance('DarkStone','Stone_Limestone',(.46,.55,.60))
    instance('WarmStone','Stone_Limestone',(1.,.94,.80))
    instance('PaleStone','Stone_Limestone',(1.08,1.09,1.06))
    walls=['Limestone','RosePlaster','SagePlaster','PaleStone','Plaster','DarkStone','BluePlaster','BluePlaster','WarmStone','PaleStone']
    roofs=['Slate','Oxblood','Copper','Slate','Copper','Oxblood','Slate','Oxblood','Copper','Slate']
    data=json.loads((ROOT/'TDKProps/SourceAssets/Gothic/mesh_manifest.json').read_text())
    assert len(data['variants'])==10
    for i,row in enumerate(data['variants']):
        mesh=u.load_asset(ASSET+'/Buildings/SM_Gothic_'+row['id'])
        assigned=[]
        for slot_index,slot in enumerate(mesh.get_editor_property('static_materials')):
            family=str(slot.material_slot_name).replace('Gothic_','')
            key={'stone':walls[i],'trim':'Ivory','roof':roofs[i],'glass':'AmberGlass','metal':'Iron','gold':'Brass','floor':'Limestone','wood':'Walnut','interior':'Plaster'}[family]
            mesh.set_material(slot_index,instances[key]);assigned.append({'slot':family,'material':key})
        u.EditorAssetLibrary.save_loaded_asset(mesh)
        report['assignments'].append({'id':row['id'],'slots':assigned})
    (REVIEW/'materials.json').write_text(json.dumps(report,indent=2))
    (REVIEW/'materials_complete.txt').write_text('30 textures, 1 master, 16 instances')
try:main()
except Exception:
    (REVIEW/'materials_error.txt').write_text(traceback.format_exc());raise
