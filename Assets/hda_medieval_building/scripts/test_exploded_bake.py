from pathlib import Path
import time
import hou
from bake_variants import node,bake_normal
ROOT=Path(__file__).resolve().parents[1]
obj=hou.node('/obj').createNode('geo','diagnostic_exploded',run_init_scripts=False)
low=node(obj,'file','low',file=str(ROOT/'geometry/baseline.bgeo.sc'))
high=node(obj,'file','high',file=str(ROOT.parent/'_render/SourceAssets/Medieval15/High/baseline.bgeo.sc'))
start=time.monotonic();paths,stats=bake_normal(low,high,'diagnostic_exploded',512)
print('EXPLODED_BAKE',time.monotonic()-start,stats,flush=True)
