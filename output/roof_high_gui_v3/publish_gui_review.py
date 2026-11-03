"""Assemble review pages from real screenshots, preserving dataset bytes."""
from pathlib import Path
import collections,hashlib,html,json,shutil
from PIL import Image,ImageDraw,ImageFont
ROOT=Path(__file__).resolve().parent
def read(p):return json.loads((ROOT/p).read_text(encoding='utf-8'))
def write(p,data):
    target=ROOT/p;target.parent.mkdir(parents=True,exist_ok=True);target.write_text(json.dumps(data,indent=2,ensure_ascii=False),encoding='utf-8')
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    variants=read('results/variants.json');captures=read('results/gui_capture_all.json');extra=read('results/gui_capture_extra.json')
    graph=read('graph_audit.json');asset=ROOT/'asset/medieval_building_modular.hda'
    assert len(captures)==16 and len(extra)==24,'Incomplete capture batch'
    by_id={r['variant_id']:r for r in variants}
    for capture in captures+extra:
        assert capture['capture']['window_visible'] and not capture['capture']['window_minimized']
        assert sha(ROOT/capture['image'])==capture['sha256'],'Changed screenshot'
        if not capture['spec'].get('asset_path'):
            r=capture['review'];expected=by_id[r['variant_id']]['levels'][r['quality']]
            assert r['position_hash']==expected['position_hash'],'Stale GUI mesh'
            assert r['hda_sha256']==sha(asset),'Stale HDA in GUI'
    shots={row['review']['variant_id']:row for row in captures}
    refs={r['reference_id']:r['path'] for r in read('manifest.json')['references']}
    cards=[]
    font=ImageFont.truetype('C:/Windows/Fonts/arial.ttf',21)
    for row in variants:
        ident=row['variant_id'];cfg=read(row['config']);shot=shots[ident]
        assert shot['capture']['ui_available'] and shot['capture']['provenance']=='houdini_gui_window_screenshot'
        assert shot['review']['primitives']==row['levels']['high']['primitives'],ident+' stale GUI geometry'
        assert sha(ROOT/shot['image'])==shot['sha256'],ident+' changed screenshot'
        source=ROOT/shot['image'];crop=source.with_name(source.stem+'_viewport.png')
        preview=ROOT/cfg['outputs']['hero'];preview.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(crop,preview)
        geom=ROOT/cfg['outputs']['geometry'];geom.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(ROOT/row['levels']['high']['file'],geom)
        reference=ROOT/refs[cfg['reference_ids'][0]]
        comparison=Image.new('RGB',(1800,860),'#202327');draw=ImageDraw.Draw(comparison)
        for path,box in ((reference,(0,55,900,855)),(crop,(900,55,1800,855))):
            im=Image.open(path).convert('RGB');im.thumbnail((box[2]-box[0],box[3]-box[1]))
            comparison.paste(im,(box[0]+(box[2]-box[0]-im.width)//2,box[1]+(box[3]-box[1]-im.height)//2))
        draw.text((24,18),ident+' | INPUT REFERENCE',font=font,fill='white')
        draw.text((924,18),'HIGH | CROP OF LIVE HOUDINI WINDOW SCREENSHOT',font=font,fill='white')
        path=ROOT/cfg['outputs']['comparison'];path.parent.mkdir(parents=True,exist_ok=True);comparison.save(path)
        row['gui_screenshot']=shot;row['hda_sha256']=sha(asset);write(cfg['outputs']['result'],row)
        cards.append({'id':ident,'hero':str(preview.relative_to(ROOT)).replace('\\','/'),'full':shot['image'],'comparison':cfg['outputs']['comparison'],
                      'low':row['levels']['low']['primitives'],'high':row['levels']['high']['primitives']})
    for group in range(4):
        subset=cards[group*4:group*4+4]
        if not subset:continue
        sheet=Image.new('RGB',(1056,824),'#202327');draw=ImageDraw.Draw(sheet)
        for i,card in enumerate(subset):
            im=Image.open(ROOT/card['hero']).convert('RGB');im.thumbnail((512,364))
            x=(i%2)*528+8;y=(i//2)*412+40;sheet.paste(im,(x,y))
            draw.text((x,y-28),card['id']+' | HIGH',font=font,fill='white')
        sheet.save(ROOT/'screenshots/final'/('review_sheet_'+str(group+1)+'.png'))
    for file in [ROOT/'manifest.json']+sorted(ROOT.glob('manifest_group_*.json')):
        m=json.loads(file.read_text(encoding='utf-8'))
        m.update(hda_type='mythic::medieval_house::4.1',hda_file='asset/medieval_building_modular.hda',hda_sha256=sha(asset),
                 generator_version='2026.11.03-roof-high-gui-v3',render_backend='houdini_gui_window_screenshot',
                 preview_backend='Actual visible Houdini GUI native window capture; viewport crop from same pixels; no OpenGL ROP',
                 normal_bake_status='not_run; source expected_features are preserved requirements, not measured completion',unreal_status='not_run',
                 camera={'projection':'orthographic','target':[1,4,0],'delta':[17,11,-21],'orthowidth_m':21,'framing':'common hero camera across all 16 records; detail views separately labelled'},
                 effective_quality='high',bake_backend='not_run')
        file.write_text(json.dumps(m,indent=2,ensure_ascii=False),encoding='utf-8')
    page=['<!doctype html><meta charset="utf-8"><title>Medieval HIGH — Houdini GUI screenshots</title>',
          '<style>body{background:#191c20;color:#eee;font:16px system-ui;margin:30px}a{color:#9dcfff}main{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:20px}img{width:100%;height:auto}article{padding:14px;background:#262b31}h2{font-size:19px}</style>',
          '<h1>Medieval — HIGH, mái phụ và Houdini GUI</h1><p>15 variants + baseline. Actual Houdini 22.0.368 GUI window screenshots. Click an image for the full application window. Viewport crops preserve the captured pixels.</p>',
          '<p><a href="evaluation.md">QA / limits</a> · <a href="README_VI.md">Mở HDA và LOW/HIGH</a> · <a href="results/final_summary.json">Measured results</a></p><main>']
    for card in cards:
        page.append('<article><h2>'+card['id']+'</h2><p>LOW '+str(card['low'])+' → HIGH '+str(card['high'])+' primitives</p><a href="'+card['full']+'"><img src="'+card['hero']+'"></a><p><a href="'+card['comparison']+'">Reference comparison</a> · <a href="scenes/'+card['id']+'.hip">HIP</a></p></article>')
    page.append('</main><h1>Roof details / before / LOW–HIGH / wire</h1><main>')
    for shot in extra:
        path=shot['image'];crop=path.replace('_gui.png','_gui_viewport.png')
        if not (ROOT/crop).exists():crop=path
        page.append('<article><h2>'+html.escape(shot['tag'])+'</h2><a href="'+path+'"><img loading="lazy" src="'+crop+'"></a></article>')
    page.append('</main>');(ROOT/'review.html').write_text('\n'.join(page),encoding='utf-8')
    contract=read('module_graph.json');lines=['# Module contracts','', '| Module | Native network | Inputs | Outputs |','|---|---|---|---|']
    for m in contract['modules']:lines.append('| '+m['module_id']+' | '+m['path']+' | '+'; '.join(k+' ← '+v for k,v in m['inputs'].items())+' | '+', '.join(m['outputs'])+' |')
    (ROOT/'module_contracts.md').write_text('\n'.join(lines),encoding='utf-8')
    write('results/publication.json',{'status':'pass','hda_sha256':sha(asset),'hero_screenshots':len(captures),'extra_screenshots':len(extra),'all_native_gui':True,'all_v3_gui_mesh_hashes_match_exports':True,'cards':cards})
    print('PUBLISHED',len(captures),len(extra),flush=True)
if __name__=='__main__':main()
