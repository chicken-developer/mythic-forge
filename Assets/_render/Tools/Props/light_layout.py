"""Find actual connected lamp globes; primitive names may repeat on different floors."""
from collections import defaultdict

def sources(geo):
    parents={};points={};names={}
    def root(key):
        while parents[key]!=key:
            parents[key]=parents[parents[key]];key=parents[key]
        return key
    for prim in geo.prims():
        name=prim.attribValue('name')
        if 'light_source' not in name:continue
        keys=[]
        for vertex in prim.vertices():
            point=vertex.point();key=(name,point.number())
            parents.setdefault(key,key);points[key]=tuple(point.position());names[key]=name;keys.append(key)
        first=root(keys[0])
        for key in keys[1:]:parents[root(key)]=first
    groups=defaultdict(list)
    for key,position in points.items():groups[(names[key],root(key))].append(position)
    result=[]
    for (name,_),positions in groups.items():
        center=tuple((min(p[i] for p in positions)+max(p[i] for p in positions))*.5 for i in range(3))
        result.append({'name':name,'position_m':center})
    result.sort(key=lambda row:(row['name'],row['position_m'][1],row['position_m'][0],row['position_m'][2]))
    for i,row in enumerate(result):row['name']+='/source_'+str(i)
    return result
