"""Compare normal projection methods using already exported geometry."""
from pathlib import Path
import hou
import numpy as np
from PIL import Image
from bake_variants import node
ROOT=Path(__file__).resolve().parents[1]
obj=hou.node('/obj').createNode('geo','diagnostic',run_init_scripts=False)
low=node(obj,'file','low',file=str(ROOT/'geometry/baseline.bgeo.sc'))
high=node(obj,'file','high',file=str(ROOT.parent/'_render/SourceAssets/Medieval15/High/baseline.bgeo.sc'))
promote=node(obj,'attribpromote','cage_normal',low,inname='N',inclass=3,outclass=2)
cage=node(obj,'attribwrangle','CAGE',promote,**{'class':2,'snippet':'@P+=normalize(v@N)*.025;'})
cop=hou.node('/img').createNode('copnet','diagnostic')
imports=[]
for name,sop in [('low',low),('high',high),('cage',cage)]:
    imports.append(node(cop,'sopimport',name,usesoppath=1,soppath=sop.path()))
for mode in ('same','high','cage'):
    b=node(cop,'bakegeometrytextures',mode,tracingmode='cagemesh' if mode=='cage' else 'surfacenormal',
           raylengthmissing='custom',raylength=.075,raybias=.001,enablematchpieces=1,pieceattrib='bake_piece',
           bakenormal=1,normaltype='offset',flipN='down',bakeocclusion=0,bakealpha=0,enableuvfilling=0)
    b.setInput(1,imports[0]);b.setInput(2,imports[0] if mode=='same' else imports[1])
    if mode=='cage':b.setInput(3,imports[2])
    out=node(cop,'rop_image','write_'+mode,b,copoutput=str(ROOT/'review_20261102'/('normal_'+mode+'.png')),
             setres=1,res1=512,res2=512,colorconversion='raw',savebackground=0)
    out.render()
    px=np.asarray(Image.open(ROOT/'review_20261102'/('normal_'+mode+'.png')).convert('RGB'))
    print(mode,'negative_z_pixels',np.count_nonzero((px[:,:,2]<120)&(px[:,:,0]>0)),'variation',np.count_nonzero(np.max(np.abs(px[:,:,:2].astype(int)-128),axis=2)>8),flush=True)
