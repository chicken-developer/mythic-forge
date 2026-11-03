import hou
b=hou.node('/obj/medieval_roof_high_v3/BUILDING')
result={}
if b:
    for tag,n in [('asset',b),('display',b.displayNode()),('final',b.node('OUT_BUILDING'))]:
        g=n.geometry();names=list(g.primStringAttribValues('name'))
        result[tag]={'path':n.path(),'prims':len(g.prims()),'canvas':sum('awning_canvas' in x for x in names),'awning_posts':sum('awning_post' in x for x in names)}
    m=b.node('materials/canvas');result['material']={p.name():p.eval() for p in m.parms() if any(k in p.name().lower() for k in ('alpha','opacity','ogl_'))}
