"""Create 28 reproducible, role-constrained Gothic building datasets."""
from pathlib import Path
import copy
import json
import random
import build_hda as B

ROOT=Path(__file__).parent
SEED=280319
# id, label, district, program, plan, floors, width, depth, front bays, roof, detail
SPECS=[
 (11,'Laborer Cottage','working','dwelling','compact',1,5.0,5.2,2,'gable',.22),
 (12,'Narrow Row House','working','dwelling','deep',2,4.6,5.6,2,'gable',.28),
 (13,'Artisan Workshop Home','working','shop','compact',2,6.2,6.8,3,'gable',.32),
 (14,'Weaver Terrace','working','dwelling','deep',2,5.2,6.8,2,'stepped',.30),
 (15,'Corner Smithy','working','shop','corner',2,7.6,9.6,3,'gable',.32),
 (16,'Neighborhood Tavern','working','inn','compact',2,8.4,8.2,3,'gable',.38),
 (17,'Merchant Townhouse','middle','dwelling','deep',3,7.8,8.4,3,'stepped',.52),
 (18,'Apothecary House','middle','shop','compact',2,7.6,8.2,3,'gable',.48),
 (19,'Baker House','middle','shop','corner',2,8.8,10.2,3,'stepped',.46),
 (20,'Scholar Residence','middle','dwelling','compact',3,8.2,8.4,3,'hip',.54),
 (21,'Merchant Courtyard','middle','dwelling','courtyard',3,12.8,14.8,4,'gable',.58),
 (22,'Noble Courtyard Manor','upper','manor','courtyard',3,16.0,17.0,5,'stepped',.72),
 (23,'Corner Patrician House','upper','manor','corner',4,13.2,15.2,4,'hip',.76),
 (24,'Masons Guildhall','public','guildhall','deep',2,14.0,10.0,5,'stepped',.68),
 (25,'Covered Market Hall','public','market','compact',1,17.0,11.0,6,'gable',.46),
 (26,'Gothic Town Hall','public','townhall','corner',3,16.0,16.0,5,'stepped',.74),
 (27,'Civic Library','public','library','courtyard',3,15.2,16.0,5,'hip',.68),
 (28,'Public Bath House','public','bathhouse','corner',2,13.8,14.0,4,'hip',.60),
]

def main():
    manifest=json.loads((ROOT/'manifest.json').read_text(encoding='utf-8'))
    records=[]
    for filename in manifest['datasets'][:10]:
        r=json.loads((ROOT/filename).read_text(encoding='utf-8'))
        r['parameters']=B.normalize(r)['parameters']
        r['district']='landmark';r['use']='religious' if r['parameters']['building_type']=='church' else 'civic'
        (ROOT/filename).write_text(json.dumps(r,indent=2),encoding='utf-8');records.append(r)
    for ident,label,district,program,plan,floors,width,depth,bays,roof,detail in SPECS:
        rng=random.Random(SEED+ident*1009)
        p=copy.deepcopy(B.BASELINE['parameters'])
        p.update(building_type='public' if district=='public' else 'residence',plan_layout='hall',
            tower_layout='none',spire_height=0.,nave_width=round(width+rng.uniform(-.16,.16),2),
            wall_height=max(3.,round(floors*(3.45 if district in ('public','upper') else 2.95),2)),
            floor_height=3.65 if program=='market' else 3.45 if district in ('public','upper') else round(rng.uniform(2.85,3.1),2),
            town_floors=floors,urban_depth=round(depth+rng.uniform(-.20,.20),2),front_bays=bays,
            urban_program=program,urban_plan=plan,urban_roof=roof,detail=detail,
            roof_pitch=round(rng.uniform(43,53),1),dormer_count=0,porch_depth=1.4 if program in ('inn','manor','guildhall','townhall','library','bathhouse') else 0.,
            balcony_enable=district in ('middle','upper') and ident not in (18,19),chimney_count=2 if program in ('shop','manor','inn') else 1,
            stone_palette='sandstone' if district=='working' else 'limestone',roof_material='copper' if ident in (23,26,28) else 'oxblood' if ident in (16,19) else 'slate',
            gold_accents=False,clock_enable=program=='townhall',facade_style='lancet',window_pattern='single',ridge_detail='sparse')
        record={'schema_version':2,'variant_id':f'variant_{ident:02d}','label':label,'district':district,'use':program,
            'source_kind':'generated_constrained','comparison_kind':'city_catalog','reference_ids':['master','preset_01','preset_05'],
            'parameters':p,'seeds':{'layout':rng.randrange(1,2147483646),'decoration':rng.randrange(1,2147483646)},
            'assumptions':['Inferred residential/civic plan within the approved Gothic module language; not a copied architectural survey'],
            'outputs':{'geometry':f'geometry/variant_{ident:02d}.obj','result':f'results/variant_{ident:02d}.json'}}
        B.normalize(record)
        target=ROOT/'datasets'/f'variant_{ident:02d}.json';target.write_text(json.dumps(record,indent=2),encoding='utf-8')
        records.append(record)
    manifest['datasets']=[f'datasets/{r["variant_id"]}.json' for r in records]
    manifest['generator_version']='3.0';manifest['city_dataset_seed']=SEED
    (ROOT/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    baseline=json.loads((ROOT/'baseline.json').read_text(encoding='utf-8'));baseline['parameters']=B.normalize(baseline)['parameters']
    (ROOT/'baseline.json').write_text(json.dumps(baseline,indent=2),encoding='utf-8')
    (ROOT/'parameter_schema.json').write_text(json.dumps({'version':'3.0','parameters':B.SCHEMA},indent=2),encoding='utf-8')
    catalog={'version':'3.0','seed':SEED,'count':len(records),'types':[{'id':r['variant_id'],'label':r['label'],'district':r['district'],'use':r['use'],'parameters':r['parameters'],'seeds':r['seeds']} for r in records]}
    (ROOT/'city_catalog.json').write_text(json.dumps(catalog,indent=2),encoding='utf-8')
    print(json.dumps({'presets':len(records),'new':len(SPECS),'districts':sorted(set(r['district'] for r in records))}),flush=True)

if __name__=='__main__':main()
