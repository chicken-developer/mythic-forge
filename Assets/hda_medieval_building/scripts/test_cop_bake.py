"""Capability test for the Houdini 22 native GPU high-to-low baker."""
from pathlib import Path
import hou
root=Path(__file__).resolve().parents[1]
obj=hou.node('/obj').createNode('geo','bake_test',run_init_scripts=False)
box=obj.createNode('box');box.parmTuple('size').set((1,1,1))
uv=obj.createNode('uvunwrap');uv.setInput(0,box)
low=obj.createNode('normal');low.setInput(0,uv)
high=obj.createNode('polybevel::3.0');high.setInput(0,low)
high.setParms({'offset':.04,'divisions':3,'ignoreflatedges':1})
highn=obj.createNode('normal','high_normal');highn.setInput(0,high)
cop=hou.node('/img').createNode('copnet','bake_test')
imports=[]
for name,node in [('low',low),('high',highn)]:
    c=cop.createNode('sopimport',name);c.setParms({'usesoppath':1,'soppath':node.path()});imports.append(c)
bake=cop.createNode('bakegeometrytextures')
bake.setInput(1,imports[0]);bake.setInput(2,imports[1])
bake.setParms({'tracingmode':'surfacenormal','raylengthmissing':'custom','raylength':.15,
              'raybias':.07,'bakeocclusion':0,'bakealpha':0,'bakenormal':1,'normaltype':'offset',
              'flipN':'down','enableuvfilling':1,'edgepadding':.005})
rop=cop.createNode('rop_image');rop.setInput(0,bake,0)
rop.setParms({'copoutput':str(root/'review_20261102/test_normal.png'),
              'setres':1,'res1':512,'res2':512,'colorconversion':'raw','savebackground':0})
rop.render()
print('BAKED',bake.errors(),bake.warnings(),flush=True)
