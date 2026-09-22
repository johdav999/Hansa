"""Batch the same bounded tree placement through MCP's supported tool orchestrator."""
import json,sys
from pathlib import Path
import PlaceLubeckTreesMcp as P
from ue_batch import call
S='editor_toolset.toolsets.scene.SceneTools';A='editor_toolset.toolsets.actor.ActorTools'
assert call(S,'get_current_level')==P.LEVEL
trees=json.loads((P.JOB/'placement_plan.json').read_text())['trees']
statefile=P.JOB/'placed_trees.json';done=json.loads(statefile.read_text()) if statefile.exists() else []
existing=call(S,'find_actors',name='',tag=P.TAG,collision_channels=[])
labels={call(A,'get_label',actor=a):a for a in existing}
for t in trees:
    label=t['id']+'_'+t['species']+'_'+t['age']
    if label in labels and not any(d['id']==t['id'] for d in done):
        tr=call(A,'get_actor_transform',actor=labels[label]);done.append(dict(**t,label=label,z=tr['location']['z'],actor=labels[label]))
statefile.write_text(json.dumps(done,indent=2))
pending=[t for t in trees if not any(d['id']==t['id'] for d in done)]
header='''import json
def level():
    return execute_tool("editor_toolset.toolsets.scene.SceneTools.get_current_level", "{}")['returnValue']
def trace(x,y,h):
    return execute_tool("editor_toolset.toolsets.scene.SceneTools.trace_world",json.dumps({'start':{'x':x,'y':y,'z':h+100},'end':{'x':x,'y':y,'z':h-200}}))['returnValue']
def spawn(path,label,x,y,z,yaw,scale):
    return execute_tool("editor_toolset.toolsets.scene.SceneTools.add_to_scene_from_asset",json.dumps({'asset_path':path,'name':label,'xform':{'location':{'x':x,'y':y,'z':z},'rotation':{'pitch':0,'yaw':yaw,'roll':0},'scale':{'x':scale,'y':scale,'z':scale}}}))['returnValue']
def tag(actor,tags):
    return execute_tool("editor_toolset.toolsets.object.ObjectTools.set_properties",json.dumps({'instance':actor,'values':json.dumps({'tags':tags})}))['returnValue']
def folder(actor,species):
    return execute_tool("editor_toolset.toolsets.scene.SceneTools.set_actor_folder",json.dumps({'actor':actor,'folder_path':'Environment/Trees/LubeckSummer/'+species}))['returnValue']
'''
for first in range(0,len(pending),10):
    batch=pending[first:first+10]
    script=header+'\ndef run():\n'
    script+='    assert level()=='+repr(P.LEVEL)+'\n'
    script+='    trees=json.loads('+repr(json.dumps(batch))+')\n    result=[]\n'
    script+='''    for t in trees:
        d=trace(t['x'],t['y'],t['source_z'])
        assert d is not None and 0<=d<=300
        z=t['source_z']+100-d
        assert abs(z-t['source_z'])<150
        label=t['id']+'_'+t['species']+'_'+t['age']
        a=spawn(t['mesh'],label,t['x'],t['y'],z,t['yaw'],t['scale'])
        assert a
        tag(a,['HansaTree.LubeckPlacement.20260916',t['id'],'Tree.Species.'+t['species'],'Tree.Age.'+t['age']])
        folder(a,t['species'])
        t.update({'z':z,'actor':a,'label':label});result.append(t)
    return {'trees':result}
'''
    r=call('editor_toolset.toolsets.programmatic.ProgrammaticToolset','execute_tool_script',script=script)
    if isinstance(r,str):r=json.loads(r)
    done.extend(r['trees']);statefile.write_text(json.dumps(done,indent=2));print('PLACED',len(done),flush=True)
context=json.loads(call('HansaEditor.HansaCityTerrainToolset','InspectAuthoringContext'))
(P.JOB/'pre_save_context.json').write_text(json.dumps(context,indent=2))
assert all('LubeckTerrain_20260907/L_Lubeck_Terrain_Preview_WP' in p for p in context['dirtyPackages']),context['dirtyPackages']
assert call('editor_toolset.toolsets.asset.AssetTools','save_assets',asset_paths=[])
print('ALL_TREES_AND_MAP_SAVED',len(done),flush=True)
