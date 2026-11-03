"""Validate complete original records before setting Houdini parameters."""
from pathlib import Path
import copy
import json
import math

ROOT=Path(__file__).resolve().parents[1]

def normalize(config=None):
    config=copy.deepcopy(config if config is not None else json.loads((ROOT/'datasets/baseline.json').read_text(encoding='utf-8')))
    schema=json.loads((ROOT/'parameter_schema.json').read_text(encoding='utf-8'))['parameters']
    p=config['parameters']
    if set(p)!=set(schema):raise ValueError({'missing':sorted(set(schema)-set(p)),'unknown':sorted(set(p)-set(schema))})
    types={'number':(int,float),'integer':(int,),'boolean':(bool,),'string':(str,)}
    for key,info in schema.items():
        value=p[key]
        if type(value) not in types[info['type']]:raise ValueError('Invalid type: '+key)
        if isinstance(value,(int,float)) and not isinstance(value,bool):
            if not math.isfinite(value):raise ValueError('Nonfinite '+key)
            if 'min' in info and value<info['min']:raise ValueError('Below min: '+key)
            if 'max' in info and value>info['max']:raise ValueError('Above max: '+key)
        if 'enum' in info and value not in info['enum']:raise ValueError('Unsupported '+key+': '+str(value))
    for side in ('front','right','rear','left'):
        key=side+'_bays';rows=p[key].split('/')
        if not all(rows) or any(set(r)-set('WD.') for r in rows) or len({len(r) for r in rows})!=1:raise ValueError('Invalid aligned pattern '+key)
        length=p['width_m'] if side in ('front','rear') else p['depth_m']
        bay=length/len(rows[0])
        for row in rows:
            if 'W' in row and p['window_width_m']+2*p['frame_width_m']>bay-.1:raise ValueError('Window outside host bay: '+side)
            if 'D' in row and p['door_width_m']+2*p['frame_width_m']>bay-.1:raise ValueError('Door outside host bay: '+side)
    if p['front_bays'].split('/')[0].count('D')!=1:raise ValueError('Require one front entrance')
    if not 3.5<=p['width_m']<=14 or not 3.5<=p['depth_m']<=12:raise ValueError('Unsupported footprint range')
    if not 1<=p['floor_count']<=3:raise ValueError('Unsupported floor_count')
    if p['balcony_layout']!='none' and (p['floor_count']<2 or not any('D' in r for r in p['front_bays'].split('/')[1:])):raise ValueError('Balcony requires upper access door')
    if p['roof_shape']=='conical' and p['footprint']!='round':raise ValueError('Conical roof requires round host')
    if p['layout_input_enable'] and not p['layout_path']:raise ValueError('Missing optional layout input path')
    if config.get('seeds',{}).keys()!={'decoration'} or type(config['seeds']['decoration']) is not int:raise ValueError('Require integer seeds.decoration')
    overrides=json.loads(p['facade_overrides'])
    if not isinstance(overrides,dict):raise ValueError('facade_overrides must be object')
    from .corrections import EXTENSIONS
    extra=config.get('extensions',{})
    if not isinstance(extra,dict) or set(extra)-set(EXTENSIONS):raise ValueError('Unknown extension')
    for key,value in extra.items():
        spec=EXTENSIONS[key]
        if 'enum' in spec and value not in spec['enum']:raise ValueError('Invalid extension enum '+key)
        if type(value) not in types[spec['type']]:raise ValueError('Invalid extension type '+key)
        if spec['type']=='number' and (not math.isfinite(value) or not spec['min']<=value<=spec['max']):raise ValueError('Invalid extension range '+key)
    offset=extra.get('window_offset_u_m',0.)
    if offset:
        name=extra.get('window_move_id','')
        tokens=name.split('_')
        if len(tokens)!=4 or tokens[0]!='main':raise ValueError('Window move requires original main-floor-side-bay ID')
        floor,side,bayid=map(int,tokens[1:])
        sides=('front','right','rear','left');patterns=p[sides[side]+'_bays'].split('/')
        pattern=patterns[min(floor,len(patterns)-1)]
        if bayid>=len(pattern) or pattern[bayid]!='W':raise ValueError('Window move target is not a window')
        length=p['width_m'] if side in (0,2) else p['depth_m'];bay=length/len(pattern)
        env=p['window_width_m']*(1.68 if p['shutters_enable'] else 1)+2*p['frame_width_m']
        effective_env=min(env,max(.1,bay-.42))
        if abs(offset)+effective_env*.5>bay*.5-.1:raise ValueError('Window move outside host bay')
    if 'semantic_graph' in config:
        # Existing automatic body-trim grammar remains unchanged. Custom semantic roof edits
        # require explicit junctions; unsupported topology is rejected before any mutation.
        graph=config['semantic_graph']
        if not isinstance(graph,dict) or 'roof_junctions' not in graph:raise ValueError('Missing explicit roof junctions in semantic_graph')
        raise ValueError('Custom semantic roof topology is not implemented by the original Medieval grammar')
    return config
