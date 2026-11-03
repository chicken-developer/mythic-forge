"""Approved architectural modules, embedded in the native HDA PythonModule.

This file is executed in native_network's namespace. It constructs native SOPs;
it never runs a Python SOP or depends on external geometry when the HDA cooks.
"""
P_HIGH=False
P_LIBRARY=None
P_CACHE={}

def p_group(parent,name):
    return annotate(parent.createNode('subnet',safe_name(name)),
        'Approved concept geometry: continuous profiles, flat buttresses, clean pointed pinnacles.')

def p_poly(parent,lines,closed=False,attrs=None):
    geo=geometry64(parent)
    attributes={k:geo.addAttrib(hou.attribType.Prim,k,'') for k in (attrs or {})}
    for i,line in enumerate(lines):
        poly=geo.createPolygon(is_closed=closed[i] if isinstance(closed,list) else closed)
        for pt in geo.createPoints(line):poly.addVertex(pt)
        for key,values in (attrs or {}).items():poly.setAttribValue(attributes[key],values[i])
    return geo

def p_stash(parent,name,geo):
    n=parent.createNode('stash',name);n.parm('stash').set(geo);return n

def p_mark(parent,node,name,material='trim'):
    return wrangle(parent,'tag_'+safe_name(name),node,
        's@name='+json.dumps(name)+';s@material_id='+json.dumps(material)+';f@jitter=0;')

def p_solid(parent,polygon,vector,name,material='stone'):
    return source_branch(parent,[dict(kind='solid',points=polygon,vector=vector,name=name,material=material,seed=101)],'',len(parent.children()))

def p_box(parent,x,y,z,w,h,d,name,material='stone'):
    return p_solid(parent,[(x-w/2,y,z-d/2),(x+w/2,y,z-d/2),(x+w/2,y+h,z-d/2),(x-w/2,y+h,z-d/2)],(0,0,d),name,material)

def p_bevel(parent,node,offset,name='profile_bevel'):
    n=parent.createNode('polybevel::3.0',name);n.setInput(0,node)
    set_menu(n,'grouptype','edges');set_menu(n,'filletshape','round')
    n.parm('offset').set(offset);n.parm('divisions').set(6 if P_HIGH else 3)
    n.parm('ignoreflatedges').set(1);n.parm('flatangle').set(12.)
    n.parm('detectcollisions').set(1);return n

def p_arch(width,spring,sill,center=0.):
    error=.00025 if P_HIGH else .0012
    count=max(8,math.ceil((math.pi/3)/(2*math.acos(max(-1.,1-error/width)))))
    r=width/2;path=[(center-r,sill),(center-r,spring)]
    for i in range(1,count+1):
        a=i*math.pi/(3*count);path.append((center+r-width*math.cos(a),spring+width*math.sin(a)))
    for i in range(1,count+1):
        a=(count-i)*math.pi/(3*count);path.append((center-r+width*math.cos(a),spring+width*math.sin(a)))
    path.append((center+r,sill));return path

def p_sweep(parent,path,profile,name,closed=False,material='trim',bevel=0.):
    spine=p_stash(parent,'spine_'+name,p_poly(parent,[path],closed))
    section=p_stash(parent,'section_'+name,p_poly(parent,[[(a,b,0.) for a,b in profile]],True))
    n=parent.createNode('sweep::2.0','sweep_'+name);n.setInput(0,spine);n.setInput(1,section)
    for parm,token in [('surfaceshape','input'),('surfacetype','quads'),('upvectortype','z'),('endcaptype','none' if closed else 'single')]:set_menu(n,parm,token)
    n.parm('upvectoratstart').set(0);n.parm('stretcharoundturns').set(1)
    n.parm('maxstretcharoundturns').set(3.)
    rev=parent.createNode('reverse','outward_'+name);rev.setInput(0,n)
    return p_mark(parent,p_bevel(parent,rev,bevel,'bevel_'+name) if bevel else rev,name,material)

def p_ring_profile(x,y,z,rx,rz):
    return [(x+a*rx,y,z+b*rz) for a,b in [(-1,-.78),(-.78,-1),(.78,-1),(1,-.78),(1,.78),(.78,1),(-.78,1),(-1,.78)]]

def p_stack(parent,levels,name,x=0.,z=0.,material='stone',bevel=.006):
    rings=p_stash(parent,'rings_'+name,p_poly(parent,[p_ring_profile(x,y,z,rx,rz) for y,rx,rz in levels],True))
    skin=parent.createNode('skin','loft_'+name);skin.setInput(0,rings);set_menu(skin,'surftype','quads')
    cap=parent.createNode('polycap','closed_'+name);cap.setInput(0,skin)
    return p_mark(parent,p_bevel(parent,cap,bevel,'bevel_'+name) if bevel else cap,name,material)

def p_boolean(parent,a,b,name,operation='subtract'):
    n=parent.createNode('boolean::2.0',name);n.setInput(0,a);n.setInput(1,b)
    set_menu(n,'booleanop',operation)
    if operation=='subtract':set_menu(n,'subtractchoices','aminusb')
    n.parm('correctnormals').set(1);n.parm('removeinlinepoints').set(1);return n

def p_cornice(parent,spec):
    w,h,d=spec['width'],spec['height'],spec['depth']
    c=min(.012,h*.08,d*.05,w*.05);sections=[]
    for f,inset in [(0.,.72),(.18,.72),(.30,.52),(.48,.52),(.65,.22),(.78,.22),(1.,0.)]:
        rx=max(c,w/2-min(w*.10,h*.22)*inset);rz=max(c,d/2-min(d*.28,h*.55)*inset)
        sections.append([(-rx,h*f,-rz+c),(-rx+c,h*f,-rz),(rx-c,h*f,-rz),(rx,h*f,-rz+c),(rx,h*f,rz-c),(rx-c,h*f,rz),(-rx+c,h*f,rz),(-rx,h*f,rz-c)])
    source=p_stash(parent,'approved_moulding_sections',p_poly(parent,sections,True))
    skin=parent.createNode('skin','continuous_stepped_profile');skin.setInput(0,source);set_menu(skin,'surftype','quads')
    cap=parent.createNode('polycap','moulding_end_caps');cap.setInput(0,skin)
    return p_mark(parent,p_bevel(parent,cap,min(.004,h*.025,d*.02,w*.02)),'profiled_cornice',spec['material'])

def p_pinnacle(parent,height,width=None):
    h=height;w=width or min(.40,h*.25);parts=[]
    parts.append(p_stack(parent,[(0,w*.35,w*.35),(.10*h,w*.35,w*.35),(.14*h,w*.44,w*.44),(.19*h,w*.44,w*.44)],'pinnacle_seat',bevel=min(.006,w*.025)))
    body=p_box(parent,0,.17*h,0,w*.72,.33*h,w*.72,'lantern')
    ow=w*.32;spring=.34*h;sill=.23*h;contour=list(reversed(p_arch(ow,spring,sill)))
    cutters=[]
    for i in range(4):
        a=i*math.pi/2
        pts=[(xx*math.cos(a)-w*.46*math.sin(a),yy,-xx*math.sin(a)-w*.46*math.cos(a)) for xx,yy in contour]
        cutters.append(p_solid(parent,pts,(w*.22*math.sin(a),0,w*.22*math.cos(a)),'niche_'+str(i)))
    body=p_boolean(parent,body,merge_nodes(parent,'niche_cutters',cutters),'blind_lancet_recesses')
    parts.append(p_mark(parent,p_bevel(parent,body,min(.004,w*.02)),'lantern','stone'))
    parts.append(p_stack(parent,[(.48*h,w*.38,w*.38),(.52*h,w*.50,w*.50),(.55*h,w*.46,w*.46)],'canopy',bevel=min(.005,w*.025)))
    parts.append(p_stack(parent,[(.53*h,w*.42,w*.42),(.56*h,w*.42,w*.42),(h,max(.002,w*.008),max(.002,w*.008))],'pointed_spire',bevel=min(.002,w*.012)))
    return merge_nodes(parent,'APPROVED_PINNACLE',parts)

def p_panel(parent,spec):
    w=spec['width'];h=spec['height'];ow=spec['opening'];t=spec['thickness'];door=spec['door']
    sill=0. if door else spec['bottom']+h*.16
    spring=min(h*.36,2.7) if door else h*(.38 if spec['profile']=='broad' else .61)
    ow=min(ow,(h-spring-.18)/.866)
    radial_extent=min(.33,(w-ow)*.35,ow*.20)
    spring=min(spring,h-.34-radial_extent-ow*.866)
    contour=list(reversed(p_arch(ow,spring,sill)))
    wall=p_box(parent,0,0,t/2,w,h,t,'wall')
    cutter=p_solid(parent,[(x,y,-.5) for x,y in contour],(0,0,t+1.),'window_cutter')
    wall=p_boolean(parent,wall,cutter,'true_arch_opening')
    parts=[]
    if spec.get('rose'):
        cy=h*.76;r=min(w*.17,h*.15);count=80 if P_HIGH else 48
        circle=[(r*math.cos(i*2*math.pi/count),cy+r*math.sin(i*2*math.pi/count),-.5) for i in range(count)]
        wall=p_boolean(parent,wall,p_solid(parent,list(reversed(circle)),(0,0,t+1.),'rose_cutter'),'true_rose_opening')
        tube=[(.05*math.cos(i*2*math.pi/12),.05*math.sin(i*2*math.pi/12)) for i in range(12)]
        parts.append(p_sweep(parent,[(x,y,-.07) for x,y,_ in circle],tube,'rose_frame',True))
        for i in range(12):
            a=i*2*math.pi/12
            parts.append(p_sweep(parent,[(r*.25*math.cos(a),cy+r*.25*math.sin(a),.03),(r*.95*math.cos(a),cy+r*.95*math.sin(a),.03)],tube,'rose_spoke_'+str(i)))
        disk=p_solid(parent,[(x,y,t*.65) for x,y,_ in circle],(0,0,.012),'rose_glazing','glass');parts.append(disk)
    parts.append(p_mark(parent,p_bevel(parent,wall,min(.012,t*.04)),'wall','stone'))
    scale=min(1.,ow/1.65);radial=min(.33,(w-ow)*.35,ow*.20)/.33
    profile=[(-.015,.035),(.035,.035),(.035,-.035),(.090,-.035),(.110,-.075),(.110,-.115),(.145,-.140),(.185,-.140),(.220,-.110),(.240,-.055),(.290,-.055),(.290,.020),(.330,.020),(.330,.085),(-.015,.085)]
    profile=[(x*radial,z*scale) for x,z in profile]
    parts.append(p_sweep(parent,[(x,y,-.045*scale) for x,y in p_arch(ow,spring,sill)],profile,'continuous_archivolt',bevel=.004*scale))
    if not door:
        # The upper bedding overlaps jambs/mullions by 15 mm; no floating feet.
        sill_profile=[(-.10,.06),(.04,.06),(.10,-.16),(.04,-.38),(-.04,-.40),(-.10,-.32)]
        sw=min(w-.08,ow+radial*.7);sill_profile=[(-sw/2,sill+.015+(yy-.10)*scale,zz*scale) for yy,zz in sill_profile]
        parts.append(p_mark(parent,p_bevel(parent,p_solid(parent,sill_profile,(sw,0,0),'sloping_sill'),.008*scale),'sloping_sill','trim'))
        glazing=list(reversed(p_arch(ow-.012*scale,spring,sill+.008)))
        parts.append(p_solid(parent,[(x,y,t*.65) for x,y in glazing],(0,0,.012),'recessed_glazing','glass'))
        tube=[(.035*scale*math.cos(i*2*math.pi/(16 if P_HIGH else 10)),.035*scale*math.sin(i*2*math.pi/(16 if P_HIGH else 10))) for i in range(16 if P_HIGH else 10)]
        if ow>=1.2 and h>=3.8 and spec['detail']>=.45:
            ribs=[];lw=ow*.70/1.65;ls=spring-ow*.224/1.65
            for i,center in enumerate((-ow*.39/1.65,ow*.39/1.65)):
                ribs.append(p_sweep(parent,[(x,y,.06*scale) for x,y in p_arch(lw,ls,sill,center)],tube,'lancet_'+str(i)))
            rr=ow*.267/1.65;ry=ls+math.sqrt((lw+rr)**2-(ow*.74/1.65)**2)
            count=80 if P_HIGH else 48
            ribs.append(p_sweep(parent,[(rr*math.cos(i*2*math.pi/count),ry+rr*math.sin(i*2*math.pi/count),.06*scale) for i in range(count)],tube,'roundel',True))
            ribs.append(p_box(parent,0,sill,.065*scale,.16*scale,ls+.04*scale-sill,.10*scale,'mullion','trim'))
            joined=ribs[0]
            for i,rib in enumerate(ribs[1:]):joined=p_boolean(parent,joined,rib,'join_tracery_'+str(i),'union')
            parts.append(p_mark(parent,joined,'connected_tracery','trim'))
        elif ow>.65:
            parts.append(p_mark(parent,p_bevel(parent,p_box(parent,0,sill,t*.38,.065*scale,spring+ow*.866-sill+.012*scale,.08*scale,'mullion','trim'),.005*scale),'mullion','trim'))
    elif spec.get('closed_door'):
        parts.append(p_solid(parent,[(x,y,t*.50) for x,y in contour],(0,0,.06),'door_leaf','wood'))
    return merge_nodes(parent,'APPROVED_PANEL',parts)

def p_cached_module(parent,spec,prefix):
    global P_CACHE
    kind=spec['module'];shape={k:v for k,v in spec.items() if k not in ('origin','u','normal','name','seed','kind')}
    key=json.dumps(shape,sort_keys=True)
    if key not in P_CACHE:
        module=p_group(P_LIBRARY,kind+'_'+str(len(P_CACHE)))
        if kind=='panel':out=p_panel(module,spec)
        elif kind=='pinnacle':out=p_pinnacle(module,spec['height'])
        elif kind=='cornice':out=p_cornice(module,spec)
        else:
            h=spec['height'];top=h+.5
            out=p_stack(module,[(0,.40,.43),(.14,.40,.43),(.24,.33,.36),(.50,.33,.36),(.64,.27,.30),(.5+h*.45,.27,.30),(.62+h*.45,.22,.27),(top,.22,.27)],'stepped_buttress')
            cap=p_stack(module,[(top-.08,.22,.27),(top,.32,.34),(top+.10,.32,.34)],'buttress_capital',bevel=.009)
            pinnacle=p_pinnacle(module,1.25,.38)
            place=module.createNode('xform','seat_pinnacle');place.setInput(0,pinnacle);place.parm('ty').set(top)
            out=merge_nodes(module,'APPROVED_BUTTRESS',[out,cap,place])
        out.setDisplayFlag(True);out.setRenderFlag(True)
        module.layoutChildren();P_CACHE[key]=out
    original=P_CACHE[key]
    source=parent.createNode('object_merge',kind+'_instance');source.parm('objpath1').set(source.relativePathTo(original));set_menu(source,'xformtype','none')
    o=spec['origin'];u=spec.get('u',(1,0,0));normal=spec.get('normal',(0,0,-1))
    def v(values):return 'set('+','.join(format(float(x),'.15g') for x in values)+')'
    placed=wrangle(parent,'place_'+kind,source,'@P='+v(o)+'+@P.x*'+v(u)+'+set(0,@P.y,0)-@P.z*'+v(normal)+';',owner='point')
    # Reverse reflected rear-facing panels before generating final vertex normals.
    determinant=u[0]*(-normal[2])-u[2]*(-normal[0])
    if determinant<0:
        n=parent.createNode('reverse','outward_rear_panel');n.setInput(0,placed);placed=n
    return wrangle(parent,'identify_'+kind,placed,'s@name='+json.dumps(prefix+spec['name']+'/')+'+s@name;')

def p_compile_beams(parent,operations,prefix):
    groups={}
    for op in operations:
        key=(round(op['radius'],8),op['material'])
        groups.setdefault(key,[]).append(op)
    outputs=[]
    for (r,material),items in groups.items():
        # Join adjacent segments of each named architectural curve, never capped bars.
        paths=[];names=[];closed_flags=[];pending={}
        for op in items:
            name=re.sub(r'\d+$','',op['name']);pending.setdefault(name,[]).append((tuple(op['a']),tuple(op['b'])))
        for name,segments in pending.items():
            while segments:
                a,b=segments.pop(0);path=[a,b];changed=True
                while changed:
                    changed=False
                    for i,(c,d) in enumerate(segments):
                        if sum((v-w)**2 for v,w in zip(path[-1],c))<1e-12:path.append(d)
                        elif sum((v-w)**2 for v,w in zip(path[-1],d))<1e-12:path.append(c)
                        else:continue
                        segments.pop(i);changed=True;break
                is_closed=len(path)>2 and sum((a-b)**2 for a,b in zip(path[0],path[-1]))<1e-12
                if is_closed:path.pop()
                paths.append(path);names.append(prefix+name);closed_flags.append(is_closed)
        for closed in (False,True):
            selected=[i for i,flag in enumerate(closed_flags) if flag==closed]
            if not selected:continue
            group=p_group(parent,'continuous_beams_'+str(len(outputs)))
            geo=p_poly(group,[paths[i] for i in selected],closed,{'name':[names[i] for i in selected],'material_id':[material]*len(selected)})
            up=geo.addAttrib(hou.attribType.Point,'start_up',(0.,0.,1.))
            for prim in geo.prims():
                positions=[v.point().position() for v in prim.vertices()]
                tangent=(positions[1]-positions[0]).normalized()
                normal=None
                for point in positions[2:]:
                    candidate=tangent.cross(point-positions[0])
                    if candidate.length()>1e-8:normal=candidate.normalized();break
                if normal is None:
                    axis=min((hou.Vector3(1,0,0),hou.Vector3(0,1,0),hou.Vector3(0,0,1)),key=lambda v:abs(v.dot(tangent)))
                    normal=tangent.cross(axis).normalized()
                for vertex in prim.vertices():vertex.point().setAttribValue(up,normal)
            spine=p_stash(group,'architectural_curves',geo)
            profile=[(-r,-r*.75),(-r*.75,-r),(r*.75,-r),(r,-r*.75),(r,r*.75),(r*.75,r),(-r*.75,r),(-r,r*.75)]
            sec=p_stash(group,'chamfered_section',p_poly(group,[[(a,b,0) for a,b in profile]],True))
            sweep=group.createNode('sweep::2.0','connected_architectural_profiles');sweep.setInput(0,spine);sweep.setInput(1,sec)
            for parm,token in [('surfaceshape','input'),('surfacetype','quads'),('upvectortype','attrib'),('endcaptype','none' if closed else 'single')]:set_menu(sweep,parm,token)
            sweep.parm('upvectorattrib').set('start_up');sweep.parm('upvectoratstart').set(0)
            sweep.parm('stretcharoundturns').set(1);sweep.parm('maxstretcharoundturns').set(3.)
            rev=group.createNode('reverse','outward_winding');rev.setInput(0,sweep)
            result=wrangle(group,'beam_materials',rev,'s@material_id='+json.dumps(material)+';f@jitter=0;')
            result.setDisplayFlag(True);result.setRenderFlag(True);group.layoutChildren();outputs.append(group)
    return outputs

def p_standard_finish(parent,source):
    solids=blast(parent,'beveled_construction_solids',source,'@source_kind=solid')
    faces=blast(parent,'original_surface_patches',source,'@source_kind=surface')
    return merge_nodes(parent,'beveled_assembly',[p_bevel(parent,solids,.006,'construction_edge_bevel'),faces])
