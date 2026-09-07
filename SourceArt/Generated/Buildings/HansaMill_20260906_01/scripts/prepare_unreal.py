from unreal_ops import Client,P
import json
c=Client();identity={'project':str(P.parents[2]/'Hansa.uproject'),'job':P.name};(P/'identity.json').write_text(json.dumps(identity))
assert json.loads(c.call('asset','read_file',{'file_path':str(P/'identity.json')}))==identity
current=c.call('scene','get_current_level');dirty=c.call('asset','is_dirty',{'asset_path':current})
root='/Game/Hansa/Generated/Staging/HansaMill_20260906_01';assert not c.call('asset','exists',{'path':root})
print('PROJECT_CONFIRMED',identity,'LEVEL',current,'DIRTY',dirty,flush=True)
(P/'unreal_identity.json').write_text(json.dumps({'identity':identity,'level':current,'dirty':dirty,'staging':root},indent=2))
mat=c.call('material','create_material',{'folder_path':root+'/Materials','asset_name':'M_Mill_Oak'})
classes=c.call('material','list_expression_classes',{'material_or_function':mat,'search':''})
(P/'unreal_classes.json').write_text(json.dumps(classes,indent=2));(P/'unreal_seed.json').write_text(json.dumps(mat));print('EXPRESSION_CLASSES',str(classes)[:2000])

