"""Compile Gothic construction profiles into inspectable native SOP networks.

Python computes custom architectural profiles and updates the graph on demand.
The saved graph uses Stash/Edit, PolyExtrude, Clip, Merge, Color, Normal and
Transform SOPs; no Python SOP evaluates the final building geometry.
"""
import hashlib
import json
import math
import re

import hou

CORE_SOURCE = ""
PRODUCTION_SOURCE = ""
PRESETS = {}
_BUSY = set()
_GEOMETRY_TEMPLATE = None
BOUNDARY_SNAP_CODE = '\nvector normals[];\nfloat offsets[];\nforeach(vector4 plane; planes) {\n    vector n = set(plane.x,plane.y,plane.z);\n    float magnitude = length(n);\n    n /= magnitude;\n    float offset = plane.w/magnitude;\n    if(abs(dot(n,@P)+offset)>1e-5) continue;\n    int independent = len(normals)==0;\n    if(len(normals)==1) independent=length(cross(normals[0],n))>1e-4;\n    if(len(normals)==2) independent=abs(dot(cross(normals[0],normals[1]),n))>1e-4;\n    if(independent) { append(normals,n); append(offsets,offset); }\n    if(len(normals)==3) break;\n}\nif(len(normals)==1) {\n    @P -= normals[0]*(dot(normals[0],@P)+offsets[0]);\n} else if(len(normals)==2) {\n    float a=dot(normals[0],@P)+offsets[0];\n    float b=dot(normals[1],@P)+offsets[1];\n    float r=dot(normals[0],normals[1]);\n    @P -= ((a-r*b)*normals[0]+(b-r*a)*normals[1])/(1-r*r);\n} else if(len(normals)==3) {\n    matrix3 basis=set(normals[0],normals[1],normals[2]);\n    @P = -set(offsets[0],offsets[1],offsets[2])*invert(transpose(basis));\n}\n'
FINISH_PARAMETERS = {"stone_palette", "roof_material", "gold_accents", "global_scale"}


def core():
    namespace = {}
    exec(compile(CORE_SOURCE, "embedded_gothic_profiles", "exec"), namespace)
    return namespace


def config_from_node(node):
    b = core()
    parameters = {}
    for name, schema in b["SCHEMA"].items():
        parm = node.parm(name)
        value = parm.evalAsString() if schema["type"] == "string" else parm.eval()
        parameters[name] = bool(value) if schema["type"] == "boolean" else value
    return b["normalize"]({"parameters": parameters,
                           "seeds": {key: node.evalParm("seed_" + key) for key in ("layout", "decoration")}})


def recipes(config):
    b = core()
    b['CURVE_SEGMENTS'] = 128 if P_HIGH else 64
    base_mesh, base_merge, base_trim = b["Mesh"], b["merge_mesh"], b["trim_mesh"]

    class ProfileMesh(base_mesh):
        def __init__(self, parameters, seed):
            super().__init__(parameters, seed)
            self.ops = []
            self.record_depth = 0

        def face(self, points, name, mat="stone"):
            before = len(self.faces)
            super().face(points, name, mat)
            if self.record_depth == 0 and len(self.faces) > before:
                self.ops.append({"kind": "surface",
                                 "points": [self.vertices[i] for i in self.faces[-1]],
                                 "name": name, "material": mat, "seed": self.seed})

        def solid(self, polygon, vector, name, mat="stone"):
            before = len(self.faces)
            self.record_depth += 1
            try:
                super().solid(polygon, vector, name, mat)
            finally:
                self.record_depth -= 1
            if len(self.faces) > before:
                self.ops.append({"kind": "solid", "points": b["clean_polygon"](polygon),
                                 "vector": tuple(vector), "name": name,
                                 "material": mat, "seed": self.seed})

        def beam(self,a,b,r,name,mat='trim'):
            before=len(self.ops)
            super().beam(a,b,r,name,mat)
            del self.ops[before:]
            self.ops.append(dict(kind='beam',a=a,b=b,radius=r,name=name,material=mat))

        def box(self,x,y,z,w,h,d,name,mat='stone'):
            before=len(self.ops)
            super().box(x,y,z,w,h,d,name,mat)
            if min(w,h,d)>1e-7 and any(token in name for token in ('cornice','floor_course','side_course','town_band','coping')):
                del self.ops[before:]
                self.ops.append(dict(kind='architecture',module='cornice',origin=(x,y,z),width=w,height=h,depth=d,material=mat,name=name))

    base_panel,base_pinnacle,base_buttress=b['panel'],b['pinnacle'],b['buttress']
    def panel(mesh,origin,u,normal,width,height,opening,name,door=False,bottom=.45,profile='lancet'):
        before=len(mesh.ops)
        base_panel(mesh,origin,u,normal,width,height,opening,name,door,bottom,profile)
        del mesh.ops[before:]
        mesh.ops.append(dict(kind='architecture',module='panel',origin=origin,u=u,normal=normal,
            width=width,height=height,opening=opening,name=name,door=door,bottom=bottom,profile=profile,
            thickness=mesh.p['wall_thickness'],detail=mesh.p['detail'],
            rose=door and name.startswith('nave_end') and height>=5.,
            closed_door=door and '_door_' in name))
    def pinnacle(mesh,x,y,z,h,name):
        before=len(mesh.ops);base_pinnacle(mesh,x,y,z,h,name);del mesh.ops[before:]
        mesh.ops.append(dict(kind='architecture',module='pinnacle',origin=(x,y,z),height=h+.35,name=name))
    def buttress(mesh,x,z,h,name):
        before=len(mesh.ops);base_buttress(mesh,x,z,h,name);del mesh.ops[before:]
        mesh.ops.append(dict(kind='architecture',module='buttress',origin=(x,0,z),height=h,name=name))
    b['panel']=panel;b['pinnacle']=pinnacle;b['buttress']=buttress

    def merge(destination, source, prefix=""):
        destination.record_depth += 1
        try:
            base_merge(destination, source, prefix)
        finally:
            destination.record_depth -= 1
        destination.ops.append({"kind": "merge", "source": source.ops, "prefix": prefix})

    def trim(source, volumes, label):
        # Plan the cutters; clipping is performed exclusively by native SOPs.
        result = ProfileMesh(source.p, source.seed)
        for name in ("vertices","faces","names","materials","colors"):
            setattr(result,name,getattr(source,name))
        result.features = source.features.copy()
        result.trim_stats = {"module": label, "computed_by_native_sops": True}
        result.ops = [{"kind": "trim", "source": source.ops, "volumes": volumes,
                       "label": label,
                       "bounds": [[min(p[i] for p in source.vertices) for i in range(3)],
                                  [max(p[i] for p in source.vertices) for i in range(3)]]}]
        return result

    b["Mesh"] = ProfileMesh
    b["merge_mesh"] = merge
    b["trim_mesh"] = trim
    config = b["normalize"](config)
    p = config["parameters"]
    model = ProfileMesh(p,config["seeds"]["decoration"])
    model.seed = (config["seeds"]["layout"]*1000003+config["seeds"]["decoration"])%2147483647
    if p["building_type"]=="church":
        b["church"](model,p)
    elif p["building_type"]=="townhouse":
        b["townhouse"](model,p)
    elif p["building_type"]=="gatehouse":
        b["urban_gate" if p["gate_composition"]=="urban_asymmetric" else "gatehouse"](model,p)
    elif p['building_type'] in ('residence','public'):
        b['urban_building'](model,p)
    else:
        b["tower"](model,0,0,p["nave_width"]*.45,p["tower_floors"],p["tower_floor_height"],
                   p["spire_height"],"tower",clock=p["clock_enable"],window_floor=0)
    return model.ops


def safe_name(name):
    return re.sub("[^a-zA-Z0-9_]", "_", name.replace("-1", "left").replace("/","_")).strip("_") or "module"


def annotate(node, text, color=(.35, .5, .65)):
    node.setComment(text)
    node.setGenericFlag(hou.nodeFlag.DisplayComment, True)
    node.setColor(hou.Color(color))
    return node


def set_menu(node, name, token):
    parm = node.parm(name)
    template = parm.parmTemplate()
    parm.set(token if isinstance(template, hou.StringParmTemplate) else template.menuItems().index(token))


def geometry64(parent):
    global _GEOMETRY_TEMPLATE
    if _GEOMETRY_TEMPLATE is None:
        # AttribCast is an HDA: its internals must evaluate outside Manual mode.
        previous = hou.updateModeSetting()
        hou.setUpdateMode(hou.updateMode.AutoUpdate)
        source = cast = None
        try:
            schema = hou.Geometry()
            schema.addAttrib(hou.attribType.Point,"extrude_direction",(0.,1.,0.))
            schema.addAttrib(hou.attribType.Prim,"extrude_distance",0.)
            schema.addAttrib(hou.attribType.Prim,"jitter",0.)
            schema.createPoint()
            source = parent.createNode("stash","__precision_schema")
            source.parm("stash").set(schema)
            cast = parent.createNode("attribcast","__double_precision")
            cast.setInput(0,source)
            set_menu(cast,"preferredprecision","64")
            cast.parm("numcasts").set(2)
            set_menu(cast,"class1","point")
            cast.parm("attribs1").set("P extrude_direction")
            set_menu(cast,"precision1","fpreal64")
            set_menu(cast,"class2","primitive")
            cast.parm("attribs2").set("extrude_distance jitter")
            set_menu(cast,"precision2","fpreal64")
            cast.cook(force=True)
            _GEOMETRY_TEMPLATE = hou.Geometry(cast.geometry())
            _GEOMETRY_TEMPLATE.deletePoints(_GEOMETRY_TEMPLATE.points())
            if _GEOMETRY_TEMPLATE.findPointAttrib("P").numericDataType()!=hou.numericData.Float64:
                raise RuntimeError("The native profile schema must retain float64 P")
        finally:
            if cast is not None:
                cast.destroy()
            if source is not None:
                source.destroy()
            hou.setUpdateMode(previous)
    geometry = hou.Geometry(_GEOMETRY_TEMPLATE)
    geometry.setPreferredPrecision(64)
    return geometry


def wrangle(parent, name, source, text, owner="primitive"):
    node = parent.createNode("attribwrangle", name)
    node.setInput(0, source)
    node.parm("class").set({"detail": 0, "primitive": 1, "point": 2, "vertex": 3}[owner])
    node.parm("snippet").set(text)
    set_menu(node, "vex_precision", "64")
    return node


def blast(parent, name, source, group, keep=True):
    node = parent.createNode("blast", name)
    node.setInput(0, source)
    node.parm("grouptype").set("prims")
    node.parm("group").set(group)
    node.parm("negate").set(int(keep))
    return node


def merge_nodes(parent, name, inputs):
    if len(inputs) == 1:
        return inputs[0]
    node = parent.createNode("merge", name)
    for index, source in enumerate(inputs):
        node.setInput(index, source)
    return node


def profiles_geometry(operations, prefix, parent):
    geometry = geometry64(parent)
    attrs = {name: (geometry.findPrimAttrib(name) or geometry.addAttrib(hou.attribType.Prim, name, value))
             for name, value in (("name", ""), ("material_id", ""), ("source_kind", ""),
                                 ("extrude_distance", 0.0), ("jitter", 0.0))}
    direction_attr = geometry.findPointAttrib("extrude_direction")
    kinds = set()
    for operation in operations:
        points = [tuple(p) for p in operation["points"]]
        if len(points) < 3:
            continue
        kind = operation["kind"]
        direction = hou.Vector3(operation.get("vector", (0, 0, 0)))
        length = direction.length()
        if kind == "solid":
            if length < 1e-9:
                continue
            normal = (hou.Vector3(points[1])-hou.Vector3(points[0])).cross(
                hou.Vector3(points[2])-hou.Vector3(points[0]))
            # Houdini's polygon convention is clockwise viewed from its normal.
            if (-normal).dot(direction) < 0:
                points.reverse()
            direction /= length
        else:
            points.reverse()
        polygon = geometry.createPolygon()
        for point in geometry.createPoints(points):
            point.setAttribValue(direction_attr, tuple(direction))
            polygon.addVertex(point)
        jitter = (int(hashlib.sha256((operation["name"] + str(operation["seed"])).encode()).hexdigest()[:8], 16)
                  % 101 / 100. - .5) * .075
        values = {"name": prefix + operation["name"], "material_id": operation["material"],
                  "source_kind": kind, "extrude_distance": length, "jitter": jitter}
        for name, value in values.items():
            polygon.setAttribValue(attrs[name], value)
        kinds.add(kind)
    return geometry, kinds


def source_branch(parent, operations, prefix, index):
    geometry, kinds = profiles_geometry(operations, prefix, parent)
    source = annotate(parent.createNode("stash", "PROFILES_" + str(index)),
                      "Embedded Gothic wall/opening profiles. No external geometry file.\n"
                      "Inspect these planar polygons before the native PolyExtrude stage.", (.35, .55, .4))
    source.parm("stash").set(geometry)
    edit = annotate(parent.createNode("edit", "edit_profiles_" + str(index)),
                    "Manual profile edits. Disable Auto Rebuild before editing generated modules.")
    edit.setInput(0, source)
    outputs = []
    if "solid" in kinds:
        solids = blast(parent, "solid_profiles_" + str(index), edit, "@source_kind=solid")
        extrude = annotate(parent.createNode("polyextrude::2.0", "extrude_walls_roofs_trim_" + str(index)),
                           "Native individual polygon extrusion.\n"
                           "extrude_direction points along the extrusion; extrude_distance gives its length.")
        extrude.setInput(0, solids)
        extrude.parm("splittype").set("elements")
        extrude.parm("extrusionmode").set("pointnormal")
        extrude.parm("ptnormalsrc").set("attribute")
        extrude.parm("ptnormalattrib").set("extrude_direction")
        extrude.parm("dist").set(1.)
        extrude.parm("uselocalzscaleattrib").set(1)
        extrude.parm("localzscaleattrib").set("extrude_distance")
        extrude.parm("outputfront").set(1)
        extrude.parm("outputback").set(1)
        extrude.parm("outputside").set(1)
        outputs.append(extrude)
    if "surface" in kinds:
        outputs.append(blast(parent, "surface_patches_" + str(index), edit, "@source_kind=surface"))
    return merge_nodes(parent, "merge_profile_surfaces_" + str(index), outputs)


def clip_volume(parent, source, volume, index):
    subnet = annotate(parent.createNode("subnet", "CUT_" + str(index) + "_" + safe_name(volume["label"])),
                      "Subtract the occupied convex architectural envelope using native Clip SOPs.\n"
                      "Each plane has editable Origin and Direction; outside fragments are retained.",
                      (.65, .35, .3))
    subnet.setInput(0, source)
    incoming = subnet.indirectInputs()[0]
    planes = volume["planes"]
    code = "vector4 planes[];\n"
    for plane_index, plane in enumerate(planes):
        values = ",".join(format(float(v), ".17g") for v in plane)
        code += "vector4 p%d = set(%s); append(planes,p%d);\n" % (plane_index, values, plane_index)
    plane_header = code
    code += """int untouched = 0;
foreach (vector4 plane; planes) {
    float maximum = -1e30;
    foreach (int pt; primpoints(0, @primnum)) {
        vector p = point(0, "P", pt);
        maximum = max(maximum, dot(set(plane.x,plane.y,plane.z),p)+plane.w);
    }
    if (maximum <= 1e-8) { untouched=1; break; }
}
i@group_keep_boundary = untouched;
"""
    guard = annotate(wrangle(subnet, "retain_coplanar_and_outside", incoming, code),
                     "Retain a face lying on/outside any envelope plane, including coplanar interfaces.")
    unchanged = blast(subnet, "unaffected_surfaces", guard, "keep_boundary")
    remaining = blast(subnet, "surfaces_to_clip", guard, "keep_boundary", keep=False)
    exterior = [unchanged]
    for plane_index, plane in enumerate(planes):
        n = hou.Vector3(plane[:3])
        origin = n * (-float(plane[3]) / n.lengthSquared())
        for side in ("below", "above"):
            clip = subnet.createNode("clip::2.0", "plane_%02d_%s" % (plane_index, side))
            clip.setInput(0, remaining)
            clip.parm("clipop").set(side)
            clip.parmTuple("origin").set(tuple(origin))
            clip.parmTuple("dir").set(tuple(n.normalized()))
            if side == "below":
                exterior.append(clip)
            else:
                next_remaining = clip
        remaining = next_remaining
    output = merge_nodes(subnet, "OUTSIDE_ENVELOPE", exterior)
    precision = subnet.createNode("attribcast", "cut_position_precision")
    precision.setInput(0,output)
    set_menu(precision,"preferredprecision","64")
    precision.parm("numcasts").set(1)
    set_menu(precision,"class1","point")
    precision.parm("attribs1").set("P")
    set_menu(precision,"precision1","fpreal64")
    output = annotate(wrangle(subnet,"align_cut_boundaries",precision,plane_header+BOUNDARY_SNAP_CODE,owner="point"),
        "Align Clip's numerical boundary deviations to the exact envelope planes.\n"
        "Only points within 10 micrometres of a plane are adjusted.", (.5,.45,.3))
    output.setDisplayFlag(True)
    output.setRenderFlag(True)
    subnet.layoutChildren()
    return subnet


def overlaps(a, b):
    return all(a[1][i] > b[0][i]+1e-8 and a[0][i] < b[1][i]-1e-8 for i in range(3))


def compile_recipes(parent, operations, prefix=""):
    branches = []
    leaves = [operation for operation in operations if operation["kind"] in ("solid", "surface")]
    if leaves:
        branches.append(p_standard_finish(parent,source_branch(parent, leaves, prefix, 0)))
    beams=[op for op in operations if op['kind']=='beam']
    if beams:branches.extend(p_compile_beams(parent,beams,prefix))
    for index, operation in enumerate(operations):
        if operation['kind']=='architecture':
            branches.append(p_cached_module(parent,operation,prefix))
        elif operation["kind"] == "merge":
            label = safe_name(operation["prefix"]) or "part_" + str(index)
            subnet = annotate(parent.createNode("subnet", label), "Architectural module: " + (prefix+operation["prefix"]))
            output = compile_recipes(subnet, operation["source"], prefix+operation["prefix"])
            output.setDisplayFlag(True)
            output.setRenderFlag(True)
            subnet.layoutChildren()
            branches.append(subnet)
        elif operation["kind"] == "trim":
            current = compile_recipes(parent, operation["source"], prefix)
            for volume_index, volume in enumerate(operation["volumes"]):
                if overlaps(operation["bounds"], volume["bounds"]):
                    current = clip_volume(parent, current, volume, volume_index)
            branches.append(current)
    if not branches:
        return parent.createNode("null", "EMPTY")
    return merge_nodes(parent, "MERGE_MODULES", branches)


def update_boundary_alignment(node, operations):
    planes = []
    seen = set()
    def collect(items):
        for item in items:
            for volume in item.get("volumes", []):
                for plane in volume["planes"]:
                    key = tuple(round(float(v), 12) for v in plane)
                    if key not in seen:
                        seen.add(key)
                        planes.append(plane)
            if "source" in item:
                collect(item["source"])
    collect(operations)
    code = "vector4 planes[];\n"
    for index, plane in enumerate(planes):
        values = ",".join(format(float(v), ".17g") for v in plane)
        code += "vector4 p%d=set(%s); append(planes,p%d);\n" % (index,values,index)
    snap = BOUNDARY_SNAP_CODE.replace(
        "plane.w/magnitude", 'plane.w*ch("../global_scale")/magnitude')
    snap = snap.replace(">1e-5", '>1e-5*abs(ch("../global_scale"))')
    node.node("final_boundary_alignment").parm("snippet").set(code+snap)


def finish_network(node, construction):
    manual = annotate(node.createNode("edit", "MANUAL_EDIT"),
                      "Edits here survive rebuilding construction.\n"
                      "After a topology change, check point selections before reusing manual edits.", (.65, .5, .25))
    manual.setInput(0, construction)
    clean = node.createNode("clean", "remove_duplicate_faces")
    clean.setInput(0, manual)
    clean.parm("fusepts").set(1)
    clean.parm("fusedist").set(1e-5)
    clean.parm("fixoverlap").set(0)
    clean.parm("deleteoverlap").set(0)
    clean.parm("orientpoly").set(0)
    clean.parm("deldegengeo").set(1)
    current = wrangle(node, "remove_exact_duplicate_surfaces", clean, """
// A face is identical even when its point order or normal is reversed.
string face_keys[];
int count = nprimitives(0);
for (int prim=0; prim<count; ++prim) {
    int points[] = primpoints(0,prim);
    int corners[];
    for(int i=0; i<len(points); ++i) {
        vector p=point(0,"P",points[i]);
        vector a=p-point(0,"P",points[(i+len(points)-1)%len(points)]);
        vector b=point(0,"P",points[(i+1)%len(points)])-p;
        float product=dot(a,a)*dot(b,b);
        // Clip may retain extra collinear vertices on the same physical face.
        if(product<1e-24) continue;
        if(dot(cross(a,b),cross(a,b))<1e-12*product && dot(a,b)>0) continue;
        append(corners,points[i]);
    }
    corners=sort(corners);
    string key = "";
    foreach(int pt; corners) key += sprintf("%d,",pt);
    append(face_keys,key);
}
int order[] = argsort(face_keys);
for(int index=1; index<len(order); ++index) {
    if(face_keys[order[index]] == face_keys[order[index-1]])
        removeprim(0,order[index],1);
}
""", owner="detail")
    palettes = {"stone": (.66, .60, .48), "trim": (.7656, .696, .5568),
                "roof": (.16, .22, .28), "glass": (.055, .12, .15),
                "wood": (.21, .11, .055), "metal": (.65, .43, .12), "gold": (.85, .56, .13),"floor":(.45,.40,.33),"interior":(.66,.60,.48)}
    for material, values in palettes.items():
        color = annotate(node.createNode("color", "color_" + material),
                         "Native primitive color for material_id=" + material, (.4, .45, .65))
        color.setInput(0, current)
        color.parm("group").set("@material_id=" + material)
        color.parm("class").set(1)
        color.parmTuple("color").set(values)
        if material in ("stone", "trim"):
            factor = 1.16 if material == "trim" else 1.
            for channel, (lime, sand, basalt) in enumerate(zip((.66,.60,.48),(.63,.47,.30),(.14,.17,.19))):
                expression = 'if(strcmp(chs("../stone_palette"),"limestone")==0,%s,if(strcmp(chs("../stone_palette"),"sandstone")==0,%s,%s))' % (
                    lime*factor, sand*factor, basalt*factor)
                color.parmTuple("color")[channel].setExpression(expression)
        elif material == "roof":
            for channel, (slate, copper, red) in enumerate(zip((.16,.22,.28),(.17,.39,.35),(.37,.12,.10))):
                color.parmTuple("color")[channel].setExpression(
                    'if(strcmp(chs("../roof_material"),"slate")==0,%s,if(strcmp(chs("../roof_material"),"copper")==0,%s,%s))' %
                    (slate,copper,red))
        current = color
    current = wrangle(node, "stone_variation_and_gold", current, """
@Cd = clamp(@Cd + f@jitter, 0.02, 0.98);
if (chi("../gold_accents") && (s@material_id=="trim" || s@material_id=="metal")) {
    @Cd=set(0.82,0.54,0.15); s@material_id="gold";
}
""")
    current = wrangle(node, "planar_uvs", current, """
vector p = point(0,"P",@ptnum);
vector n = abs(prim_normal(0,@primnum,0.5,0.5));
if(n.x>=n.y && n.x>=n.z) v@uv=set(p.y,p.z,0);
else if(n.y>=n.z) v@uv=set(p.x,p.z,0);
else v@uv=set(p.x,p.y,0);
string material=prim(0,"material_id",@primnum);
if(material=="floor") v@uv=set(p.x,p.z,0);
if(material=="wood") {
    int pts[]=primpoints(0,@primnum);
    vector along=set(0,1,0);float longest=0;
    for(int i=0;i<len(pts);i++) {
        vector edge=point(0,"P",pts[(i+1)%len(pts)])-point(0,"P",pts[i]);
        if(dot(edge,edge)>longest) {longest=dot(edge,edge);along=normalize(edge);}
    }
    int dominant=abs(along.x)>abs(along.y) ? (abs(along.x)>abs(along.z)?0:2) : (abs(along.y)>abs(along.z)?1:2);
    if(along[dominant]<0) along=-along;
    vector across=normalize(cross(prim_normal(0,@primnum,.5,.5),along));
    float low=1e20,high=-1e20;
    foreach(int pt;pts) {vector position=point(0,"P",pt);float t=dot(position,along);low=min(low,t);high=max(high,t);}
    // Walnut's grain runs in V. Stay inside one plank row so its baked board
    // seams cannot cut a single structural timber into apparent short blocks.
    v@uv=set(dot(p,across),.02+.12*clamp((dot(p,along)-low)/max(1e-7,high-low),0.,1.),0);
}
""", owner="vertex")
    normals = annotate(node.createNode("normal", "outward_vertex_normals"), "Native vertex normals after assembly.")
    normals.setInput(0, current)
    normals.parm("type").set("typevertex")
    normals.parm("cuspangle").set(48)
    set_menu(normals,'method','area')
    normals.parm("normalize").set(1)
    scale = annotate(node.createNode("xform", "global_scale"), "Live native uniform scale; rebuilding profiles is unnecessary.")
    scale.setInput(0, normals)
    scale.parm("scale").setExpression('ch("../global_scale")')
    precision = node.createNode("attribcast", "boundary_precision")
    precision.setInput(0, scale)
    set_menu(precision,"preferredprecision","64")
    precision.parm("numcasts").set(1)
    set_menu(precision,"class1","point")
    precision.parm("attribs1").set("P")
    set_menu(precision,"precision1","fpreal64")
    alignment = annotate(wrangle(node,"final_boundary_alignment",precision,"",owner="point"),
        "Restore exact architectural boundary positions after welding and scale.\n"
        "Only numerical deviations within 10 micrometres are aligned.",(.5,.45,.3))
    triangulate=node.createNode('divide','game_triangulation');triangulate.setInput(0,alignment)
    triangulate.parm('convex').set(1);triangulate.parm('usemaxsides').set(1);triangulate.parm('numsides').set(3)
    cleanup=wrangle(node,'remove_zero_area_export_triangles',triangulate,
        'if(primintrinsic(0,"measuredarea",@primnum)<1e-12) removeprim(0,@primnum,1);')
    output = node.createNode("null", "OUT_GOTHIC_BUILDING")
    output.setInput(0, cleanup)
    output.setDisplayFlag(True)
    output.setRenderFlag(True)
    node.layoutChildren()


def rebuild(node):
    global P_HIGH,P_LIBRARY,P_CACHE
    if 'p_panel' not in globals():exec(compile(PRODUCTION_SOURCE,'embedded_approved_modules','exec'),globals())
    P_HIGH=node.evalParm('mesh_quality')=='high' if node.parm('mesh_quality') else False
    P_CACHE={}
    if node.path() in _BUSY:
        return
    try:
        config = config_from_node(node)
        operations = recipes(config)
    except (ValueError, KeyError) as error:
        node.parm("rebuild_status").set("Invalid configuration: " + str(error))
        raise hou.NodeError(str(error)) from error
    _BUSY.add(node.path())
    previous_mode = hou.updateModeSetting()
    hou.setUpdateMode(hou.updateMode.Manual)
    staging = None
    construction = node.node("construction")
    old = None
    try:
        if construction is None:
            construction = annotate(node.createNode("subnet", "construction"),
                "Generated native SOP assembly. Disable Auto Rebuild before manual edits.")
            construction.setUserData("owner", "gothic_native_network")
        # Staging stays inside the editable subnet even in a locked HDA.
        staging = construction.createNode("subnet", "generated_new")
        P_LIBRARY=p_group(staging,'APPROVED_MODULE_LIBRARY')
        old = construction.node("generated")
        consumers = [(c.outputNode(),c.inputIndex()) for c in old.outputConnections()] if old else []
        display = old.isDisplayFlagSet() if old else True
        render = old.isRenderFlagSet() if old else True
        output = compile_recipes(staging, operations)
        output.setDisplayFlag(True)
        output.setRenderFlag(True)
        staging.layoutChildren()
        hou.setUpdateMode(hou.updateMode.AutoUpdate)
        staging.cook(force=True)
        hou.setUpdateMode(hou.updateMode.Manual)
        if staging.errors():
            raise hou.NodeError(str(staging.errors()))
        if old is not None:
            old.destroy()
        staging.setName("generated")
        for consumer,index in consumers:
            consumer.setInput(index,staging)
        staging.setDisplayFlag(display)
        staging.setRenderFlag(render)
        construction.layoutChildren()
        if node.node("MANUAL_EDIT") is None:
            finish_network(node, construction)
        update_boundary_alignment(node,operations)
        node.setUserData("native_config", json.dumps(config, sort_keys=True))
        node.setUserData("native_graph_version", "3.0")
        node.parm("rebuild_status").set("Native SOP network is up to date")
    except Exception:
        if staging is not None and staging.name() != "generated":
            staging.destroy()
        if old is not None:
            old.setDisplayFlag(True)
            old.setRenderFlag(True)
        node.parm("rebuild_status").set("Rebuild failed; previous generated modules retained")
        raise
    finally:
        _BUSY.discard(node.path())
        hou.setUpdateMode(previous_mode)


def on_parameter_changed(kwargs):
    node = kwargs["node"]
    if node.evalParm("auto_rebuild"):
        rebuild(node)
    else:
        node.parm("rebuild_status").set("Parameters changed: press Rebuild Generated Modules")


def apply_preset(kwargs):
    node = kwargs["node"]
    config = PRESETS[node.evalParm("building_preset")]
    for name, value in config["parameters"].items():
        node.parm(name).set(int(value) if isinstance(value, bool) else value)
    for name, value in config["seeds"].items():
        node.parm("seed_" + name).set(value)
    rebuild(node)


def interface(config):
    b = core()
    group = hou.ParmTemplateGroup()
    group.append(hou.StringParmTemplate("building_preset", "Building Preset", 1,
        default_value=("baseline",), menu_items=list(PRESETS), menu_labels=[v.get("label", k) for k,v in PRESETS.items()],
        script_callback="hou.phm().apply_preset(kwargs)", script_callback_language=hou.scriptLanguage.Python))
    group.append(hou.ToggleParmTemplate("auto_rebuild", "Auto Rebuild Generated Modules", default_value=True,
        help="Disable before editing generated SOP nodes. Shape parameter changes then require the Rebuild button."))
    group.append(hou.ButtonParmTemplate("rebuild_native", "Rebuild Generated Modules",
        script_callback="hou.phm().rebuild(kwargs['node'])", script_callback_language=hou.scriptLanguage.Python))
    group.append(hou.StringParmTemplate("rebuild_status", "Network Status", 1,
                                      default_value=("Native SOP network is up to date",)))
    group.append(hou.StringParmTemplate('mesh_quality','Mesh Quality',1,default_value=('game',),
        menu_items=['game','high'],menu_labels=['Game / Nanite','High / same design'],
        script_callback='hou.phm().on_parameter_changed(kwargs)',script_callback_language=hou.scriptLanguage.Python))
    geometry, finish = [], []
    for name, schema in b["SCHEMA"].items():
        value = config["parameters"][name]
        label = name.replace("_", " ").title()
        if schema["type"] == "string":
            template = hou.StringParmTemplate(name, label, 1, default_value=(value,),
                menu_items=schema["enum"], menu_labels=[v.replace("_"," ").title() for v in schema["enum"]])
        elif schema["type"] == "boolean":
            template = hou.ToggleParmTemplate(name, label, default_value=value)
        else:
            cls = hou.IntParmTemplate if schema["type"] == "integer" else hou.FloatParmTemplate
            template = cls(name,label,1,default_value=(value,),min=schema["min"],max=schema["max"],
                           min_is_strict=True,max_is_strict=True)
        if name in FINISH_PARAMETERS:
            finish.append(template)
        else:
            template.setScriptCallback("hou.phm().on_parameter_changed(kwargs)")
            template.setScriptCallbackLanguage(hou.scriptLanguage.Python)
            geometry.append(template)
    for name,value in config["seeds"].items():
        geometry.append(hou.IntParmTemplate("seed_"+name, "Seed "+name, 1, default_value=(value,),
            script_callback="hou.phm().on_parameter_changed(kwargs)", script_callback_language=hou.scriptLanguage.Python))
    group.append(hou.FolderParmTemplate("shape_controls", "Shape", parm_templates=geometry))
    group.append(hou.FolderParmTemplate("finish_controls", "Finish", parm_templates=finish))
    return group
