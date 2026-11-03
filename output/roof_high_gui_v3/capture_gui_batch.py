"""Drive the running Houdini UI by file queue; capture only its actual window."""
from pathlib import Path
import argparse,json,time,hashlib
ROOT=Path(__file__).resolve().parent

def job(ident,data):
    target=ROOT/'gui_results'/(ident+'.json')
    if target.exists():target.unlink()
    cmd=ROOT/'gui_commands'/(ident+'.json')
    cmd.write_text(json.dumps(dict(data,id=ident)),encoding='utf-8')
    deadline=time.monotonic()+150
    while not target.exists():
        if time.monotonic()>deadline:raise RuntimeError('GUI timed out: '+ident)
        time.sleep(.3)
    result=json.loads(target.read_text(encoding='utf-8'))
    if result['status']!='pass':raise RuntimeError(result)
    return result['result']

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--start',type=int,default=100);parser.add_argument('--offset',type=int,default=0);parser.add_argument('--mode',choices=['all','extra'],default='all');a=parser.parse_args()
    specs=[]
    if a.mode=='all':
        specs=[{'dataset':p.stem,'quality':'high','view':'hero','tag':p.stem+'_high_hero'} for p in sorted((ROOT/'datasets').glob('*.json'))]
    else:
        for ident in ('06_tavern','07_weapon_guild','08_normal_house_b','11_artisan_house'):
            specs += [{'dataset':ident,'quality':'high','view':'rear','tag':ident+'_high_rear'},
                      {'dataset':ident,'quality':'high','view':'hero','target':[1.,6.5,0.],'width':14.,'tag':ident+'_high_roof'},
                      {'dataset':ident,'quality':'low','view':'hero','target':[1.,6.5,0.],'width':14.,'tag':ident+'_low_roof'},
                      {'dataset':ident,'quality':'low','label':'BEFORE V2 LOW','scene':'../modular_rebuild_v2/scenes/variants/'+ident+'.hip','asset_path':'/obj/medieval_modular_v2/BUILDING','view':'hero','target':[1.,6.5,0.],'width':14.,'tag':ident+'_before_roof'}]
        specs += [{'dataset':'10_normal_house_c_canopy','quality':'high','target':[0.,2.,-3.5],'width':10.,'view':'hero','tag':'10_canopy_contact_high'},
                  {'dataset':'06_tavern','quality':'high','target':[0.,6.2,-2.],'width':7.,'wire':True,'tag':'06_dormer_high_wire'},
                  {'dataset':'06_tavern','quality':'low','target':[0.,6.2,-2.],'width':7.,'wire':True,'tag':'06_dormer_low_wire'},
                  {'dataset':'06_tavern','quality':'high','network':True,'tag':'06_high_network'}]
        specs += [{'dataset':'10_normal_house_c_canopy','quality':'high','target':[0.,2.3,-4.8],'delta':[10.,2.,-21.],'width':10.5,'view':'hero','tag':'10_canopy_under_balcony_high'},
                  {'dataset':'08_normal_house_b','quality':'high','target':[0.,2.4,-4.],'width':5.5,'view':'hero','tag':'08_pitched_canopy_contact_high'},
                  {'dataset':'06_tavern','quality':'high','network':True,'network_full':True,'tag':'06_all_stages_network'},
                  {'dataset':'06_tavern','quality':'high','network':'detail_levels','network_full':True,'tag':'06_high_bevel_network'}]
    result=json.loads((ROOT/'results'/('gui_capture_'+a.mode+'.json')).read_text(encoding='utf-8'))[:a.offset] if a.offset else []
    for i,spec in enumerate(specs[a.offset:]):
        prefix=str(a.start+i).zfill(3);review=job(prefix+'_review',dict(spec,script='gui_review.py'))
        if spec.get('network_full'):
            time.sleep(1.0);job(prefix+'_frame',{'script':'gui_frame_network.py'})
        time.sleep(2.5) # Let the actual UI redraw before a native window capture.
        output='screenshots/final/'+spec['tag']+'_gui.png'
        shot=job(prefix+'_capture',{'action':'screenshot','output':output,'crop_viewport':not spec.get('network_full')})
        if spec.get('network_full'):
            old_crop=ROOT/output.replace('_gui.png','_gui_viewport.png')
            if old_crop.exists():old_crop.unlink()
        path=ROOT/output
        result.append({'tag':spec['tag'],'spec':spec,'review':review,'capture':shot,'image':output,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
        (ROOT/'results'/('gui_capture_'+a.mode+'.json')).write_text(json.dumps(result,indent=2),encoding='utf-8')
        print(spec['tag'],review['primitives'],shot['provenance'],flush=True)

if __name__=='__main__':main()
