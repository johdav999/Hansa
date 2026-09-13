"""P19 staged import only; run discovery and review evidence before promotion."""
import json,sys
from pathlib import Path
REPO=Path(__file__).resolve().parents[1]
JOB=REPO/'Saved/GenerationJobs/hansa-vehicles_P19_20260908'
sys.path.insert(0,str(REPO/'SourceArt/Generated/Buildings/HansaSawmill_P13_20260908/scripts'))
from ue_batch import call
source=(REPO/'SourceArt/Generated/Buildings/HansaMarket_P15_20260908/scripts/import_market.py').read_text()
source=source[:source.index("bp=call(BP,'create'")]
source=source.replace('from ue_batch import *','').replace('Market_P15','Vehicles_P19').replace("manifest['revision']==5","manifest['revision']==4")
source=source.replace("assert call(SC,'get_current_level')=='/Game/Hansa/Generated/Staging/Residences_P14/L_Residences_Review'", "assert call(SC,'get_current_level')=='/Game/Hansa/World/Cities/Lubeck/L_Lubeck_MVP'\nassert not call(AS,'is_dirty',asset_path=call(SC,'get_current_level'))")
source=source.replace("if name=='SM_HansaMarket':rec['simpleCollision']='FBX UCX: floor and four columns; checked by staged test'\n else:", '')
source=source.replace("for axis in ('x','y'):assert rec['bounds']['min'][axis]>=-580 and rec['bounds']['max'][axis]<=580", "for axis in ('x','y'):assert rec['bounds']['min'][axis]>=-1300 and rec['bounds']['max'][axis]<=1300")
source=source.replace("assert abs(rec['bounds']['min']['z'])<=2", "assert rec['bounds']['min']['z']>=-216 and rec['bounds']['max']['z']<=1771")
source=source.replace("('Oak','Clay','Canvas','Stone','Iron','Brass','Wicker','Sack')", "('Oak','TarredOak','Linen','Hemp','Iron','Canvas')")
source=source.replace('M_Market_','M_Vehicle_').replace('T_Market_','T_Vehicle_')
exec(compile(source,str(__file__),'exec'))
blueprints={}
for family in ('Cog','Wagon'):
    bp=call(BP,'create',folder_path=ROOT,asset_name='BP_'+family+'_Review',asset_type={'refPath':'/Script/Hansa.HansaCargoVehiclePresentation'})
    cdo=call(BP,'get_default_object',blueprint=bp)
    props(cdo,{'bSeaVehicle':family=='Cog'})
    roles={'Body':'SM_HansaCog_Hull','Rig':'SM_HansaCog_Rig','Sail':'SM_HansaCog_Sail','FurledSail':'SM_HansaCog_FurledSail','Cargo':'SM_HansaCargo_Sacks'} if family=='Cog' else {'Body':'SM_HansaWagon_Body','Cargo':'SM_HansaCargo_Sacks',**{'Wheel'+str(i):'SM_HansaWagon_Wheel' for i in range(4)}}
    components=call(AC,'get_components',actor=cdo,component_type={'refPath':'/Script/Engine.StaticMeshComponent'})
    assert len(components)==9
    for component in components:
        role=component['refPath'].split(':')[-1].split('.')[-1]
        if role in roles:props(component,{'StaticMesh':meshes[roles[role]]['mesh']})
    call(BP,'compile_blueprint',blueprint=bp);call(AS,'save_assets',asset_paths=[bp['refPath']])
    blueprints[family]={'blueprint':bp,'cdo':cdo,'components':components}
(JOB/'evidence/unreal-blueprints.json').write_text(json.dumps(blueprints,indent=2))
print('P19_STAGED_NOT_PROMOTED')
