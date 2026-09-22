"""Save/reopen evidence and native viewport captures for the authored tree layout."""
import json,sys,math,base64,time,collections,shutil
from pathlib import Path
import PlaceLubeckTreesMcp as P
from ue_batch import call
S='editor_toolset.toolsets.scene.SceneTools';A='editor_toolset.toolsets.actor.ActorTools';APP='EditorToolset.EditorAppToolset'
done=json.loads((P.JOB/'placed_trees.json').read_text());assert len(done)==320 and len({t['id'] for t in done})==320
assert all(t['water_clearance_cm']>=1300 and t['source_slope_degrees']<=15 and abs(t['z']-t['source_z'])<150 for t in done)
assert call(S,'get_current_level')==P.LEVEL
context=json.loads(call('HansaEditor.HansaCityTerrainToolset','InspectAuthoringContext'))
assert not context['dirtyPackages'],context['dirtyPackages']
call(S,'load_level',level_path=P.LEVEL)
checks=[]
# Soft actor references resolve the saved external packages into the editor world.
# Preserve normal World Partition streaming instead of marking the forest always-loaded.
for first in range(0,len(done),40):
    chunk=done[first:first+40]
    script="import json\ndef transform(actor):\n    return execute_tool('editor_toolset.toolsets.actor.ActorTools.get_actor_transform',json.dumps({'actor':actor}))['returnValue']\ndef run():\n    records=json.loads("+repr(json.dumps(chunk))+")\n    results=[]\n    for t in records:\n        tr=transform(t['actor'])\n        error=max(abs(tr['location'][axis]-t[axis]) for axis in ['x','y','z'])\n        assert error<.01\n        results.append({'id':t['id'],'location_error_cm':error})\n    return {'checks':results}"
    r=call('editor_toolset.toolsets.programmatic.ProgrammaticToolset','execute_tool_script',script=script)
    if isinstance(r,str):r=json.loads(r)
    checks.extend(r['checks'])
actors=call(S,'find_actors',name='',tag=P.TAG,collision_channels=[]);assert len(actors)==320,len(actors)
out=P.ROOT/'Docs/Images/World/LubeckTrees_20260916';out.mkdir(exist_ok=True)
def capture(name,loc,target):
    dx,dy,dz=[b-a for a,b in zip(loc,target)];tr={'location':dict(zip('xyz',loc)),'rotation':{'pitch':math.degrees(math.atan2(dz,math.hypot(dx,dy))),'yaw':math.degrees(math.atan2(dy,dx)),'roll':0},'scale':{'x':1,'y':1,'z':1}}
    call(APP,'SetCameraTransform',transform=tr);time.sleep(2)
    c=call(APP,'CaptureViewport',captureTransform=tr,bShowUI=False,annotations={'gridSpacing':0,'gridExtent':0,'gridHeight':0,'maxLabelDistance':0,'classFilter':{'refPath':'/Script/Engine.Actor'},'maxLabels':0})
    (out/(name+'.png')).write_bytes(base64.b64decode(c['image']['data']))
capture('founding_woodland',(-54000,-17000,16000),(-44000,2500,400))
capture('bank_grove',(-59000,4000,10000),(-49000,15500,450))
capture('regional_grove',(-140000,-85000,14000),(-125000,-75000,1300))
capture('overview',(-60000,-35000,43000),(-36000,10000,400))
capture('founding_woodland',(-54000,-17000,16000),(-44000,2500,400))
report={'map':P.LEVEL,'count':len(actors),'seed':20260916,'by_species':dict(collections.Counter(t['species'] for t in done)),'by_age':dict(collections.Counter(t['age'] for t in done)),'reopened':True,'sample_transform_checks':checks,'minimum_water_clearance_cm':min(t['water_clearance_cm'] for t in done),'maximum_source_slope_degrees':max(t['source_slope_degrees'] for t in done),'tag':P.TAG,'outliner_folder':'Environment/Trees/LubeckSummer','gameplay_integration':'Static environmental map actors only; harvesting, construction clearing and regrowth remain unimplemented.'}
(out/'verification.json').write_text(json.dumps(report,indent=2))
dest=P.ROOT/'SourceArt/Generated/Trees/LubeckSummer/v1/placement';dest.mkdir(exist_ok=True)
shutil.copy2(P.JOB/'placement_plan.json',dest/'placement_plan.json');shutil.copy2(P.JOB/'placed_trees.json',dest/'placed_trees.json')
doc=P.ROOT/'Docs/Development/LubeckTreePlacement.md'
doc.write_text('# Lübeck tree placement — 2026-09-16\n\n'+f'Added and saved {len(actors)} individual StaticMeshActors to the configured startup/game map `{P.LEVEL}` through Unreal MCP. The user explicitly requested applying the previously reviewed tree family to the currently used Lübeck map. This is a placement revision on the existing staged terrain, not a Shipping release or a terrain promotion.\n\n'+
'The ten species/age meshes are reused with seeded yaw and uniform scale variation. Twelve groups provide mixed woodland near the founding area and six regional groves. A 60m radius around the founding focus remains clear. Hydrology rejection uses the same retained water-build.json as the native water bodies and gameplay placement classifier; trunks are at least 13m outside that geometry. Candidate slopes are at most 15 degrees. Each accepted tree was grounded using a live Unreal trace near the surveyed height, avoiding interception by previously placed crowns.\n\n'+
'The actors are grouped under `Environment/Trees/LubeckSummer/<species>`, tagged `HansaTree.LubeckPlacement.20260916`, and carry a stable authoring label such as `LubeckTree_0001_Beech_Mature`. These labels are authoring metadata, not authoritative simulation IDs. World Partition external actor and folder packages were saved. Reload verification resolved the saved World Partition actor references and found all 320 tagged actors; all 320 saved transforms match within 0.01cm. No terrain, native water geometry, gameplay building/road definitions, or saved-game state was edited.\n\n'+
'Species counts: '+str(report['by_species'])+'. Ages: '+str(report['by_age'])+'.\n\n'+
'Evidence: `Docs/Images/World/LubeckTrees_20260916/verification.json` and sibling native captures. Durable placement records are under `SourceArt/Generated/Trees/LubeckSummer/v1/placement/`. Reproduction tools: `Scripts/PlaceLubeckTreesMcp.py` and `Scripts/PlaceLubeckTreesBatchMcp.py`; they resume by tags/labels and do not duplicate existing trees. The supplied map uses the existing staged mesh references, consistent with its current staged terrain status.\n\n'+
'This adds environmental trees only. Lumber hut targeting, harvest yield, automatic construction clearing, persistence of harvested trees and wind are not implemented by this placement task. The 320-actor layout has not received a forest-scale performance acceptance test.\n')
print(json.dumps(report),flush=True)
