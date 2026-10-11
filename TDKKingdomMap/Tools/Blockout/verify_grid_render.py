"""Measure physical grid spacing in actual face-aligned Unreal BaseColor captures.
No image synthesis/editing, no third-party packages. A missing/incorrect render fails.
"""
import json, math, statistics, struct, zlib
from pathlib import Path

BASE=Path(__file__).resolve().parents[2]/'Design/Blockout/Generated'

def read_png(path):
    data=path.read_bytes(); offset=8; compressed=b''
    while offset<len(data):
        size=struct.unpack_from('>I',data,offset)[0]; kind=data[offset+4:offset+8]; payload=data[offset+8:offset+8+size]; offset+=size+12
        if kind==b'IHDR': w,h,depth,ctype,_,_,interlace=struct.unpack('>IIBBBBB',payload)
        elif kind==b'IDAT': compressed+=payload
        elif kind==b'IEND': break
    if depth!=8 or ctype not in (2,6) or interlace: raise ValueError('Expected non-interlaced RGB/RGBA8 capture')
    channels=4 if ctype==6 else 3; stride=w*channels; raw=zlib.decompress(compressed); rows=[]; previous=bytearray(stride)
    for y in range(h):
        start=y*(stride+1); filt=raw[start]; row=bytearray(raw[start+1:start+1+stride])
        for x in range(stride):
            a=row[x-channels] if x>=channels else 0; b=previous[x]; c=previous[x-channels] if x>=channels else 0
            if filt==1: row[x]=(row[x]+a)&255
            elif filt==2: row[x]=(row[x]+b)&255
            elif filt==3: row[x]=(row[x]+(a+b)//2)&255
            elif filt==4:
                p=a+b-c; distances=[abs(p-a),abs(p-b),abs(p-c)]; predictor=(a,b,c)[distances.index(min(distances))]; row[x]=(row[x]+predictor)&255
            elif filt!=0: raise ValueError('Invalid PNG filter')
        rows.append(row); previous=row
    def luma(x,y):
        i=x*channels; r,g,b=rows[y][i:i+3]; return .2126*r+.7152*g+.0722*b
    return w,h,luma

def line_centres(values,lo,threshold):
    baseline=sorted(values)[int(len(values)*.85)]; groups=[]; current=[]
    for i,value in enumerate(values):
        if value<baseline*threshold: current.append(i+lo)
        elif current: groups.append(current); current=[]
    if current: groups.append(current)
    return [statistics.mean(g) for g in groups if len(g)>=1]

def spacing_check(centres,expected_px,spacing_cm):
    distances=[b-a for a,b in zip(centres,centres[1:])]
    observed=statistics.median(distances) if distances else 0
    # Pixel quantisation and antialiasing are bounded to two pixels at these render scales.
    return dict(pass_=len(distances)>=2 and abs(observed-expected_px)<=2,
                measured_cm=observed/expected_px*spacing_cm,expected_cm=spacing_cm,
                expected_pixels=expected_px,measured_pixels=observed,lines=len(centres))

def main():
    checks={}
    for name,width,face_width in [('unit',3,1),('unequal',6,2),('rotated',6,2)]:
        w,h,pixel=read_png(BASE/f'editor_calibration_{name}.png'); ppm=w/(width*100)
        row=round(h/2-21.3*ppm); lo=math.ceil(w/2-face_width*50*ppm+8); hi=math.floor(w/2+face_width*50*ppm-8)
        centres=line_centres([pixel(x,row) for x in range(lo,hi)],lo,.94)
        checks[name+'_minor']=spacing_check(centres,10*ppm,10)
        if name!='unit':
            col=round(w/2-21.3*ppm); lo=math.ceil(h/2-150*ppm+15); hi=math.floor(h/2+150*ppm-15)
            centres=line_centres([pixel(col,y) for y in range(lo,hi)],lo,.45)
            checks[name+'_major']=spacing_check(centres,100*ppm,100)
    w,h,pixel=read_png(BASE/'editor_calibration_ramp.png'); ppm=w/600
    col=round(w/2-21.3*ppm); half_length_cm=math.sqrt(200**2+100**2)/2
    lo=round(h/2-half_length_cm*ppm+8); hi=round(h/2+half_length_cm*ppm-8)
    centres=line_centres([pixel(col,y) for y in range(lo,hi)],lo,.45)
    checks['ramp_intrinsic_major']=spacing_check(centres,100*ppm,100)
    w,h,pixel=read_png(BASE/'editor_calibration_roof_slope.png'); ppm=w/300
    col=round(w/2-21.3*ppm); half_length_cm=math.sqrt(100**2+100**2)/2
    lo=round(h/2-half_length_cm*ppm+8); hi=round(h/2+half_length_cm*ppm-8)
    centres=line_centres([pixel(col,y) for y in range(lo,hi)],lo,.94)
    checks['roof_intrinsic_minor']=spacing_check(centres,10*ppm,10)
    w,h,pixel=read_png(BASE/'editor_calibration_cylinder.png'); ppm=w/400; row=round(h/2-21.3*ppm)
    centres=line_centres([pixel(x,row) for x in range(round(w/2-90*ppm),round(w/2+90*ppm))],round(w/2-90*ppm),.72)
    expected=[w/2-100*math.cos(theta)*ppm for theta in (-1,-2)]
    errors=[min((abs(c-e) for c in centres),default=999) for e in expected]
    checks['cylinder_arc_major']=dict(pass_=all(e<=3 for e in errors),expected_line_pixels=expected,pixel_errors=errors,physical_arc_spacing_cm=100)
    for name in ('unit','unequal','rotated','composite','ramp','cylinder','roof_slope'):
        w,h,pixel=read_png(BASE/f'editor_calibration_{name}.png')
        bright=sum(pixel(x,y)>240 for y in range(25,115) for x in range(25,370))
        checks[name+'_label_visible']=dict(pass_=bright>=100,bright_text_pixels=bright)
    calibration=json.loads((BASE/'unreal_calibration_report.json').read_text())
    capture_ok=calibration['status']=='pass' and calibration.get('captures_saved',False)
    result=dict(status='pass' if capture_ok and all(c['pass_'] for c in checks.values()) else 'fail',
                fingerprint=calibration['fingerprint'],calibration_capture_status=calibration['status'],
                execution='Measured pixel positions in actual Unreal BaseColor renders; slope capture aligned to actual native ramp face',checks=checks)
    (BASE/'grid_render_validation.json').write_text(json.dumps(result,indent=2)+'\n'); print(json.dumps(result,indent=2))
    raise SystemExit(0 if result['status']=='pass' else 1)

if __name__=='__main__': main()
