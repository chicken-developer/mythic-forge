from pathlib import Path
import runpy
root=Path('E:/Repos/mythic-forge/TDKProps/Tools/Gothic')
for file in ['assign_materials.py','validate_assets.py','build_lineup.py']:
 runpy.run_path(str(root/file),run_name='__main__')
