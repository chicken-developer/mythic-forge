import hou
from hda_builder.evidence import ROOT,write_json,geometry_metrics
obj=hou.node('/obj').createNode('geo','native_cleanup_test',run_init_scripts=False)
src=obj.createNode('file');src.parm('file').set(str(ROOT/'geometry/before/baseline.bgeo.sc'))
results=[]
for label,kind,params in [
 ('clean_degenerate','clean',{'fusepts':0,'deldegengeo':1,'degentol':1e-8,'fixoverlap':1,'deleteoverlap':0,'delunusedpts':1}),
 ('doctor_overlap','polydoctor',{'illformed':0,'manyedges':0,'nonconvex':0,'overlapping':2,'intersect':0,'pairoverlaps':0}),
]:
    node=obj.createNode(kind,label);node.setInput(0,src);node.setParms(params)
    m=geometry_metrics(node.geometry());results.append({'label':label,'metrics':m})
    print(label,m['points'],m['primitives'],m['degenerate_polygons'],m['exact_duplicate_polygons'],flush=True)
write_json(ROOT/'results/cleanup_probe.json',results)
