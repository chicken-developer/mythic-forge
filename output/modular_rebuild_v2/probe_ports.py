from pathlib import Path
import hou
root=Path(__file__).resolve().parent
obj=hou.node('/obj').createNode('geo','port_probe',run_init_scripts=False)
sub=obj.createNode('subnet','many_ports')
print('BEFORE',len(sub.indirectInputs()),sub.type().maxNumInputs(),flush=True)
asset=sub.createDigitalAsset(name='mythic_probe::stage::1.0',hda_file_name=str(root/'results/ports_probe.hda'),min_num_inputs=0,max_num_inputs=20)
asset.allowEditingOfContents()
print('AFTER',len(asset.indirectInputs()),asset.type().maxNumInputs(),flush=True)
for index in range(8):
    n=asset.createNode('null',f'IN_{index}')
    n.setInput(0,asset.indirectInputs()[index])
    o=asset.createNode('output',f'OUT_{index}');o.setInput(0,n);o.parm('outputidx').set(index)
asset.type().definition().updateFromNode(asset)
print('OUTPUTS',len(asset.outputConnectors()),asset.type().maxNumOutputs(),flush=True)
print('PACK',[(p.name(),p.eval()) for p in obj.createNode('pack').parms() if p.name() in ('packbyname','nameattribute','createpath','transfer_attributes')],flush=True)
print('DOC',hou.HDADefinition.setMinNumInputs.__doc__,flush=True)
