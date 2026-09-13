"""Reuse the verified swatch exporter for the seven P17 modules."""
from pathlib import Path
REPO=Path(__file__).resolve().parents[1]
JOB=REPO/'Saved/GenerationJobs/hansa-harbor_P17_20260908'
source=(REPO/'SourceArt/Generated/Buildings/HansaWarehouse_P16_20260908/scripts/export_reference.py').read_text()
source=source.replace('JOB=Path(__file__).resolve().parents[1]', 'JOB='+repr(JOB.as_posix())+'\nJOB=Path(JOB)')
source=source.replace('Market-r5.blend','Harbor-r5.blend').replace('M_Market_','M_Harbor_').replace('SM_HansaMarket','SM_Hansa')
source=source.replace('HansaMarket.blend','HansaHarbor.blend').replace('P15_EXPORT_COMPLETE','P17_EXPORT_COMPLETE')
source=source.replace("materials=[m for m in bpy.data.materials if m.name.startswith('M_Harbor_') and 'ReviewGround' not in m.name]", "materials=[bpy.data.materials['M_Harbor_'+name] for name in ('Oak','WetOak','Iron','Wicker')]")
source=source.replace('copies.append(o)', 'o.hide_render=False; o.hide_set(False); copies.append(o)')
exec(source)
