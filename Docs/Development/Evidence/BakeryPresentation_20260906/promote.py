
import sys,pathlib,json,hashlib
B=pathlib.Path.cwd();sys.path.insert(0,str(B/'Saved/GenerationJobs/hansa-bakery_imagegen_20260906_02/scripts'))
from unreal_ops import Client
c=Client();J=B/'Saved/GenerationJobs/hansa-bakery_runtime_20260906_03';J.mkdir(exist_ok=True)
R=B/'SourceArt/Generated/Buildings/HansaBakery_ImageGen_20260906_02'
records=json.loads((R/'evidence/unreal_materials.json').read_text());oldmesh=json.loads((R/'evidence/unreal_import_final.json').read_text())['mesh']
root='/Game/Mesh/hansa-bakery';mapping={};created=[]
# A unique source marker verifies that this native editor has the actual current project.
marker={'job':J.name,'approval':'Johan: Approve production promotion and bakery assignment'}
(J/'identity.json').write_text(json.dumps(marker));assert json.loads(c.call('asset','read_file',{'file_path':str(J/'identity.json')}))==marker
def clone(src,dest):
 assert not c.call('asset','exists',{'path':dest}),dest
 assert c.call('asset','duplicate',{'path':src.split('.')[0],'new_path':dest}),(src,dest)
 obj=c.call('asset','load_asset',{'asset_path':dest});created.append(dest);mapping[src]=obj;return obj
for r in records:
 for k,t in r['textures'].items():
  src=t['asset']['refPath']
  if src not in mapping:clone(src,root+'/Textures/T_Bakery_'+r['name']+'_'+k)
print('TEXTURES_PROMOTED',len(mapping),flush=True)
for r in records:
 mat=clone(r['material']['refPath'],root+'/Materials/M_Bakery_'+r['name'])
 for node in c.call('material','get_expressions',{'material_or_function':mat}):
  props=json.loads(c.call('object','list_properties',{'instance':node}))
  if 'texture' not in props:continue
  v=json.loads(c.call('object','get_properties',{'instance':node,'properties':['texture']}))
  src=v.get('texture',{}).get('refPath') if isinstance(v.get('texture'),dict) else None
  if src in mapping:assert c.call('object','set_properties',{'instance':node,'values':json.dumps({'texture':mapping[src]})})
 c.call('material','recompile',{'material_or_function':mat});c.call('asset','save_assets',{'asset_paths':[mat['refPath']]})
 print('MATERIAL',r['name'],flush=True)
mesh=clone(oldmesh['refPath'],root+'/Meshes/SM_HansaBakery')
for r in records:assert c.call('mesh','set_material',{'mesh':mesh,'slot_name':r['slot'],'material':mapping[r['material']['refPath']]})
c.call('mesh','set_nanite_enabled',{'mesh':mesh,'enabled':True});assert c.call('mesh','is_nanite_enabled',{'mesh':mesh})
assert c.call('mesh','generate_convex_collisions',{'mesh':mesh,'hull_count':1,'max_hull_verts':16,'hull_precision':10000})
c.call('asset','update_metadata_tags',{'asset_path':mesh['refPath'],'set_tags':{'Hansa.StablePurpose':'Building.Bakery','Hansa.ApprovedBy':'Johan','Hansa.Approval':'Approve production promotion and bakery assignment; 2026-09-06','Hansa.SourcePackage':'SourceArt/Generated/Buildings/HansaBakery_ImageGen_20260906_02','Hansa.SourceManifestSha256':hashlib.sha256((R/'MANIFEST.sha256.json').read_bytes()).hexdigest()},'remove_tags':[]})
assert c.call('asset','save_assets',{'asset_paths':created})
seen=set();queue=[mesh['refPath'].split('.')[0]]
while queue:
 p=queue.pop()
 if p in seen:continue
 seen.add(p);deps=c.call('asset','get_dependencies',{'asset_path':p})
 for d in deps:
  assert '/Generated/Staging/' not in d and '/Developer/' not in d,(p,d)
  if d.startswith('/Game/'):queue.append(d.split('.')[0])
(J/'promotion_assets.json').write_text(json.dumps({'mesh':mesh,'created_assets':created,'dependency_closure':sorted(seen),'mapping':mapping},indent=2))
definition=c.call('asset','load_asset',{'asset_path':'/Game/Hansa/Core/Buildings/DA_Building_Bakery'})
props=json.loads(c.call('object','list_properties',{'instance':definition}));assert 'presentationMesh' in props
before=json.loads(c.call('object','get_properties',{'instance':definition,'properties':['presentationMesh','authoredRevision']}))
assert c.call('object','set_properties',{'instance':definition,'values':json.dumps({'presentationMesh':mesh,'authoredRevision':before['authoredRevision']+1})})
assert c.call('asset','save_assets',{'asset_paths':[definition['refPath']]})
(J/'promotion.json').write_text(json.dumps({'mesh':mesh,'definition':definition,'before':before,'approved_by':'Johan','authorization':'Approve production promotion and bakery assignment','created_assets':created,'dependency_closure':sorted(seen),'nanite':True,'selection_collision':'one 16-vertex convex hull','mapping':mapping},indent=2))
print('PROMOTION_VERIFIED',len(created),len(seen),flush=True)

