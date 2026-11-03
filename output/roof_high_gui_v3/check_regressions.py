"""Executed HDA parameter propagation, invalid-input and relationship checks."""
import copy
import json
import hashlib
import numpy as np
import hou
import build_hda as B
from hda_builder.evidence import ROOT,write_json,digest

def state(node):
    g=node.geometry()
    return {'points':g.intrinsicValue('pointcount'),'primitives':g.intrinsicValue('primitivecount'),
            'positions':digest(g.pointFloatAttribValues('P')),
            'names':digest(list(g.primStringAttribValues('name'))) if g.findPrimAttrib('name') else None,
            'uv':digest(g.vertexFloatAttribValues('uv')) if g.findVertexAttrib('uv') else None,
            'color':digest(g.primFloatAttribValues('Cd')) if g.findPrimAttrib('Cd') else None,
            'bounds':list(g.boundingBox().sizevec())}

def opening_relations(node):
    points=node.node('SHELL/facade_layout/OUT_opening_points').geometry()
    holder=hou.node('/obj').createNode('geo','RELATION_QA',run_init_scripts=False)
    test=holder.createNode('attribwrangle','opening_ray_through_wall');test.parm('class').set(2)
    # Connections cannot cross networks; stash actual cooked inputs in QA-only Stash SOPs.
    opening_stash=holder.createNode('stash','actual_opening_points');opening_stash.parm('stash').set(points)
    walls_stash=holder.createNode('stash','actual_cut_walls');walls_stash.parm('stash').set(node.node('OPENINGS/wall_cuts').geometry())
    test.setInput(0,opening_stash);test.setInput(1,walls_stash)
    test.parm('snippet').set('vector hit,uvw; i@qa_wall_hit=intersect(1,@P+v@outward*.45,-v@outward*.9,hit,uvw)>=0;')
    g=test.geometry();rows=[]
    for p in g.points():
        rows.append({'opening_id':p.attribValue('opening_id'),'wall_id':p.attribValue('wall_id') if g.findPointAttrib('wall_id') else '',
                     'kind':p.attribValue('opening_kind'),'center':list(p.position()),'ray_blocked_by_wall':bool(p.attribValue('qa_wall_hit')),
                     'aperture_w':p.attribValue('aperture_w'),'aperture_h':p.attribValue('aperture_h')})
    holder.destroy()
    return rows

def main():
    cfg=json.loads((ROOT/'datasets/baseline.json').read_text(encoding='utf-8'))
    node=B.build(cfg);node.cook(force=True)
    baseline=state(node);roof_before=state(node.node('ROOF/surfaces_tiles'))
    wall_before=state(node.node('OPENINGS/wall_cuts'));window_before=state(node.node('OPENINGS/windows'));facade_before=state(node.node('DETAILS/facade/OUT_facade_details'))
    relations=opening_relations(node)
    candidates=[r for r in relations if r['kind']=='W' and r['opening_id'].startswith('main_1_0_')]
    target=candidates[0]['opening_id']
    moved=copy.deepcopy(cfg);moved['extensions']={'window_move_id':target,'window_offset_u_m':.08}
    B.apply_config(node,moved);node.cook(force=True)
    moved_state=state(node);roof_after=state(node.node('ROOF/surfaces_tiles'));moved_relations=opening_relations(node)
    responsive={'wall_cut_changed':wall_before['positions']!=state(node.node('OPENINGS/wall_cuts'))['positions'],
                'window_frame_changed':window_before['positions']!=state(node.node('OPENINGS/windows'))['positions'],
                'facade_dressing_changed':facade_before['positions']!=state(node.node('DETAILS/facade/OUT_facade_details'))['positions']}
    p0=next(r for r in relations if r['opening_id']==target);p1=next(r for r in moved_relations if r['opening_id']==target)
    distance=float(np.linalg.norm(np.asarray(p0['center'])-p1['center']))
    tests=[{'test':'move_window','target':target,'distance_m':distance,'same_host_wall':p0['wall_id']==p1['wall_id'],
            'roof_geometry_stable':roof_before==roof_after,'opening_and_frame_changed':baseline['positions']!=moved_state['positions'],
            **responsive,
            'status':'pass' if abs(distance-.08)<1e-5 and p0['wall_id']==p1['wall_id'] and roof_before==roof_after and all(responsive.values()) else 'fail'}]
    tests.append({'test':'real_opening_rays','count':len(relations),'blocked':[r['opening_id'] for r in relations if r['ray_blocked_by_wall']],
                  'status':'pass' if relations and not any(r['ray_blocked_by_wall'] for r in relations) else 'fail'})
    B.apply_config(node,cfg);reset=state(node)
    uv=copy.deepcopy(cfg);uv['extensions']={'uv_tile_scale':2.0}
    B.apply_config(node,uv);uv_state=state(node)
    tests.append({'test':'uv_tiling','before':reset,'after':uv_state,'status':'pass' if reset['positions']==uv_state['positions'] and reset['names']==uv_state['names'] and reset['uv']!=uv_state['uv'] else 'fail'})
    B.apply_config(node,cfg);repeat1=state(node);B.apply_config(node,copy.deepcopy(cfg));repeat2=state(node)
    tests.append({'test':'same_config_seed_determinism','hash_a':repeat1,'hash_b':repeat2,'status':'pass' if repeat1==repeat2==baseline else 'fail'})
    changed=copy.deepcopy(cfg);changed['parameters']['width_m']+=.6
    B.apply_config(node,changed);wide=state(node)
    tests.append({'test':'footprint_width_recook','before':baseline['bounds'],'after':wide['bounds'],'status':'pass' if baseline['positions']!=wide['positions'] and wide['bounds'][0]>baseline['bounds'][0] else 'fail'})
    wing=json.loads((ROOT/'datasets/11_artisan_house.json').read_text(encoding='utf-8'))
    B.apply_config(node,wing);wing0=state(node.node('ROOF/surfaces_tiles'));wing['parameters']['wing_width_ratio']+=.1
    B.apply_config(node,wing);wing1=state(node.node('ROOF/surfaces_tiles'))
    tests.append({'test':'wing_size_roof_recook','status':'pass' if wing0['positions']!=wing1['positions'] else 'fail','before':wing0,'after':wing1})
    invalids=[]
    bad=copy.deepcopy(cfg);bad['parameters']['window_width_m']=99.;invalids.append(('invalid_window',bad))
    bad=copy.deepcopy(cfg);bad['parameters']['footprint']='t_shape';invalids.append(('unsupported_t_footprint',bad))
    bad=copy.deepcopy(cfg);bad['parameters']['roof_shape']='conical';invalids.append(('invalid_conical_host',bad))
    bad=copy.deepcopy(cfg);bad['semantic_graph']={'roof_patches':[]};invalids.append(('missing_explicit_roof_joint',bad))
    bad=copy.deepcopy(cfg);bad['extensions']={'window_move_id':target,'window_offset_u_m':.95};invalids.append(('window_outside_wall',bad))
    for label,bad in invalids:
        snapshot={p.name():p.rawValue() for p in node.parms()}
        try:B.apply_config(node,bad);error=None
        except (ValueError,IndexError) as exc:error=str(exc)
        unchanged=snapshot=={p.name():p.rawValue() for p in node.parms()}
        tests.append({'test':label,'error':error,'no_parameter_mutation':unchanged,'status':'pass' if error and unchanged else 'fail'})
    B.apply_config(node,cfg)
    guard=hou.node('/obj').createNode('geo','ARTIST_OWNED_GUARD',run_init_scripts=False);guard.setUserData('artist','keep')
    first=node.path();rebuilt=B.build(cfg)
    tests.append({'test':'safe_rerun','status':'pass' if rebuilt.path()==first and guard.userData('artist')=='keep' and state(rebuilt)==baseline else 'fail'})
    for width,depth,floors,label in [(3.5,3.5,1,'min'),(14.,12.,3,'max')]:
        stress=copy.deepcopy(cfg);stress['parameters'].update(width_m=width,depth_m=depth,floor_count=floors,front_bays='D'+'/W'*(floors-1),right_bays='W',rear_bays='W',left_bays='W',balcony_layout='none')
        try:
            B.apply_config(node,stress);s=state(node)
            tests.append({'test':'range_'+label,'state':s,'status':'pass' if s['points']>0 else 'fail'})
        except Exception as exc:tests.append({'test':'range_'+label,'status':'fail','error':str(exc)})
    # Exposed legacy grammars are measured separately from the 15 source records.
    for footprint,roof,label in [('rectangular','hipped','hipped'),('round','conical','round_conical'),('hybrid','gable','hybrid_tower')]:
        stress=copy.deepcopy(cfg);stress['parameters'].update(footprint=footprint,roof_shape=roof)
        try:
            B.apply_config(node,stress);s=state(node)
            tests.append({'test':'legacy_grammar_'+label,'status':'pass' if s['points'] else 'fail','state':s})
        except Exception as exc:tests.append({'test':'legacy_grammar_'+label,'status':'fail','error':str(exc)})
    result={'houdini':hou.applicationVersionString(),'tests':tests,'baseline_opening_relations':relations,
            'moved_opening_relations':moved_relations,'status':'pass' if all(t['status']=='pass' for t in tests) else 'fail',
            'course_note':'Original plaster facades have no masonry/plank course generator; bay framing and window dressing consume opening metadata.',
            'roof_joint_note':'Original body-trim roof grammar retained; custom explicit roof graphs are rejected, not substituted.'}
    write_json(ROOT/'results/regressions.json',result)
    print('REGRESSIONS',result['status'],[(t['test'],t['status']) for t in tests],flush=True)
    return result['status']!='pass'

if __name__=='__main__':raise SystemExit(main())
