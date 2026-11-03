"""Check projected opening clearance against the actual cooked brace geometry."""
import math


def audit(geo):
    groups = {}
    for prim in geo.prims():
        name = prim.attribValue('name')
        if ('opening_clear' in name or 'joint_awning_footing' in name or 'continuous_glazing' in name
                or 'door_board' in name or 'door_leaf' in name
                or ('glass' in name and 'light_source' not in name)):
            groups.setdefault(name, []).append(prim)
    braces = {name: [tuple(v.point().position()) for p in prims for v in p.vertices()]
              for name, prims in groups.items() if 'opening_clear' in name}
    checked, minimum = 0, None
    footing_contacts=0
    footings=[[tuple(v.point().position()) for p in prims for v in p.vertices()]
              for name,prims in groups.items() if 'joint_awning_footing' in name]
    for name,points in braces.items():
        if 'awning_post' not in name:continue
        low=min(p[1] for p in points)
        center=[(min(p[i] for p in points)+max(p[i] for p in points))/2 for i in (0,2)]
        candidates=[f for f in footings if all(min(p[i] for p in f)-1e-5<=center[j]<=max(p[i] for p in f)+1e-5 for j,i in enumerate((0,2)))]
        assert any(abs(low-max(p[1] for p in f))<1e-5 for f in candidates),(name,'post does not bear on footing')
        footing_contacts+=1
    conflicts = []
    for name, prims in groups.items():
        if 'opening_clear' in name or 'joint_awning_footing' in name:
            continue
        normal = tuple(prims[0].normal())
        length = math.hypot(normal[0], normal[2])
        if length < .9:
            continue
        normal = (normal[0]/length, 0., normal[2]/length)
        along = (normal[2], 0., -normal[0])
        dot = lambda a,b: sum(x*y for x,y in zip(a,b))
        positions = [tuple(v.point().position()) for p in prims for v in p.vertices()]
        bounds = lambda points,axis: (min(dot(p,axis) for p in points),max(dot(p,axis) for p in points))
        wl, wh = bounds(positions, along)
        yl, yh = bounds(positions, (0,1,0))
        plane = sum(dot(p,normal) for p in positions)/len(positions)
        for brace, points in braces.items():
            bl, bh = bounds(points, along)
            by0, by1 = bounds(points, (0,1,0))
            if min(yh,by1)-max(yl,by0) < .025:
                continue
            near = min(abs(dot(p,normal)-plane) for p in points)
            if near > (2.0 if 'awning' in brace else .35):
                continue
            gap = max(wl-bh,bl-wh)
            checked += 1
            minimum = gap if minimum is None else min(minimum,gap)
            if gap < .015:
                conflicts.append({'brace':brace,'opening':name,'projected_gap_m':gap})
    assert not conflicts, conflicts[:5]
    names = geo.primStringAttribValues('name')
    assert not any('SKYTOWN|' in name and '/SkyTown/' not in name for name in names), 'Unresolved prop marker'
    return {'support_groups':len(braces),'opening_support_pairs_checked':checked,
            'minimum_projected_gap_m':minimum,'blocked_openings':0,'awning_footing_contacts':footing_contacts}
