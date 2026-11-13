"""Turn a populated subnet into an HDA: interface from ParmSpecs, PythonModule, embedded presets."""
import json
from . import parms as P
from .presets import PYTHON_MODULE, SECTION


def make_hda(subnet, type_name, label, hda_path, specs, presets=None, max_inputs=1, help_text="", max_outputs=1):
    import hou
    hda_node = subnet.createDigitalAsset(name=type_name, hda_file_name=hda_path, description=label,
                                         min_num_inputs=0, max_num_inputs=max_inputs)
    defn = hda_node.type().definition()
    if max_outputs > 1:
        try:
            defn.setMaxNumOutputs(max_outputs)          # outputs come from the `output` SOPs inside
        except AttributeError:
            pass                                         # older API: outputs still follow the output SOPs
    defn.setParmTemplateGroup(P.build_template_group(specs))
    defn.addSection("PythonModule", PYTHON_MODULE)
    defn.setExtraFileOption("PythonModule/IsPython", True)
    defn.addSection(SECTION, json.dumps(presets or {}, indent=2))
    if help_text:
        defn.addSection("Help", help_text)
    defn.save(hda_path, hda_node)
    hda_node.matchCurrentDefinition()
    return hda_node


def embed_presets(hda_node, presets):
    """Replace the embedded preset section with `presets` ({name: preset}) and save the .hda."""
    defn = hda_node.type().definition()
    defn.addSection(SECTION, json.dumps(presets, indent=2))
    defn.save(defn.libraryFilePath(), hda_node)
