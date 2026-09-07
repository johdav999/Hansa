import bpy,pathlib,json
P=pathlib.Path(__file__).resolve().parents[1]
bpy.context.preferences.filepaths.save_version=0
records=[]
for im in bpy.data.images:
 if im.type=='IMAGE':
  if not im.packed_file:im.pack()
  assert im.packed_file,im.name
  records.append({'name':im.name,'size':list(im.size),'packed':True})
bpy.ops.wm.save_as_mainfile(filepath=str(P/'exports/HansaBakery.blend'))
(P/'evidence/packing.json').write_text(json.dumps(records,indent=2))
print('PACKED',len(records))
