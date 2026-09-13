"""Clean FBX/GLB round-trip verification for P17, preserving native capture size."""
from pathlib import Path
REPO=Path(__file__).resolve().parents[1]
JOB=REPO/'Saved/GenerationJobs/hansa-harbor_P17_20260908'
source=(REPO/'SourceArt/Generated/Buildings/HansaWarehouse_P16_20260908/scripts/reimport_warehouse.py').read_text()
source=source.replace('JOB=Path(__file__).resolve().parents[1];', 'JOB=Path('+repr(JOB.as_posix())+');')
source=source.replace("assert name.endswith('Hoist') or abs(lo[2])<.02,(name,lo)", "assert all(abs(lo[i]-min(p[i] for p in expected))<.025 for i in range(3)),(name,lo)")
start=source.index("  if name.endswith(('Skid','Cargo')):")
end=source.index("s.world=bpy.data.worlds.new",start)
source=source[:start]+'''  if name=='SM_HansaDock_Deck4m':
   for x in (-6,-2,2,6):
    copy=o.copy();s.collection.objects.link(copy);copy.location.x+=x
   o.hide_render=True
  if name=='SM_HansaQuay_Edge4m':
   copy=o.copy();s.collection.objects.link(copy);copy.location.y-=4
''' + source[end:]
source=source.replace("location=(12,-12,22)", "location=(10,-12,20)").replace('key.data.size=10','key.data.size=8').replace('Vector((0,0,4))','Vector((0,0,0))')
source=source.replace("('whole',(24,-28,22),(0,0,5.3),42),('detail',(14,-13,10),(5,0,4),48)", "('whole',(21,-24,18),(0,0,-.1),43),('detail',(11,-9,6),(6.3,.5,1.6),52)")
source=source.replace('P16_REIMPORT_VERIFIED','P17_REIMPORT_VERIFIED')
exec(source)
