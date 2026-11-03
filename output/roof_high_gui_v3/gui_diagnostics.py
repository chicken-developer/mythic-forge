import hou
b=hou.node('/obj/medieval_roof_high_v3/BUILDING')
result={'templates':[{'name':p.name(),'label':p.label(),'hidden':p.isHidden(),'type':str(p.type())} for p in b.parmTemplateGroup().entries()],
        'quality_hidden':b.parm('preview_quality').isHidden(),'quality_template':b.parm('preview_quality').parmTemplate().asCode(),
        'dormer_nodes':[(n.name(),n.type().name(),[i.name() for i in n.inputs() if i]) for n in b.node('ROOF/dormers/DORMER_MODULE').children()]}
