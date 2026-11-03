"""Run in Substance 3D Designer's Python Editor; creates editable native graphs."""
from pathlib import Path
import json
import traceback
import sd
from sd.api.sbs.sdsbscompgraph import SDSBSCompGraph
from sd.api.sdproperty import SDPropertyCategory
from sd.api.sbs.sdsbscompnode import SDPropertyInheritanceMethod
from sd.api.sdvalueint import SDValueInt
from sd.api.sdvalueint2 import SDValueInt2
from sd.api.sdvaluefloat import SDValueFloat
from sd.api.sdvaluefloat2 import SDValueFloat2
from sd.api.sdvaluefloat4 import SDValueFloat4
from sd.api.sdvaluebool import SDValueBool
from sd.api.sdvaluestring import SDValueString
from sd.api.sdvaluecolorrgba import SDValueColorRGBA
from sd.api.sdvalueenum import SDValueEnum
from sd.api.sdvaluearray import SDValueArray
from sd.api.sdvaluestruct import SDValueStruct
from sd.api.sdtypestruct import SDTypeStruct
from sd.api.sdvalueusage import SDValueUsage, SDUsage
from sd.api.sdtypeusage import SDTypeUsage
from sd.api.sdbasetypes import float2, float4, int2, ColorRGBA
from sd.api.sdgraphobjectframe import SDGraphObjectFrame

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT/'Assets/texture_gothic'
EXPORT = SRC/'exports'
LIB = Path('C:/Program Files/Adobe/Adobe Substance 3D Designer/resources/packages')
app = sd.getContext().getSDApplication()
pm = app.getPackageMgr()
libs = {}

def value(node, key, val):
    prop = node.getPropertyFromId(key, SDPropertyCategory.Input)
    if not prop:
        raise ValueError('Missing property '+key)
    t = prop.getType().getId()
    if t == 'int': v=SDValueInt.sNew(int(val))
    elif t == 'float': v=SDValueFloat.sNew(float(val))
    elif t == 'bool': v=SDValueBool.sNew(bool(val))
    elif t == 'int2': v=SDValueInt2.sNew(int2(*val))
    elif t == 'float2': v=SDValueFloat2.sNew(float2(*val))
    elif t == 'float4': v=SDValueFloat4.sNew(float4(*val))
    elif t == 'ColorRGBA': v=SDValueColorRGBA.sNew(ColorRGBA(*val))
    elif t == 'string': v=SDValueString.sNew(str(val))
    else: v=SDValueEnum.sFromValue(t,int(val))
    node.setInputPropertyValueFromId(key,v)

def node(g, kind, x, y, **params):
    n=g.newNode('sbs::compositing::'+kind)
    n.setPosition(float2(x,y))
    for k,v in params.items(): value(n,k,v)
    return n

def library(g, name, x, y, **params):
    if name not in libs: libs[name]=pm.loadUserPackage(str(LIB/(name+'.sbs')),True)
    r=libs[name].findResourceFromUrl(name)
    assert r,name
    n=g.newInstanceNode(r);n.setPosition(float2(x,y))
    for k,v in params.items():value(n,k,v)
    return n

def wire(a,b,port='input1',output=None):
    out=output or a.getProperties(SDPropertyCategory.Output)[0].getId()
    a.newPropertyConnectionFromId(out,b,port)

def constant(g, val, x, y):
    rgba=(val,val,val,1.) if isinstance(val,(float,int)) else (*val,1.)
    n=node(g,'uniform',x,y,outputcolor=rgba)
    if isinstance(val,(float,int)):value(n,'colorswitch',False)
    return n

def levels(g,source,x,y,low=0.,high=1.,outlow=0.,outhigh=1.):
    n=node(g,'levels',x,y,levelinlow=(low,)*4,levelinhigh=(high,)*4,leveloutlow=(outlow,)*4,levelouthigh=(outhigh,)*4)
    wire(source,n);return n

def mix(g,a,b,x,y,opacity=1.,mask=None,mode=0):
    n=node(g,'blend',x,y,opacitymult=opacity,blendingmode=mode,alphablending=False)
    wire(a,n,'destination');wire(b,n,'source')
    if mask:wire(mask,n,'opacity')
    return n

def gradient(g,src,colors,x,y):
    n=node(g,'gradient',x,y)
    t=SDTypeStruct.sNew('sbs::compositing::gradient_key_rgba')
    arr=SDValueArray.sNew(t,0)
    for position,color in colors:
        key=SDValueStruct.sNew(t)
        key.setPropertyValueFromId('value',SDValueColorRGBA.sNew(ColorRGBA(*color,1.)))
        key.setPropertyValueFromId('position',SDValueFloat.sNew(position))
        key.setPropertyValueFromId('midpoint',SDValueFloat.sNew(.5))
        arr.pushBack(key)
    n.setInputPropertyValueFromId('gradientrgba',arr)
    wire(src,n);return n

def output(g,src,identifier,usage,x,y):
    n=node(g,'output',x,y)
    n.setAnnotationPropertyValueFromId('identifier',SDValueString.sNew(identifier))
    n.setAnnotationPropertyValueFromId('label',SDValueString.sNew(identifier))
    arr=SDValueArray.sNew(SDTypeUsage.sNew(),0)
    arr.pushBack(SDValueUsage.sNew(SDUsage.sNew(usage,'RGBA','sRGB' if usage=='baseColor' else 'Linear')))
    n.setAnnotationPropertyValueFromId('usages',arr)
    wire(src,n,'inputNodeOutput');return n

def bricks(g,x,y,count=(6,12),gap=(.035,.055),bevel=(.09,.12,.25,.12),offset=(.5,0.),slope=(.06,.02,0.,0.)):
    return library(g,'brick_generator',x,y,Bricks=count,Gap=gap,Bevel=bevel,Offset=offset,Height=(.86,1.,.10,.08),Slope=slope)

def frames(g):
    for title,pos,size,color in [
        ('GOTHIC | 01 - Patterns and surface wear',(-1140,-180),(1250,1600),(.19,.26,.29,1.)),
        ('GOTHIC | 02 - PBR response / DirectX / ORM',(160,-180),(770,1280),(.28,.25,.19,1.)),
        ('GOTHIC | 03 - Export 2048 x 2048',(970,-180),(300,1790),(.20,.29,.25,1.))]:
        frame=SDGraphObjectFrame.sNew(g);frame.setTitle(title);frame.setPosition(float2(*pos));frame.setSize(float2(*size));frame.setColor(ColorRGBA(*color))

SPECS=[
    ('Stone_Limestone','stone',[(0.,(.26,.25,.22)),(.15,(.49,.49,.43)),(.6,(.67,.65,.55)),(1.,(.79,.77,.65))],(.76,.92),0.,.85),
    ('Wall_Plaster','plaster',[(0.,(.56,.48,.34)),(.45,(.78,.72,.56)),(1.,(.90,.84,.69))],(.78,.94),0.,.40),
    ('Trim_Ivory','trim',[(0.,(.51,.49,.41)),(.4,(.75,.73,.62)),(1.,(.91,.88,.76))],(.63,.83),0.,.36),
    ('Roof_Slate','roof',[(0.,(.06,.12,.17)),(.25,(.13,.23,.29)),(.7,(.23,.38,.43)),(1.,(.38,.51,.53))],(.53,.76),0.,1.25),
    ('Roof_Copper','copper',[(0.,(.13,.24,.22)),(.3,(.22,.43,.37)),(.72,(.32,.54,.46)),(1.,(.52,.59,.43))],(.43,.69),.65,.9),
    ('Roof_Oxblood','roof',[(0.,(.16,.06,.075)),(.3,(.36,.14,.14)),(.7,(.56,.25,.23)),(1.,(.68,.36,.28))],(.6,.82),0.,1.25),
    ('Glass_Amber','glass',[(0.,(.10,.16,.20)),(.14,(.20,.34,.40)),(.65,(.49,.58,.53)),(1.,(.85,.65,.31))],(.20,.38),.15,.38),
    ('Metal_Brass','brass',[(0.,(.30,.21,.10)),(.35,(.57,.40,.18)),(.7,(.76,.58,.29)),(1.,(.90,.74,.43))],(.30,.52),.88,.13),
    ('Metal_Iron','iron',[(0.,(.07,.10,.12)),(.45,(.18,.23,.25)),(1.,(.32,.37,.38))],(.42,.66),.78,.2),
    ('Wood_Walnut','wood',[(0.,(.12,.065,.035)),(.25,(.27,.15,.075)),(.65,(.45,.29,.14)),(1.,(.60,.44,.25))],(.65,.84),0.,.8),
]

def build():
    EXPORT.mkdir(parents=True,exist_ok=True)
    (SRC/'build_complete.txt').unlink(missing_ok=True)
    (SRC/'build_error.txt').unlink(missing_ok=True)
    package=pm.newUserPackage()
    report={'application':'Adobe Substance 3D Designer','resolution':2048,'normal_format':'DirectX','orm_channels':{'R':'ambientOcclusion','G':'roughness','B':'metallic'},'graphs':[]}
    for index,(name,kind,palette,rough,metal,normal_strength) in enumerate(SPECS):
        g=SDSBSCompGraph.sNew(package);g.setIdentifier(name)
        g.setDefaultParentSize(int2(2048,2048))
        g.setPropertyInheritanceMethod(g.getPropertyFromId('$outputsize',SDPropertyCategory.Input),SDPropertyInheritanceMethod.Absolute)
        value(g,'$outputsize',(11,11));value(g,'$randomseed',71+index*13)
        g.setAnnotationPropertyValueFromId('label',SDValueString.sNew(name.replace('_',' ')))
        cloud=library(g,'clouds_2',-1000,350)
        grain=library(g,'grunge_map_001',-1000,600,balance=.54,contrast=.32)
        if kind in ('stone','plaster'):
            pattern=bricks(g,-1000,0,(7,13),(.035,.055))
            warp=node(g,'warp',-770,0,intensity=.7)
            wire(pattern,warp);wire(cloud,warp,'inputgradient')
            height=mix(g,warp,grain,-520,0,opacity=.10,mode=3)
            if kind=='plaster':
                plaster=levels(g,cloud,-770,350,outlow=.68,outhigh=.79)
                chips=levels(g,cloud,-770,600,low=.42,high=.51)
                brickcolor=gradient(g,height,[(0.,(.32,.23,.18)),(.35,(.54,.31,.23)),(1.,(.69,.46,.33))],-270,250)
                plastercolor=gradient(g,cloud,palette,-270,500)
                base=mix(g,brickcolor,plastercolor,0,350,mask=chips)
                height=mix(g,height,plaster,-270,0,mask=chips)
            else:base=gradient(g,height,palette,0,250)
        elif kind in ('roof','copper','glass','wood'):
            if kind=='glass':
                pattern=library(g,'tile_generator',-1000,0,Nb_X=5,Nb_Y=5,Free_Rotation=.125,Scale=.68,Interstice=(.045,.045,0.,0.),BackgroundColor=.06)
            else:
                pattern=bricks(g,-1000,0,(1,12) if kind=='wood' else ((4,5) if kind=='copper' else (7,10)),(.012,.045),(.06,.12,.22,.12),(.0,0.) if kind in ('wood','copper') else (.5,0.),(.05,.05,0.,0.) if kind=='copper' else (.05,.4,0.,0.))
            height=mix(g,pattern,grain,-500,0,opacity=.07,mode=3)
            if kind=='wood':
                fiber=library(g,'anisotropic_noise',-1000,850,X_Amount=4,Y_Amount=160,rotate=True)
                height=mix(g,height,fiber,-270,0,opacity=.20,mode=3)
            base=gradient(g,height,palette,0,250)
            if kind=='copper':
                patina=gradient(g,cloud,[(0.,(.20,.26,.18)),(.6,(.24,.42,.33)),(1.,(.48,.55,.36))],-250,600)
                base=mix(g,base,patina,250,250,opacity=.28,mask=grain)
        else:
            height=levels(g,grain,-500,0,outlow=.43,outhigh=.57)
            base=gradient(g,cloud,palette,0,250)
        normal=node(g,'normal',260,0,intensity=normal_strength,inversedy=True,input2alpha=False)
        wire(height,normal)
        ao=levels(g,height,260,500,outlow=.66 if kind in ('stone','roof') else .83,outhigh=1.)
        roughnode=levels(g,grain,260,700,outlow=rough[0],outhigh=rough[1])
        metallic=constant(g,metal,260,900)
        # Native channel shuffle packs AO/roughness then metallic; no bitmap inputs.
        ar=node(g,'shuffle',510,500,channelred=0,channelgreen=4,channelblue=4,channelalpha=3)
        wire(ao,ar,'input1');wire(roughnode,ar,'input2')
        orm=node(g,'shuffle',740,500,channelred=0,channelgreen=1,channelblue=4,channelalpha=3)
        wire(ar,orm,'input1');wire(metallic,orm,'input2')
        outputs=[('BaseColor',base,'baseColor',0),('Normal',normal,'normal',230),('ORM',orm,'unknown',460),('Height',height,'height',690),('Roughness',roughnode,'roughness',920),('Metallic',metallic,'metallic',1150),('AO',ao,'ambientOcclusion',1380)]
        outnodes=[]
        for identifier,source,usage,y in outputs:outnodes.append((identifier,output(g,source,identifier,usage,1040,y)))
        frames(g)
        pm.savePackageAs(package,str(SRC/'Gothic_Stylized_build.sbs'))
        g.compute()
        files=[]
        for identifier,n in outnodes:
            tex=n.getPropertyValue(n.getProperties(SDPropertyCategory.Output)[0]).get()
            if tex is None:raise RuntimeError('Uncomputed '+name+' '+identifier)
            path=EXPORT/('T_Gothic_'+name+'_'+identifier+'.png')
            tex.save(str(path));files.append(path.name)
        report['graphs'].append({'name':name,'kind':kind,'nodes':len(g.getNodes()),'files':files})
        print('Exported '+name)
    pm.savePackageAs(package,str(SRC/'Gothic_Stylized.sbs'))
    (SRC/'material_manifest.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    app.getUIMgr().openResourceInEditor(g)
    (SRC/'build_complete.txt').write_text('10 native procedural graphs, 70 texture outputs',encoding='utf-8')

try:
    build()
except BaseException:
    (SRC/'build_error.txt').write_text(traceback.format_exc(),encoding='utf-8')
    raise


