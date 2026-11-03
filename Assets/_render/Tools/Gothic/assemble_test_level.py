from pathlib import Path
import runpy
root=(Path(__file__).resolve().parents[4] / 'Assets/_render/Tools/Gothic')
for file in ['assign_materials.py','validate_assets.py','build_lineup.py']:
 runpy.run_path(str(root/file),run_name='__main__')
