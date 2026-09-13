"""Export P20 street modules with packed sources and native portable material maps."""
from pathlib import Path
import sys
REV = int(sys.argv[-1]) if sys.argv[-1].isdigit() else 5
REPO=Path(__file__).resolve().parents[1]
JOB=REPO/'Saved/GenerationJobs/city-life_P20_20260908'
EXPORT_DIR = 'exports' if REV == 5 else 'exports-r'+str(REV)
assert not (JOB/EXPORT_DIR/'export-manifest.json').exists(), 'Preserve prior exports; select a new revision.'
(JOB/EXPORT_DIR).mkdir(exist_ok=True)
source=(REPO/'SourceArt/Generated/Buildings/HansaWarehouse_P16_20260908/scripts/export_reference.py').read_text()
source=source.replace('JOB=Path(__file__).resolve().parents[1]','JOB=Path('+repr(JOB.as_posix())+')')
source=source.replace('Market-r5.blend','CityProps-r'+str(REV)+'.blend').replace('M_Market_','M_CityLife_')
source=source.replace("col.name.startswith('SM_HansaMarket')","col.name.startswith('SM_Hansa')")
source=source.replace('HansaMarket.blend','HansaCityProps.blend').replace('P15_EXPORT_COMPLETE','P20_PARTIAL_EXPORT_COMPLETE')
source=source.replace("materials=[m for m in bpy.data.materials if m.name.startswith('M_CityLife_') and 'ReviewGround' not in m.name]", "materials=[bpy.data.materials['M_CityLife_'+name] for name in ('Oak','Iron','Wicker')]")
source=source.replace("bpy.ops.wm.save_as_mainfile(filepath=str(JOB/'exports'/'HansaCityProps.blend'))", "bpy.ops.file.pack_all()\nbpy.ops.wm.save_as_mainfile(filepath=str(JOB/'exports'/'HansaCityProps.blend'))")
source=source.replace("'exports'",repr(EXPORT_DIR)).replace("'revision':5", "'revision':"+str(REV))
exec(source)
