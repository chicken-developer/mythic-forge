"""Render measured Houdini geometry using plain base colors."""
from pathlib import Path
import math
import hou

COLORS = {'plaster': (.79,.66,.45), 'wood': (.22,.10,.041),
          'stone': (.45,.43,.37), 'tile': (.15,.27,.42), 'roof': (.15,.27,.42),
          'glass': (.92,.51,.13), 'teal': (.12,.32,.37), 'canvas': (.73,.52,.28),
          'metal': (.13,.15,.16), 'rock': (.42,.43,.34), 'light': (1.,.67,.25),
          'leaf': (.22,.36,.08), 'flower': (.70,.35,.22)}

def plain_materials(building, red=False):
    root=building.node('materials')
    for mat in root.children():
        color = COLORS.get(mat.name(), (.42,.32,.22))
        if red and mat.name() in ('roof','tile'): color=(.54,.19,.075)
        if mat.parmTuple('basecolor'): mat.parmTuple('basecolor').set(color)
        for parm in mat.parms():
            if 'usetexture' in parm.name().lower() and isinstance(parm.parmTemplate(), hou.ToggleParmTemplate):
                parm.set(0,follow_parm_reference=False)
        if mat.parm('baseBumpAndNormal_enable'):mat.parm('baseBumpAndNormal_enable').set(0)
        for key in ('rough','roughness'):
            if mat.parm(key):mat.parm(key).set(.72)
        for key in ('transparency','transmission'):
            if mat.parm(key):mat.parm(key).set(0)

def setup(building, ortho=21, res=720, backend='opengl'):
    obj=hou.node('/obj')
    building.setDisplayFlag(True)
    for node in obj.children():
        if node!=building and node.type().name() not in ('cam',):
            if hasattr(node,'setDisplayFlag'):node.setDisplayFlag(False)
            if hasattr(node,'setRenderFlag'):node.setRenderFlag(False)
            if node.parm('light_enable'):node.parm('light_enable').set(0)
    cam=obj.createNode('cam','REVIEW_camera')
    target=hou.Vector3(0,3.6,0)
    eye=hou.Vector3(15,12,-19)
    delta=target-eye
    rotation=(math.degrees(math.atan2(delta[1],math.hypot(delta[0],delta[2]))),
              math.degrees(math.atan2(-delta[0],-delta[2])),0.)
    cam.parmTuple('t').set(eye);cam.parmTuple('r').set(rotation)
    cam.parm('projection').set('ortho');cam.parm('orthowidth').set(ortho)
    cam.parmTuple('res').set((res,res))
    for name,pos,power,size in [('key',(-8,15,-10),1.1,7),('fill',(10,10,-3),.7,8),('rim',(1,14,12),.7,6)]:
        light=obj.createNode('hlight','REVIEW_'+name)
        light.parm('light_type').set('distant');light.parm('light_intensity').set(power)
        light.parmTuple('areasize').set((size,size))
        light.parmTuple('t').set(pos)
        light.parmTuple('r').set(hou.Vector3(0,0,-1).matrixToRotateTo(target-hou.Vector3(pos)).extractRotates())
    rop=hou.node('/out').createNode('opengl' if backend=='opengl' else 'ifd','REVIEW_render')
    rop.parm('camera').set(cam.path())
    if backend=='opengl':
        rop.parm('vobjects').set(building.path())
        rop.parm('alights').set('REVIEW_*')
        rop.parm('usetextures').set(0)
        rop.parm('hqlighting').set(1)
    if backend!='opengl':
        rop.parm('vm_renderengine').set('raytrace')
        rop.parmTuple('vm_samples').set((3,3))
        rop.parm('vobject').set(building.path())
        rop.parm('soho_pipecmd').set('mantra -j 8')
        rop.parm('soho_foreground').set(1)
    return rop,cam

def render(rop,path):
    path=Path(path);path.parent.mkdir(parents=True,exist_ok=True)
    rop.parm('picture' if rop.type().name()=='opengl' else 'vm_picture').set(str(path))
    rop.render()

if __name__=='__main__':
    import argparse,time,json
    root=Path(__file__).resolve().parents[1]
    ap=argparse.ArgumentParser();ap.add_argument('--scene',type=Path,default=root/'medieval_building.hip');ap.add_argument('--output',type=Path,default=root/'review_20261102/after.png');args=ap.parse_args()
    hou.hipFile.load(str(args.scene),suppress_save_prompt=True,ignore_load_warnings=True)
    b=hou.node('/obj/medieval_building')
    start=time.monotonic();geo=b.node('OUT_BUILDING').geometry()
    print('COOK',len(geo.points()),len(geo.prims()),list(geo.boundingBox().sizevec()),time.monotonic()-start,flush=True)
    errors=[(n.path(),n.errors()) for n in b.allSubChildren() if n.errors()]
    print('ERRORS',errors,flush=True)
    plain_materials(b,True)
    rop,cam=setup(b)
    render(rop,args.output)
    print('RENDERED',flush=True)
