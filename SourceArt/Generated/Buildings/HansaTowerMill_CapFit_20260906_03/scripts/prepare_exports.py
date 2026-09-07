from pathlib import Path
P=Path(__file__).resolve().parents[1]
old=P.parent/'hansa-tower-mill_20260906_02'
tail=(old/'scripts/bake_export.py').read_text().split('# Build standard image-based delivery materials.')[1]
prefix="""import bpy,pathlib,json,shutil
P=pathlib.Path(__file__).resolve().parents[1]
old=P.parent/'hansa-tower-mill_20260906_02'
bpy.ops.wm.open_mainfile(filepath=str(P/'exports/HansaTowerMill_CapFit.blend'))
S=bpy.context.scene
inventory=json.loads((old/'material_inventory.json').read_text())
for rec in inventory:
 for kind,path in rec['maps'].items():
  dest=P/'exports'/pathlib.Path(path).name;shutil.copy2(path,dest);rec['maps'][kind]=str(dest)
(P/'material_inventory.json').write_text(json.dumps(inventory,indent=2))
"""
(P/'scripts/export_fixed.py').write_text(prefix+'# Build standard image-based delivery materials.'+tail.replace('HansaTowerMill','HansaTowerMill_CapFit'))
s=(old/'scripts/verify_exports.py').read_text().replace('HansaTowerMill','HansaTowerMill_CapFit').replace("['Front','Base_Detail']","['Cap_Fit']").replace('resolution_x=1000','resolution_x=1100').replace('resolution_y=1200','resolution_y=1100')
(P/'scripts/verify_exports.py').write_text(s)
for name in ['finish_preview.py','final_unreal_check.py','capture_unreal.py','check_import_options.py']:
 (P/'scripts'/name).write_text((old/'scripts'/name).read_text())
