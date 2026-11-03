"""Create 15 full parameter records mapped to the supplied concept sheets."""
from pathlib import Path
import copy
import hashlib
import json
import shutil

ROOT=Path(__file__).resolve().parents[1]
REF=ROOT.parent/'img_refs/medievals'

def main():
    schema=json.loads((ROOT/'parameter_schema.json').read_text(encoding='utf-8'))
    defaults={k:v['default'] for k,v in schema['parameters'].items()}
    defaults.update(footprint='rectangular',width_m=8.2,depth_m=6.8,floor_count=2,floor_height_m=2.65,
        roof_axis='x',roof_shape='gable',roof_height_m=2.2,timber_pattern='curved_brace',
        facade_style='B',facade_module_density=.65,front_bays='WDW/W.W',right_bays='W.W/W.W',
        rear_bays='W.W/W.W',left_bays='W.W/W.W',facade_overrides='{}',attic_window_enable=False,
        window_shape='rectangular',window_width_m=.95,window_height_m=1.2,door_width_m=1.25,door_height_m=2.1,
        shutters_enable=False,upper_overhang_m=.14,chimney_count=1,awning_enable=False,entrance_canopy=False,
        dressing_enable=True,interior_enable=True,interior_furniture=True,interior_partitions=False,
        balcony_layout='none',workshop_enable=False,utility_type='none',cupola_enable=False,dormer_count=1,
        mesh_quality='normal',detail_level='high',ground_enable=True,ground_margin_m=.9,ground_slope_preset='flat',
        palette='storybook_red',eave_curve=.12,tile_size_m=.43)
    rows=[
      ('01_food_shop','Food shop / bakery','ConceptArt_04_FoodShop_V1.png','simple',dict(floor_count=1,width_m=8.,depth_m=6.8,roof_height_m=2.4,awning_enable=True,chimney_count=2,front_bays='WDW',right_bays='W.W',left_bays='W.W',rear_bays='W.W',facade_overrides='{"main.right.0.0":"planter"}')),
      ('02_general_store','General store','ConceptArt_04_GeneralStore_V1.png','simple',dict(floor_count=1,width_m=9.6,depth_m=7.2,roof_height_m=2.6,awning_enable=True,front_bays='WDW',right_bays='W.W',left_bays='W.W',rear_bays='W.W',palette='slate_blue')),
      ('03_herbal_store','Herbal store','ConceptArt_04_HerbalStore_V1.png','simple',dict(floor_count=1,width_m=8.,depth_m=6.8,roof_height_m=2.35,awning_enable=True,front_bays='WDW',right_bays='W.W',left_bays='W.W',rear_bays='W.W',window_shape='arched',palette='slate_blue',facade_overrides='{"main.front.0.0":"planter","main.front.0.2":"planter","main.right.0.0":"trellis"}')),
      ('04_merchant_house','Merchant house','ConceptArt_04_MerchantHouse_V1.png','medium',dict(width_m=8.4,depth_m=7.2,entrance_canopy=True,palette='slate_blue',front_bays='WDW/WWW',dormer_count=1)),
      ('05_mixed_shop','Mixed shop and residence','ConceptArt_04_MixedShop_V1.png','medium',dict(width_m=9.6,depth_m=8.4,awning_enable=True,front_bays='WDW/WWW',chimney_count=2,roof_height_m=2.4)),
      ('06_tavern','Tavern / inn','ConceptArt_04_Tarven_V1.png','complex',dict(footprint='l_shape',width_m=8.,depth_m=7.6,wing_width_ratio=.55,wing_depth_ratio=.62,wing_floor_count=2,wing_overlap_m=1.0,wing_offset_z_m=-.7,wing_roof_ratio=.8,roof_height_m=2.4,awning_enable=True,front_bays='WDW/WWW',balcony_layout='front',dormer_count=1,front_window_count=3)),
      ('07_weapon_guild','Weapon merchant guild','ConceptArt_04_WeaponGuild_V1.png','complex',dict(footprint='l_shape',width_m=8.,depth_m=8.8,wing_width_ratio=.50,wing_depth_ratio=.62,wing_floor_count=2,wing_overlap_m=.8,wing_offset_z_m=0.,wing_roof_ratio=.75,roof_height_m=2.65,facade_style='C',awning_enable=True,palette='slate_blue',window_shape='arched',front_bays='WDW/W.W',facade_overrides='{"main.front.1.1":"banner"}')),
      ('08_normal_house_b','Normal house B','ConceptArt_05_NormalHouse_02_V1.png','medium',dict(width_m=8.2,depth_m=6.8,entrance_canopy=True,front_bays='WDW/WWW',shutters_enable=True,roof_height_m=2.3)),
      ('09_normal_house_c','Normal house C / balcony','ConceptArt_05_NormalHouse_03_V1.png','medium',dict(width_m=8.8,depth_m=7.6,balcony_layout='front',front_bays='WDW/WDW',dormer_count=0,entrance_canopy=False,window_shape='arched',roof_height_m=2.55)),
      ('10_normal_house_c_canopy','Normal house C / canopy variation','ConceptArt_05_NormalHouse_V1.png','medium',dict(width_m=8.8,depth_m=7.6,balcony_layout='wrap',front_bays='WDW/WDW',dormer_count=0,awning_enable=True,awning_span_ratio=.86,window_shape='arched',roof_height_m=2.55)),
      ('11_artisan_house','Artisan house','ConceptArt_06_ArtisanHouse_v1.png','complex',dict(footprint='l_shape',width_m=6.8,depth_m=7.4,wing_width_ratio=.48,wing_depth_ratio=.58,wing_floor_count=1,wing_overlap_m=.7,wing_offset_z_m=.3,wing_roof_ratio=.64,roof_axis='z',roof_height_m=2.5,entrance_canopy=True,balcony_layout='front',front_bays='WDW/WDW',palette='slate_blue',dormer_count=0)),
      ('12_craft_guild_hall','Craft guild hall','ConceptArt_06_CraftGuildHall_v1.png','complex',dict(width_m=11.4,depth_m=9.6,roof_axis='z',roof_height_m=2.8,facade_style='C',front_bays='.D./W.W',door_width_m=2.5,door_height_m=2.35,window_shape='arched',palette='slate_blue',dormer_count=0,attic_window_enable=True,facade_overrides='{"main.front.1.1":"crest","main.front.0.0":"banner","main.front.0.2":"banner"}')),
      ('13_material_storage','Material storage','ConceptArt_06_MarterialStorageBuilding_v1.png','simple',dict(width_m=9.2,depth_m=6.8,floor_count=1,floor_height_m=3.2,roof_axis='z',roof_height_m=2.6,front_bays='.D.',right_bays='W.W',left_bays='W.W',rear_bays='.W.',door_width_m=2.5,door_height_m=2.65,attic_window_enable=True,dormer_count=0,chimney_count=0,palette='slate_blue',timber_pattern='cross_brace',facade_style='A',interior_style='workshop')),
      ('14_toolmaker_workshop','Toolmaker workshop','ConceptArt_06_ToolmakerWorkshop_v1.png','complex',dict(footprint='l_shape',width_m=6.8,depth_m=7.6,wing_width_ratio=.45,wing_depth_ratio=.65,wing_floor_count=1,wing_overlap_m=.65,wing_offset_z_m=-.5,wing_roof_ratio=.65,roof_axis='z',roof_height_m=2.35,awning_enable=True,awning_span_ratio=.60,dormer_count=0,palette='slate_blue',interior_style='workshop',front_bays='WDW/WWW',facade_overrides='{"main.left.1.1":"crest"}')),
      ('15_forgehouse','Forgehouse','ConceptArt_07_Forgehouse_V1.png','complex',dict(footprint='l_shape',width_m=9.2,depth_m=9.2,wing_width_ratio=.50,wing_depth_ratio=.62,wing_floor_count=1,wing_overlap_m=1.0,wing_offset_z_m=-1.5,wing_roof_ratio=.75,roof_axis='z',roof_height_m=3.1,eave_curve=.38,awning_enable=True,chimney_count=2,cupola_enable=True,front_bays='WDW/W.W',window_shape='arched',palette='slate_blue',facade_style='C',interior_style='workshop',facade_overrides='{"main.front.1.1":"banner"}')),
    ]
    out=ROOT/'datasets';out.mkdir(exist_ok=True)
    refs=ROOT/'references';refs.mkdir(exist_ok=True)
    manifest={'schema_version':1,'asset_name':'medieval_building','generator':'build_hda.py','parameter_schema':'parameter_schema.json',
              'baseline':'datasets/baseline.json','references':[],'datasets':[],
              'requested_variant_count':15,'target_houdini_version':'22.0.368',
              'camera':{'projection':'orthographic','framing':'common maximum bounds','orthowidth_m':25},
              'render_backend':'houdini_opengl','bake_backend':'houdini_copernicus_bakegeometrytextures::2.0',
              'normal_format':'DirectX tangent space, MikkT, 0..1, linear PNG',
              'reference_fidelity':'Parametric reconstructions with native guild turrets, furnace/flue and hoists; bespoke emblems, plants, unique props and fire simulation remain simplified or omitted.'}
    for i,(ident,label,filename,complexity,changes) in enumerate(rows):
        p=copy.deepcopy(defaults);p.update(changes)
        if ident=='12_craft_guild_hall':
            p.update(door_width_m=3.0,rear_bays='.W./.W.',facade_overrides=json.dumps({
                'main.front.0.0':'guild_pier','main.front.0.2':'guild_pier',
                'main.rear.0.0':'guild_pier','main.rear.0.2':'guild_pier','main.front.1.1':'crest'}))
        elif ident=='13_material_storage':
            p['facade_overrides']=json.dumps({'main.right.0.1':'hoist'})
        elif ident=='14_toolmaker_workshop':
            p.update(front_bays='.DW/WWW',facade_overrides=json.dumps({'main.front.0.0':'hoist_hook','main.left.1.1':'crest'}))
        elif ident=='15_forgehouse':
            p.update(front_bays='.DW/W.W',awning_enable=False,chimney_count=0,
                     facade_overrides=json.dumps({'main.front.0.0':'forge','main.front.1.1':'banner'}))
        # The balcony needs a real upper door. No balcony on a window-only wall.
        if p['balcony_layout']!='none' and 'D' not in p['front_bays'].split('/')[1]:
            parts=p['front_bays'].split('/');parts[1]='WDW';p['front_bays']='/'.join(parts)
        refid='ref_'+ident
        shutil.copy2(REF/filename,refs/filename)
        manifest['references'].append({'reference_id':refid,'role':'variant','path':'references/'+filename,'sha256':hashlib.sha256((REF/filename).read_bytes()).hexdigest()})
        rec={'schema_version':1,'variant_id':ident,'label':label,'complexity':complexity,'comparison_kind':'preset',
             'source_kind':'input','reference_ids':[refid],'parameters':p,'seeds':{'decoration':17},'module_dependencies':[],
             'expected_features':{'floor_count':p['floor_count'],'entrance_count':1,'roof_type':p['roof_shape'],'footprint':p['footprint'],
                                  'interior_enable':True,'normal_bake_from_high':True},
             'assumptions':['Dimensions are modeling targets, not surveyed measurements.','Hidden interiors follow the existing native floor/stair grammar.',
                            'Bespoke reference props, vegetation, signage and sculpted ornament are outside this base-color architectural bake.'],
             'outputs':{'geometry':'geometry/'+ident+'.bgeo.sc','hero':'previews/'+ident+'_hero.png','comparison':'comparisons/'+ident+'.png','result':'results/'+ident+'.json'}}
        if i in (8,9):rec['assumptions'].append('The two supplied NormalHouse C sheets have identical image bytes; canopy/balcony coverage distinguishes these two presets.')
        if i==11:rec['assumptions'].append('Course-built square corner turrets are native geometry; bespoke guild emblems remain simplified.')
        if i==14:rec['assumptions'].append('Native furnace mouth and hollow industrial flue are included; fire simulation and bespoke bell are omitted.')
        (out/(ident+'.json')).write_text(json.dumps(rec,indent=2),encoding='utf-8')
        manifest['datasets'].append('datasets/'+ident+'.json')
    baseline=copy.deepcopy(json.loads((out/'08_normal_house_b.json').read_text()))
    baseline.update(variant_id='baseline',label='Baseline native house',comparison_kind='baseline')
    baseline['reference_ids']=['ref_baseline']
    manifest['references'].insert(0,{'reference_id':'ref_baseline','role':'baseline','path':'references/ConceptArt_05_NormalHouse_02_V1.png'})
    for k in baseline['outputs']:baseline['outputs'][k]=baseline['outputs'][k].replace('08_normal_house_b','baseline')
    (out/'baseline.json').write_text(json.dumps(baseline,indent=2),encoding='utf-8')
    (ROOT/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    for group_index in range(3):
        group=copy.deepcopy(manifest);group['datasets']=manifest['datasets'][group_index*5:group_index*5+5]
        used={json.loads((ROOT/f).read_text())['reference_ids'][0] for f in group['datasets']}
        group['references']=[r for r in manifest['references'] if r['role']=='baseline' or r['reference_id'] in used]
        (ROOT/f'manifest_group_{group_index+1}.json').write_text(json.dumps(group,indent=2),encoding='utf-8')
    print('CREATED',len(rows),'full configs')

if __name__=='__main__':main()
