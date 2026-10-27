"""Deterministic Gothic city layout in metres, using actual exported bounds."""
from pathlib import Path
import collections
import json
import math
import random

ROOT=Path(__file__).resolve().parents[3]
SOURCE=ROOT/'TDKProps/SourceAssets/Gothic'
SEED=91728

def intersects(a,b,gap=0.):
    return a[0]<b[2]+gap and a[2]>b[0]-gap and a[1]<b[3]+gap and a[3]>b[1]-gap

def plan():
    records=json.loads((SOURCE/'mesh_manifest.json').read_text())['variants']
    data={r['id']:r for r in records};rng=random.Random(SEED)
    buildings=[];roads=[];parks=[];props=[]
    def road(name,a,b,width=8.):
        assert a[0]==b[0] or a[1]==b[1]
        rect=[min(a[0],b[0])-width/2 if a[0]==b[0] else min(a[0],b[0]),min(a[1],b[1])-width/2 if a[1]==b[1] else min(a[1],b[1]),max(a[0],b[0])+width/2 if a[0]==b[0] else max(a[0],b[0]),max(a[1],b[1])+width/2 if a[1]==b[1] else max(a[1],b[1])]
        roads.append({'name':name,'a':a,'b':b,'width':width,'rect':rect})
    for x in (-225,-150,-75,75,150,225):road('Street_NS_'+str(x),(x,-225),(x,225))
    for y in (-225,-150,-75,75,150,225):road('Street_EW_'+str(y),(-225,y),(225,y))
    road('South_Gate_Avenue',(0,-270),(0,-41),12)
    road('Cathedral_Approach',(0,41),(0,145),12)
    road('West_Gate_Avenue',(-270,0),(-41,0),12)
    road('East_Gate_Avenue',(41,0),(270,0),12)
    for s in (-1,1):
        road('Square_NS_'+str(s),(s*37,-37),(s*37,37),8)
        road('Square_EW_'+str(s),(-37,s*37),(37,s*37),8)
    def add(ident,x,y,yaw,district=None,role='housing',check=True):
        r=data[ident];sx,_,sy=r['size_m']
        if int(abs(yaw))%180==90:sx,sy=sy,sx
        rect=[x-sx/2,y-sy/2,x+sx/2,y+sy/2]
        if check:
            if any(intersects(rect,b['rect'],1.5) for b in buildings):return False
            if role!='gate' and any(intersects(rect,road['rect'],.7) for road in roads):return False
        item={'id':ident,'label':r['label'],'district':district or r['district'],'role':role,'location_m':[round(x,4),round(y,4),.18],'yaw':yaw,'rect':rect,'height_m':r['size_m'][1]}
        buildings.append(item);return True
    landmarks=[('variant_10',0,187.5,0),('variant_08',-187.5,187.5,0),('variant_06',187.5,187.5,0),('variant_04',-187.5,-187.5,180),('variant_02',187.5,-187.5,180),('variant_01',-112.5,112.5,0)]
    for ident,x,y,yaw in landmarks:assert add(ident,x,y,yaw,'cathedral_close' if y>150 else 'public','landmark'),ident
    assert add('variant_05',0,-235,0,'public','gate')
    assert add('variant_09',235,0,90,'public','gate')
    assert add('variant_07',-235,22,90,'public','watchtower')
    for ident,sign,y in [('variant_24',-1,20),('variant_25',-1,-20),('variant_26',1,20),('variant_27',1,-20)]:
        depth=data[ident]['size_m'][2]
        assert add(ident,sign*(43+depth/2),y,90 if sign<0 else -90,'public','civic'),ident
    assert add('variant_28',112.5,112.5,0,'public','civic')
    pools={'working':[r for r in records if r['district']=='working'],
           'middle':[r for r in records if r['district']=='middle']+[data['variant_03']],
           'upper':[r for r in records if r['district']=='upper']}
    used=collections.Counter(b['id'] for b in buildings)
    centers=(-187.5,-112.5,-37.5,37.5,112.5,187.5)
    for cy in centers:
        for cx in centers:
            # Central civic plots and the cathedral forecourt remain open and legible.
            if abs(cx)<75 and abs(cy)<75:continue
            if abs(cx)<75 and cy>150:continue
            block=[cx-31,cy-31,cx+31,cy+31]
            if any(intersects(block,b['rect'],2.) for b in buildings):continue
            district='upper' if cx>75 and cy>0 else 'working' if cx<-75 and cy<75 else 'middle'
            available=pools[district]
            placed=0
            for side in ('south','north','west','east'):
                cursor=-23.
                while cursor<23:
                    choices=sorted(available,key=lambda r:(used[r['id']],rng.random()))
                    fitted=False
                    for r in choices:
                        sx,_,depth=r['size_m'];pitch=sx+2.5
                        if cursor+sx>24:continue
                        along=cursor+sx/2
                        if side=='south':x,y,yaw=cx+along,cy-29.5+depth/2,0
                        elif side=='north':x,y,yaw=cx-along,cy+29.5-depth/2,180
                        elif side=='west':x,y,yaw=cx-29.5+depth/2,cy-along,-90
                        else:x,y,yaw=cx+29.5-depth/2,cy+along,90
                        if add(r['id'],x,y,yaw,district):
                            used[r['id']]+=1;placed+=1;cursor+=pitch;fitted=True;break
                    if not fitted:cursor+=4.
            if placed:
                center_rect=[cx-4,cy-4,cx+4,cy+4]
                if not any(intersects(center_rect,b['rect'],1) for b in buildings):
                    parks.append({'center_m':[cx,cy],'size_m':[8,8],'district':district})
    assert set(b['id'] for b in buildings)==set(data),'A building type is missing from the sample city'
    for i,b in enumerate(buildings):
        assert not any(intersects(b['rect'],other['rect'],.2) for other in buildings[i+1:]),b
    # Context props sit along sidewalks and in courtyards rather than the carriageways.
    for road in roads:
        a,b=road['a'],road['b'];length=math.dist(a,b);vertical=a[0]==b[0]
        for step in range(int(length//32)+1):
            f=(step+.5)/max(1,int(length//32)+1)
            x=a[0]+(b[0]-a[0])*f;y=a[1]+(b[1]-a[1])*f
            if vertical:x+=road['width']/2+1.
            else:y+=road['width']/2+1.
            box=[x-.5,y-.5,x+.5,y+.5]
            if not any(intersects(box,building['rect'],.5) for building in buildings) and not any(intersects(box,other['rect'],.5) for other in roads):
                props.append({'type':'Lamp','location_m':[x,y,.18],'yaw':0})
    props.append({'type':'Fountain','location_m':[0,0,.20],'yaw':0})
    for x,y,yaw in [(-11,-10,0),(11,-10,0),(-11,10,180),(11,10,180)]:props.append({'type':'Bench','location_m':[x,y,.20],'yaw':yaw})
    for x,y in [(-23,-23),(-23,23),(23,-23),(23,23)]:
        props.append({'type':'Planter','location_m':[x,y,.20],'yaw':0})
        props.append({'type':'Cypress','location_m':[x,y,.65],'yaw':rng.randrange(360)})
    for park in parks:
        x,y=park['center_m'];props.append({'type':'Cypress','location_m':[x,y,.20],'yaw':rng.randrange(360)})
        props.append({'type':'Bench','location_m':[x,y-3,.20],'yaw':0})
    # Walls are modular; gaps align with the south/east gates and west avenue.
    walls=[]
    for side in ('north','south','east','west'):
        for n in range(59):
            along=-232+n*8
            if side=='south' and abs(along)<data['variant_05']['size_m'][0]/2+5:continue
            if side=='east' and abs(along)<data['variant_09']['size_m'][0]/2+5:continue
            if side=='west' and abs(along)<9:continue
            x,y,yaw=(along,235,0) if side=='north' else (along,-235,0) if side=='south' else (235,along,90) if side=='east' else (-235,along,90)
            walls.append({'type':'Wall','location_m':[x,y,0],'yaw':yaw})
    report={'version':'3.0','seed':SEED,'units':'metres, UE Z-up','bounds_m':[-275,-275,275,275],
        'building_types':28,'building_instances':len(buildings),'district_counts':dict(collections.Counter(b['district'] for b in buildings)),
        'buildings':buildings,'roads':roads,'parks':parks,'props':props,'walls':walls,
        'square':{'center_m':[0,0],'size_m':[60,60]},'validation':{'building_overlaps':0,'ordinary_buildings_on_roads':0,'all_28_types_present':True}}
    (SOURCE/'city_layout.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print('CITY_LAYOUT',len(buildings),len(props),dict(collections.Counter(b['district'] for b in buildings)),flush=True)

if __name__=='__main__':plan()
