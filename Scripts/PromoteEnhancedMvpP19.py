"""User-approved P19 copy/remap and exact vehicle-definition diff. No map edits."""
import hashlib,json,sys
from pathlib import Path
import PromoteEnhancedMvpP12P15 as p
REPO=Path(__file__).resolve().parents[1]
p.EVIDENCE=REPO/'Saved/GenerationJobs/approved-p19-20260908'
SOURCE='/Game/Hansa/Generated/Staging/Vehicles_P19';DEST='/Game/Mesh/hansa-vehicles'
AC='editor_toolset.toolsets.actor.ActorTools'

def plan():
    assert not p.call(p.A,'exists',path=DEST)
    assets=[]
    for file in sorted((REPO/'Content/Hansa/Generated/Staging/Vehicles_P19').rglob('*.uasset')):
        path='/Game/'+file.relative_to(REPO/'Content').with_suffix('').as_posix()
        kind=p.call(p.A,'get_asset_class',asset_path=path)
        if kind.endswith('_C'):kind='Blueprint'
        assert kind in ('Texture2D','Material','StaticMesh','Blueprint'),(path,kind)
        assert not p.call(p.A,'is_dirty',asset_path=path)
        assets.append({'source':path,'destination':path.replace(SOURCE,DEST,1),'class':kind,'sha256':hashlib.sha256(file.read_bytes()).hexdigest()})
    assert len(assets)==33,len(assets)
    definitions=[]
    for family in ('Cog','Wagon'):
        path='/Game/Hansa/Core/Vehicles/DA_Vehicle_'+family
        assert not p.call(p.A,'is_dirty',asset_path=path)
        before=p.get(p.ref(path),list(p.fields(p.ref(path))))
        definitions.append({'path':path,'before':before,'family':family})
    p.save([{'prompt':'P19','source':SOURCE,'destination':DEST,'assets':assets,'definitions':definitions}], 'plan.json')
    print('P19_PLAN',len(assets),'packages; 2 definitions; shared canonical family',DEST)

def assets(family):
    assert not p.call(p.A,'exists',path=DEST),'Existing/partial destination: inspect, do not overwrite'
    for item in family['assets']:
        file=REPO/'Content'/(item['source'].removeprefix('/Game/')+'.uasset')
        assert hashlib.sha256(file.read_bytes()).hexdigest()==item['sha256']
    for item in sorted(family['assets'],key=lambda i:{'Texture2D':0,'Material':1,'StaticMesh':2,'Blueprint':3}[i['class']]):
        assert p.call(p.A,'duplicate',path=item['source'],new_path=item['destination'])
        obj=p.ref(item['destination'])
        if item['class']=='Material':
            for expr in p.call(p.M,'get_expressions',material_or_function=obj):
                if 'texture' in p.fields(expr):p.set_values(expr,p.remap(p.get(expr,['texture']),family))
            p.call(p.M,'recompile',material_or_function=obj)
        elif item['class']=='StaticMesh':
            for slot in p.call(p.S,'get_material_slots',mesh=obj):
                mat=p.call(p.S,'get_material',mesh=obj,slot_name=slot)
                assert p.call(p.S,'set_material',mesh=obj,slot_name=slot,material=p.remap(mat,family))
        elif item['class']=='Blueprint':
            cdo=p.call(p.B,'get_default_object',blueprint=obj)
            for component in p.call(AC,'get_components',actor=cdo,component_type={'refPath':'/Script/Engine.StaticMeshComponent'}):
                old=p.get(component,['staticMesh','overrideMaterials'])
                p.set_values(component,p.remap(old,family))
            p.call(p.B,'compile_blueprint',blueprint=obj)
        assert p.call(p.A,'save_assets',asset_paths=[item['destination']])
        print('SAVED',item['destination'],flush=True)
    p.audit_assets([family])

def bind(family):
    assert (p.EVIDENCE/'P19-dependencies.json').exists()
    for definition in family['definitions']:
        obj=p.ref(definition['path']);before=definition['before'];name=definition['family']
        assert not p.call(p.A,'is_dirty',asset_path=definition['path'])
        assert p.get(obj,list(before))==before,'Definition changed since plan'
        bp=DEST+'/BP_'+name+'_Review';mesh=DEST+'/Meshes/'+('SM_HansaCog_Hull' if name=='Cog' else 'SM_HansaWagon_Body')
        changes={'presentationActorClass':p.ref(bp+'.'+bp.rsplit('/',1)[1]+'_C'),'presentationMesh':p.ref(mesh),'authoredRevision':before['authoredRevision']+1}
        p.save({'approval':'User explicitly approved P19 promotion on 2026-09-08','before':before,'changes':changes},'diff-'+name+'.json')
        p.set_values(obj,changes);assert p.call(p.A,'save_assets',asset_paths=[definition['path']])
        actual=p.get(obj,list(before))
        assert all(actual[k]==v for k,v in before.items() if k not in (*changes,'contentHash'))
        p.save(actual,'bound-'+name+'.json');print('BOUND',definition['path'])

if __name__=='__main__':
    mode=sys.argv[1]
    if mode=='plan':plan()
    else:
        family=json.loads((p.EVIDENCE/'plan.json').read_text())[0]
        if mode=='assets':assets(family)
        elif mode=='audit':p.audit_assets([family])
        elif mode=='bind':bind(family)
        else:raise ValueError(mode)
