import hou
n=hou.ui.paneTabOfType(hou.paneTabType.NetworkEditor)
items=[x for x in n.pwd().children() if x.name() not in ('materials','CTRL_BUILDING')]
positions=[x.position() for x in items]
bounds=hou.BoundingRect(hou.Vector2(min(p[0] for p in positions)-.5,min(p[1] for p in positions)-.5),hou.Vector2(max(p[0] for p in positions)+3,max(p[1] for p in positions)+1))
n.setVisibleBounds(bounds,transition_time=0,max_scale=1000)
result={'network':n.pwd().path(),'nodes':[x.name() for x in items],'bounds':str(bounds)}
