"""One-time reference transcription. Normal reruns edit layout.json, not this file."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'Design/Blockout'
S = 200 / 240  # 0..200 m ruler in reference 01, estimated
def xy(points):
    return [[round((x-580)*S, 3), round((450-y)*S, 3)] for x,y in points]
def region(id, points, **kw):
    return dict(id=id, polygon_m=xy(points), **kw)

wallpx = [(580,150),(680,157),(790,221),(882,275),(962,377),(966,540),
          (933,660),(800,748),(687,843),(580,865),(389,805),(280,700),
          (227,660),(165,540),(173,377),(261,274),(349,226),(480,158)]
lower = [(350,226),(480,165),(680,165),(790,221),(748,300),(660,275),
         (580,295),(505,275),(430,303)]
upper = [(190,552),(290,571),(438,554),(515,538),(650,538),(706,562),
         (740,590),(804,642),(794,737),(687,830),(580,850),(389,795),
         (280,690),(227,650)]
waterfront = [(804,642),(850,658),(921,672),(968,719),(976,765),(1003,787),
              (930,822),(850,869),(809,862),(795,792),(794,737)]
land = [(580,20),(725,32),(861,66),(990,125),(1007,240),(993,399),
        (993,610),(982,715),(1003,787),(930,822),(850,869),(809,862),
        (751,939),(612,981),(420,920),(250,804),(160,680),(112,479),
        (154,280),(280,155),(431,60)]
d = {
 'schema':'thanh-a-layout', 'version':1, 'units':'m', 'seed':17429, 'estimated':True,
 'coordinate_system':{'x':'East','y':'North','z':'Up','origin':'Central plaza XY; water datum Z=0'},
 'scale':1.0,
 'reference':{'primary':'References/01_terrain_layout.png.png',
    'massing':'References/02_city_massing.png.png',
    'silhouette':['References/03_gothic_reference.png.png','References/04_medieval_reference.png.png'],
    'crop_px':[105,0,1033,1000], 'source_size_px':[1448,1086],
    'origin_px':[580,450], 'meters_per_pixel':S,
    'image_to_plan_affine':[[S,0,-580*S],[0,-S,450*S]],
    'scale_ruler_px':[[25,1017],[265,1017]], 'scale_ruler_m':200,
    'estimated':True, 'perspective_inset_used_for_measurement':False},
 'water_datum_m':0, 'water_is_assumption':True,
 'pawn':{'class':'/Script/TDKKingdomMap.ThanhACharacter','radius_m':.42,'half_height_m':.96,
    'max_step_m':.35,'walkable_angle_deg':35,'design_ramp_angle_deg':12,'speed_m_s':6},
 'land':[region('island',land,base_elevation_m=8)],
 'water':[region('west_sea',[(0,275),(145,275),(110,500),(160,695),(365,915),(580,985),(580,1050),(0,1050)]),
          region('east_sea',[(1008,225),(1032,225),(1032,1050),(580,1050),(580,985),(751,939),(809,862),(976,765),(993,610)])],
 'city_boundary':region('city_inside',wallpx),
 'architecture_regions':[
    region('arch_lower',lower,architecture_style='Medieval',reference_color='blue'),
    region('arch_middle',[(190,552),(175,378),(270,280),(350,226),(430,303),(505,275),(580,295),(660,275),(748,300),(790,221),(872,280),(950,377),(949,539),(921,649),(804,642),(740,590),(706,562),(650,538),(515,538),(438,554),(290,571)],architecture_style='Hybrid',reference_color='green'),
    region('arch_upper',upper,architecture_style='Gothic',reference_color='red'),
    region('arch_port',waterfront,architecture_style='Medieval',reference_color='blue')],
 'elevation_zones':[
    region('middle_tier',wallpx,elevation_m=8,range_m=[6,10],priority=0),
    region('gate_district',lower,elevation_m=2,range_m=[0,4],priority=10),
    region('market',[(475,293),(684,293),(693,359),(642,390),(520,390),(452,351)],elevation_m=6,range_m=[4,8],priority=20),
    region('upper_tier',upper,elevation_m=18,range_m=[15,22],priority=30),
    region('forge_yard',[(744,474),(945,514),(921,649),(804,642),(740,590),(706,562)],elevation_m=12,range_m=[10,15],priority=35),
    region('waterfront',waterfront,elevation_m=2,range_m=[2,2],priority=40),
    region('watchtower_ground',[(267,651),(320,651),(334,710),(275,735),(245,685)],elevation_m=22,range_m=[22,22],priority=50),
    region('gate_village',[(480,5),(695,5),(730,125),(480,125)],elevation_m=5,range_m=[3,8],priority=5),
    region('mill_hamlet',[(240,100),(430,100),(420,195),(290,225)],elevation_m=7,range_m=[5,10],priority=5),
    region('quarry_hamlet',[(815,40),(985,65),(1000,190),(860,192)],elevation_m=9,range_m=[5,12],priority=5)],
 'plazas':[dict(id='central_plaza',center_m=[0,0],radius_m=48,elevation_m=8),
           dict(id='council_forecourt',center_m=xy([(580,653)])[0],radius_m=22,elevation_m=18)],
 'roads':[], 'retaining_walls':[], 'stairs':[],
 'walls':{'id':'city_wall','points_m':xy(wallpx),'closed':True,'height_m':10,'thickness_m':3,
     'tower_radius_m':8,'tower_height_m':17,
     'openings':[
        dict(id='main_gate',center_m=xy([(580,150)])[0],width_m=18,ground_m=0,clear_height_m=8),
        dict(id='nw_postern',center_m=xy([(369,217)])[0],width_m=9,ground_m=2,clear_height_m=5),
        dict(id='west_postern',center_m=xy([(169,460)])[0],width_m=8,ground_m=8,clear_height_m=5),
        dict(id='east_postern',center_m=xy([(964,460)])[0],width_m=8,ground_m=8,clear_height_m=5),
        dict(id='south_postern',center_m=xy([(580,865)])[0],width_m=10,ground_m=18,clear_height_m=6)]},
 'landmarks':[
    dict(id='council_hall',position_m=xy([(580,726)])[0]+[18],size_m=[34,52,22],style='Gothic',roof_m=16,towers=True),
    dict(id='barracks',position_m=xy([(736,672)])[0]+[18],size_m=[32,20,12],style='Gothic',roof_m=9,towers=False),
    dict(id='watchtower',position_m=xy([(283,696)])[0]+[22],size_m=[12,12,28],style='Gothic',roof_m=14,towers=True),
    dict(id='forge',position_m=xy([(854,565)])[0]+[12],size_m=[24,24,10],style='Hybrid',roof_m=6,towers=False)],
 'no_build':[
    region('barracks_training_yard',[(699,701),(774,701),(774,740),(699,740)]),
    region('watchtower_overlook',[(285,715),(345,715),(365,754),(312,765)])],
 'building_styles':{
    'Medieval':{'spacing_m':28,'size_x_m':[7,11],'size_y_m':[10,15],'height_m':[5,8],'roof_m':[3,5],'tower_every':0,'density':.8},
    'Hybrid':{'spacing_m':26,'size_x_m':[9,13],'size_y_m':[12,18],'height_m':[7,12],'roof_m':[5,8],'tower_every':5,'density':.85},
    'Gothic':{'spacing_m':23,'size_x_m':[12,16],'size_y_m':[16,23],'height_m':[14,22],'roof_m':[9,13],'tower_every':3,'density':.96}},
 'clearance_m':2.5,
 'piers':[dict(id='pier_main',a_m=xy([(855,775)])[0],b_m=xy([(930,798)])[0],width_m=10,elevation_m=2),
          dict(id='pier_south',a_m=xy([(826,808)])[0],b_m=xy([(850,905)])[0],width_m=9,elevation_m=2),
          dict(id='pier_east',a_m=xy([(930,798)])[0],b_m=xy([(986,849)])[0],width_m=8,elevation_m=2)],
 'heightmap':{'resolution':1009,'min_xy_m':[-504,-504],'extent_m':[1008,1008],
    'encoded_min_m':-32,'encoded_max_m':32,'num_subsections':1,'subsection_quads':63},
 'samples':[],
 'routes':{'required':['main_axis','council_approach','upper_east','port_descent','port_access'],
           'branches':['residential','craft']},
 'capture_views':[
    dict(id='top_down',location_m=[0,0,950],target_m=[0,0,0],ortho_width_m=980),
    dict(id='south',location_m=[0,-690,350],target_m=[0,-70,18],fov=65),
    dict(id='gate_to_plaza',location_m=[0,245,33],target_m=[0,-120,18],fov=75),
    dict(id='port_descent',location_m=[380,-320,130],target_m=[180,-190,8],fov=70)]
}
def road(id, points, zs, width=10, kind='road'):
    d['roads'].append(dict(id=id,points_m=[p+[z] for p,z in zip(xy(points),zs)],width_m=width,kind=kind))
road('main_axis',[(580,85),(580,150),(580,195),(580,280),(580,360),(580,450),(580,525),(580,620),(580,653)], [5,0,2,4,8,8,8,18,18],18)
road('council_approach',[(580,653),(580,678)],[18,18],12)
road('residential',[(580,450),(455,481),(325,500),(220,460),(169,460),(133,460)],[8]*6,10)
road('craft',[(580,450),(711,450),(850,475),(964,460),(1000,460)],[8]*5,10)
road('west_middle_loop',[(325,500),(285,430),(320,332),(420,326),(480,365),(580,360)],[8,8,8,6,8,8],8)
road('east_middle_loop',[(711,450),(716,350),(680,330),(660,360),(580,360)],[8,8,6,8,8],8)
road('upper_west',[(580,620),(430,620),(350,657),(340,690),(300,695)],[18,18,18,22,22],10)
road('upper_east',[(580,653),(672,620),(770,645)],[18,18,18],12)
road('forge_approach',[(711,450),(775,525),(820,560)],[8,12,12],10)
road('forge_to_upper',[(775,525),(748,580),(770,645)],[12,15,18],10)
road('port_descent',[(770,645),(803,675),(804,717),(851,748),(895,720),(875,770)],[18,15,10,6,2,2],10,'switchback')
road('port_access',[(875,770),(855,775),(826,808)],[2,2,2],12)
road('south_exit',[(580,653),(637,765),(580,810),(580,865),(580,885)],[18]*5,10)
road('nw_exit',[(430,303),(407,253),(369,217),(348,197)],[8,4,2,2],9)
for id,pts in [('lower_west_edge',[(350,226),(395,280),(430,303),(505,275)]),
               ('lower_east_edge',[(660,275),(748,300),(790,221)]),
               ('upper_north_edge',[(190,552),(290,571),(438,554),(515,538),(650,538),(706,562),(740,590),(804,642)]),
               ('port_cliff',[(804,642),(794,737)])]:
    d['retaining_walls'].append(dict(id=id,points_m=xy(pts),thickness_m=1.2))
d['stairs']=[dict(id='plaza_west_stair',points_m=[p+[z] for p,z in zip(xy([(455,495),(475,560)]),[8,18])],width_m=5,step_rise_m=.25,landing_m=4),
             dict(id='plaza_east_stair',points_m=[p+[z] for p,z in zip(xy([(700,492),(675,556)]),[8,18])],width_m=5,step_rise_m=.25,landing_m=4)]
for id,px,z in [('main_gate',(580,150),0),('market',(580,360),8),('residential',(325,500),8),('craft',(711,450),8),('central_plaza',(580,450),8),('council_hall_ground',(580,653),18),('barracks_ground',(770,645),18),('watchtower_ground',(340,690),22),('waterfront',(875,770),2),('forge_yard',(775,525),12)]:
    d['samples'].append(dict(id='sample_'+id,position_m=xy([px])[0],ground_m=z))
OUT.mkdir(parents=True, exist_ok=True)
(OUT/'layout.json').write_text(json.dumps(d,indent=2)+'\n', encoding='utf-8')
print('Wrote editable layout.json')
