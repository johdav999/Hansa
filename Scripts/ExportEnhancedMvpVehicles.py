"""Portable P19 role export. Bake shaders, never resize the ImageGen source."""
from pathlib import Path
REPO=Path(__file__).resolve().parents[1]
JOB=REPO/'Saved/GenerationJobs/hansa-vehicles_P19_20260908'
source=(REPO/'SourceArt/Generated/Buildings/HansaWarehouse_P16_20260908/scripts/export_reference.py').read_text()
source=source.replace('JOB=Path(__file__).resolve().parents[1]', 'JOB=Path('+repr(JOB.as_posix())+')')
source=source.replace('Market-r5.blend','Vehicles-r4.blend').replace('M_Market_','M_Vehicle_').replace('SM_HansaMarket','SM_Hansa')
source=source.replace('HansaMarket.blend','HansaVehicles.blend').replace('P15_EXPORT_COMPLETE','P19_EXPORT_COMPLETE')
source=source.replace("materials=[m for m in bpy.data.materials if m.name.startswith('M_Vehicle_') and 'ReviewGround' not in m.name]", "materials=[bpy.data.materials['M_Vehicle_'+name] for name in ('Oak','TarredOak','Linen','Hemp','Iron','Canvas')]")
source=source.replace('copies.append(o)', 'o.hide_render=False; o.hide_set(False); copies.append(o)')
source=source.replace("'revision':5", "'revision':4")
source=source.replace("'sourceCoverageMetres':2", "'sourceCoverageMetres':{'default':2,'M_Vehicle_Linen':0.5}")
source=source.replace("bpy.ops.wm.save_as_mainfile(filepath=str(JOB/'exports'/'HansaVehicles.blend'))", "bpy.ops.file.pack_all()\nbpy.ops.wm.save_as_mainfile(filepath=str(JOB/'exports'/'HansaVehicles.blend'))")
exec(source)
