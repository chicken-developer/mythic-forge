"""Cook native configurations, pack UVs, export FBX, and bake high-detail normals."""
from pathlib import Path
import argparse
import collections
import hashlib
import json
import math
import sys
import time
import traceback
import hou
import numpy as np
from PIL import Image,ImageDraw

ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
import build_hda as B
from render_review import plain_materials,setup,render
from render_variant import compose
PROJECT=ROOT.parent/'_render'
DEST=PROJECT/'SourceAssets/Medieval15'

def node(parent,typ,name,input_node=None,**parms):
    n=parent.createNode(typ,name)
    if input_node:n.setInput(0,input_node)
    for key,value in parms.items():
        p=n.parm(key)
        if p is None:raise ValueError((typ,key))
        p.set(value)
    return n

def force(n):
    n.cook(force=True)
    errors=[(c.path(),c.errors()) for c in (n,*n.allSubChildren()) if c.errors()]
    if errors:raise RuntimeError(errors)
    return n.geometry()

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def build_bake_geometry(building):
    obj=hou.node('/obj').createNode('geo','MEDIEVAL_BAKE',run_init_scripts=False)
    merge=node(obj,'object_merge','LIVE_NATIVE_BUILDING',objpath1=building.node('OUT_BUILDING').path())
    # Local per-polygon orthonormal mapping, then native packing of ALL islands.
    uv=node(obj,'attribwrangle','FACE_LOCAL_UV',merge,**{'class':3,'snippet':'''int pr=@primnum;int pts[]=primpoints(0,pr);
vector n=normalize(prim_normal(0,pr,.5,.5)),origin=point(0,"P",pts[0]);
vector u=0;float longest=0.;for(int i=0;i<len(pts);i++){
vector e=vector(point(0,"P",pts[(i+1)%len(pts)]))-vector(point(0,"P",pts[i]));
if(length2(e)>longest){u=normalize(e);longest=length2(e);}}
vector v=normalize(cross(n,u));vector q=@P-origin;v@uv=set(dot(q,u),dot(q,v),0.);'''})
    # Vertex wrangles use class=3. All original UV islands are explicitly split.
    area=sum(p.intrinsicValue('measuredarea') for p in uv.geometry().prims())
    # A conservative fixed texel scale avoids repeated maximum-fill searches
    # across thousands of separate tile / timber faces.
    layout=node(obj,'uvlayout::3.0','UNIQUE_BAKE_UV',uv,separatoredges='*',correctareas=1,
                scaling='custom',scale=math.sqrt(.20/max(area,.01)),rotstep='none',
                padding=1,resolution='res3',iterations=1,packincavities=0,generatenonpackedpoly=1)
    tri=node(obj,'divide','TRIANGULATED_LOW',layout,usemaxsides=1,numsides=3)
    clean=node(obj,'attribwrangle','REMOVE_DEGENERATES',tri,**{'class':1,'snippet':'if(primintrinsic(0,"measuredarea",@primnum)<1e-10)removeprim(0,@primnum,1);'})
    conn=node(obj,'connectivity','TRACE_PIECES',clean,connecttype='prim',attribname='bake_piece',attribtype='string')
    normals=node(obj,'normal','LOW_NORMALS',conn)
    normals=node(obj,'attribwrangle','REMOVE_NUMERICAL_NORMAL_SLIVERS',normals,**{'class':1,'snippet':'''int invalid=0;
foreach(int v;primvertices(0,@primnum))if(length(vector(vertex(0,"N",v)))<.5)invalid++;
if(invalid && primintrinsic(0,"measuredarea",@primnum)<1e-6)removeprim(0,@primnum,1);'''})
    # Restrict high-detail bevel to solid architectural members. Thin glazing,
    # mullions, straps, vegetation and canvas keep their original topology.
    group=node(obj,'attribwrangle','SEMANTIC_HIGH_EDGES',normals,**{'class':1,'snippet':'''int eligible=s@material_family=="wood"||s@material_family=="stone"||s@material_family=="tile"||s@material_family=="metal";
if(re_match(".*(mullion|transom|strap|hinge|baluster|leaf|plant|shutter).*",s@name))eligible=0;
i@group_high_detail_members=eligible;'''})
    high=node(obj,'polybevel::3.0','HIGH_DETAIL_BEVEL',group,group='high_detail_members',grouptype='prims',
              offset=.008,divisions=3,ignoreflatedges=1,detectcollisions=1,limit='individually')
    hightri=node(obj,'divide','TRIANGULATED_HIGH',high,usemaxsides=1,numsides=3)
    highclean=node(obj,'attribwrangle','CLEAN_HIGH_AND_RESTORE_PIECES',hightri,**{'class':1,'snippet':'''if(primintrinsic(0,"measuredarea",@primnum)<1e-10){removeprim(0,@primnum,1);return;}
if(s@bake_piece==""||s@material_family==""){int pr;vector uv;xyzdist(1,@P,pr,uv);
s@bake_piece=prim(1,"bake_piece",pr);s@material_family=prim(1,"material_family",pr);s@name=prim(1,"name",pr);s@shop_materialpath=prim(1,"shop_materialpath",pr);}'''})
    highclean.setInput(1,normals)
    highn=node(obj,'normal','HIGH_NORMALS',highclean)
    return obj,normals,highn

def export_fbx(sop,path):
    obj=hou.node('/obj').createNode('geo','FBX_EXPORT',run_init_scripts=False)
    imported=node(obj,'object_merge','mesh',objpath1=sop.path())
    scaled=node(obj,'xform','METRES_TO_CM',imported,scale=100.)
    scaled.setDisplayFlag(True);scaled.setRenderFlag(True)
    rop=node(hou.node('/out'),'filmboxfbx','EXPORT_FBX',startnode=obj.path(),trange=0,sopoutput=str(path))
    rop.render();rop.destroy();obj.destroy()

def bake_normal(low,high,ident,resolution):
    cop=hou.node('/img').createNode('copnet','MEDIEVAL_NORMAL_BAKE')
    # Exploded projection isolates connected modules without building thousands
    # of independent trace sets. Both meshes receive the same translation;
    # their normals, UVs and exported positions remain untouched.
    parent=low.parent()
    fix=node(parent,'attribwrangle','BAKE_RESTORE_TRACE_IDS',high,**{'class':1,'snippet':'''if(s@bake_piece==""){int pr;vector uv;xyzdist(1,@P,pr,uv);s@bake_piece=prim(1,"bake_piece",pr);}'''})
    fix.setInput(1,low)
    exploded=[]
    for name,sop in [('LOW',low),('HIGH',fix)]:
        e=node(parent,'attribwrangle','EXPLODED_'+name,sop,**{'class':2,'snippet':'''int prs[]=pointprims(0,@ptnum);if(!len(prs))return;
string piece=prim(0,"bake_piece",prs[0]);int id=atoi(re_replace("[^0-9]","",piece));
@P+=set(id%16,(id/16)%16,id/256)*32.;'''})
        exploded.append(e)
    imports=[]
    for name,sop in [('low',exploded[0]),('high',exploded[1])]:
        imports.append(node(cop,'sopimport',name,usesoppath=1,soppath=sop.path()))
    baker=node(cop,'bakegeometrytextures','HIGH_TO_LOW',tracingmode='surfacenormal',
               raylengthmissing='custom',raylength=.075,raybias=.001,enablematchpieces=0,pieceattrib='bake_piece',
               bakenormal=1,normaltype='offset',flipN='down',bakeocclusion=0,bakealpha=1,
               enableuvfilling=1,edgepadding=3./resolution,exterior='diffuse',visibilityculling='noculling')
    baker.setInput(1,imports[0]);baker.setInput(2,imports[1])
    outputs={}
    print('BAKE_START',ident,time.strftime('%H:%M:%S'),flush=True)
    for kind,index in [('normal',0),('hitmask',10)]:
        path=(DEST/'Normals' if kind=='normal' else ROOT/'results/bake_masks')/(ident+('_Normal_DX.png' if kind=='normal' else '_hits.png'))
        path.parent.mkdir(parents=True,exist_ok=True)
        out=node(cop,'rop_image','WRITE_'+kind,baker,copoutput=str(path),setres=1,res1=resolution,res2=resolution,
                 colorconversion='raw',savebackground=0)
        out.setInput(0,baker,index);out.render();outputs[kind]=path
    if baker.errors():raise RuntimeError(baker.errors())
    pixels=np.asarray(Image.open(outputs['normal']).convert('RGB'))
    stats={'resolution':list(pixels.shape[:2]),'nonflat_pixels':int(np.count_nonzero(np.max(np.abs(pixels[:,:,:2].astype(int)-128),axis=2)>8)),
           'normal_sha256':sha(outputs['normal']),'backend':'Houdini Copernicus bakegeometrytextures::2.0',
           'normal_space':'DirectX / MikkT / linear','high_trace_piece_attribute':'bake_piece',
           'projection_isolation':'native exploded connected components, identical low/high translations'}
    if stats['nonflat_pixels']<100:raise ValueError('Normal bake has no measured high-detail variation')
    cop.destroy()
    print('BAKE_END',ident,time.strftime('%H:%M:%S'),flush=True)
    return outputs,stats

def metrics(geo):
    points=np.asarray(geo.pointFloatAttribValues('P')).reshape(-1,3)
    uvs=np.asarray(geo.vertexFloatAttribValues('uv')).reshape(-1,3)
    normals=np.asarray(geo.vertexFloatAttribValues('N')).reshape(-1,3)
    if not (np.isfinite(points).all() and np.isfinite(uvs).all() and np.isfinite(normals).all()):raise ValueError('Non-finite attributes')
    invalid_normals=int(np.count_nonzero(np.abs(np.linalg.norm(normals,axis=1)-1)>1e-3))
    if invalid_normals:raise ValueError(('Non-unit normals',invalid_normals))
    areas=np.asarray([p.intrinsicValue('measuredarea') for p in geo.prims()])
    return {'points':len(points),'primitives':len(geo.prims()),'bounds_m':list(geo.boundingBox().sizevec()),
            'finite_attributes':True,'invalid_normals':invalid_normals,'degenerate_faces':int(np.count_nonzero(areas<1e-10)),
            'uv_min':uvs[:,:2].min(axis=0).tolist(),'uv_max':uvs[:,:2].max(axis=0).tolist(),
            'materials':dict(collections.Counter(geo.primStringAttribValues('material_family'))),
            'position_sha256':hashlib.sha256(points.tobytes()).hexdigest()}

def run(record,building,args,rop):
    ident=record['variant_id'];start=time.monotonic();B.apply_config(building,record)
    plain_materials(building,record['parameters']['palette']=='storybook_red')
    # Geometry output remains live; export helpers are temporary native SOP branches.
    obj,low,high=build_bake_geometry(building)
    try:
        lowgeo=force(low);lowstats=metrics(lowgeo);print('LOW',ident,lowstats['primitives'],time.strftime('%H:%M:%S'),flush=True)
        highgeo=force(high);highstats=metrics(highgeo);print('HIGH',ident,highstats['primitives'],time.strftime('%H:%M:%S'),flush=True)
        if lowstats['degenerate_faces'] or highstats['degenerate_faces']:raise ValueError('Degenerate geometry remains')
        if '' in lowstats['materials'] or '' in highstats['materials']:raise ValueError('Unassigned material family')
        for p in ('geometry','results','previews','comparisons'):(ROOT/p).mkdir(exist_ok=True)
        lowgeo.saveToFile(str(ROOT/'geometry'/(ident+'.bgeo.sc')))
        highgeo.saveToFile(str(DEST/'High'/(ident+'.bgeo.sc')))
        export_fbx(low,DEST/'Meshes'/('SM_MD15_'+ident+'.fbx'))
        export_fbx(high,DEST/'High'/('SM_MD15_'+ident+'_High.fbx'))
        outputs,bakestats=bake_normal(low,high,ident,args.resolution)
        obj.setDisplayFlag(False)
        hero=ROOT/record['outputs']['hero']
        if not args.no_render:render(rop,hero)
        # Review labels and side-by-side composition operate on actual renders.
        if not args.no_render:
            compose(record,args.manifest)
        opening=building.node('F_FACADE_LAYOUT/FACADE_OPENING_POINTS').geometry()
        clear=[]
        for pt in opening.points():
            if pt.attribValue('envelope_w')>pt.attribValue('bay_width')-.4+1e-4:clear.append(pt.number())
        if clear:raise ValueError(('Opening envelope clips bay',clear))
        result={'variant_id':ident,'effective_config':record,'generator_version':B.VERSION,'generator_sha256':sha(ROOT/'build_hda.py'),
                'hip_sha256':sha(ROOT/'medieval_building.hip'),'houdini':hou.applicationVersionString(),'seconds':time.monotonic()-start,
                'stages':{'code_generated':'pass','static_checked':'pass','houdini_cooked':'pass','geometry_exported':'pass','normal_baked':'pass','rendered':'not_run' if args.no_render else 'pass','unreal_imported':'not_run'},
                'low':lowstats,'high':highstats,'bake':bakestats,'opening_envelope_violations':clear,
                'image_provenance':'houdini_geometry_render','render_backend':'Houdini OpenGL ROP',
                'files':{'mesh':str(DEST/'Meshes'/('SM_MD15_'+ident+'.fbx')),'high_mesh':str(DEST/'High'/('SM_MD15_'+ident+'_High.fbx')),'normal':str(outputs['normal'])}}
        (ROOT/record['outputs']['result']).write_text(json.dumps(result,indent=2),encoding='utf-8')
        print('DONE',ident,round(result['seconds'],2),flush=True)
        return result
    finally:obj.destroy()

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--ids',nargs='*');ap.add_argument('--resolution',type=int,default=4096);ap.add_argument('--no-render',action='store_true');ap.add_argument('--skip-existing',action='store_true');ap.add_argument('--report-name',default='batch_latest');args=ap.parse_args()
    args.manifest=json.loads((ROOT/'manifest.json').read_text(encoding='utf-8'))
    for folder in ('Meshes','High','Normals'):(DEST/folder).mkdir(parents=True,exist_ok=True)
    (ROOT/'results').mkdir(exist_ok=True)
    hou.hipFile.load(str(ROOT/'medieval_building.hip'),suppress_save_prompt=True,ignore_load_warnings=True)
    building=hou.node('/obj/medieval_building');rop=None
    if not args.no_render:rop,cam=setup(building,ortho=25,res=900)
    records=[json.loads((ROOT/f).read_text(encoding='utf-8')) for f in [args.manifest['baseline']]+args.manifest['datasets']]
    summary=[];failed=[]
    for rec in records:
        if args.ids and rec['variant_id'] not in args.ids:continue
        result_file=ROOT/rec['outputs']['result']
        if args.skip_existing and result_file.exists():
            previous=json.loads(result_file.read_text(encoding='utf-8'))
            if previous.get('hip_sha256')==sha(ROOT/'medieval_building.hip') and previous.get('effective_config')==rec and previous.get('stages',{}).get('normal_baked')=='pass':
                print('SKIP_VERIFIED',rec['variant_id'],flush=True);summary.append(previous);continue
        try:summary.append(run(rec,building,args,rop))
        except Exception as exc:
            failed.append({'variant_id':rec['variant_id'],'error':str(exc),'traceback':traceback.format_exc()});print('FAILED',rec['variant_id'],traceback.format_exc(),flush=True)
    (ROOT/'results'/(args.report_name+'.json')).write_text(json.dumps({'results':[r['variant_id'] for r in summary],'failures':failed},indent=2),encoding='utf-8')
    if failed:raise SystemExit(1)

if __name__=='__main__':main()

