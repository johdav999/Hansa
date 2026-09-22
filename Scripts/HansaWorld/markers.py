"""Repair only the staged campaign reference markers through the live editor API.

Run with the preview stopped. --repair changes only the 62 marker/label actors;
then use Unreal's Save Current Level to persist World Partition external packages.
Without --repair this is a read-only actor and visibility regression check.
"""
import argparse
import json
import sys
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'SourceArt/Generated/Buildings/LubeckUrbanHousing_20260916/scripts'))
from editor import call

MAP='/Game/Hansa/Generated/Staging/HansaWorld_20260918/L_HansaWorld_WP'
SCENE='editor_toolset.toolsets.scene.SceneTools'
OBJECT='editor_toolset.toolsets.object.ObjectTools'
ACTOR='editor_toolset.toolsets.actor.ActorTools'

def run(repair=False):
    if call('EditorToolset.EditorAppToolset','IsPIERunning'):
        raise RuntimeError('Stop the preview before auditing or saving source marker actors.')
    if call(SCENE,'get_current_level')!=MAP:
        raise RuntimeError('Refusing to operate on a different map.')
    cities=json.loads((ROOT/'SourceArt/Terrain/HansaWorld/Prototype_20260918/terrain-manifest.json').read_text(encoding='utf-8'))['cities']
    expected={prefix+c['id'] for c in cities for prefix in ('DEV_CityMarker_','DEV_CityLabel_')}
    actors=call(SCENE,'get_actors_in_folder',folder_path='CityMarkers')
    by_label={call(ACTOR,'get_label',actor=a):a for a in actors}
    if set(by_label)!=expected or len(actors)!=len(expected):
        raise RuntimeError('Marker inventory differs from the 31-city manifest; refusing partial repair.')
    report=[]
    for label,actor in sorted(by_label.items()):
        schema=json.loads(call(OBJECT,'list_properties',instance=actor))
        names=['bIsEditorOnlyActor','bHidden','bIsSpatiallyLoaded']
        assert all(n in schema for n in names)
        before=json.loads(call(OBJECT,'get_properties',instance=actor,properties=names))
        if repair:
            values=dict(bIsEditorOnlyActor=False,bHidden=False,bIsSpatiallyLoaded=False)
            assert call(OBJECT,'set_properties',instance=actor,values=json.dumps(values))
        after=json.loads(call(OBJECT,'get_properties',instance=actor,properties=names))
        assert not any(after.values()),(label,after)
        components=call(ACTOR,'get_components',actor=actor)
        for component in components:
            if not any(n in component['refPath'] for n in ('StaticMeshComponent','NewTextRenderComponent')):continue
            props=json.loads(call(OBJECT,'list_properties',instance=component))
            keys=[k for k in ('bVisible','bHiddenInGame','bIsEditorOnly') if k in props]
            visibility=json.loads(call(OBJECT,'get_properties',instance=component,properties=keys))
            assert visibility.get('bVisible',True) and not visibility.get('bHiddenInGame',False) and not visibility.get('bIsEditorOnly',False),(label,visibility)
        report.append({'label':label,'before':before,'after':after})
    print(json.dumps({'map':MAP,'actors':len(report),'repair_applied':repair,'requires_native_level_save':repair,'editor_only_before':sum(r['before']['bIsEditorOnlyActor'] for r in report),'all_preview_visible':True}))

def audit_preview():
    if not call('EditorToolset.EditorAppToolset','IsPIERunning'):
        raise RuntimeError('This regression check requires a running Play/Simulate session.')
    group='editor_toolset.toolsets.programmatic.ProgrammaticToolset'
    call(group,'get_execution_environment')
    script='''import json
def find():
    return execute_tool("editor_toolset.toolsets.scene.SceneTools.find_actors",json.dumps({"name":"DEV_City","tag":"","collision_channels":[]}))["returnValue"]
def label(actor):
    return execute_tool("editor_toolset.toolsets.actor.ActorTools.get_label",json.dumps({"actor":actor}))["returnValue"]
def properties(actor):
    return json.loads(execute_tool("editor_toolset.toolsets.object.ObjectTools.get_properties",json.dumps({"instance":actor,"properties":["bIsEditorOnlyActor","bHidden","bIsSpatiallyLoaded"]}))["returnValue"])
def run():
    actors=find()
    assert len(actors)==62
    names=[]
    for actor in actors:
        assert "/UEDPIE_" in actor["refPath"] and "HansaWorld_20260918" in actor["refPath"]
        assert not any(properties(actor).values())
        names.append(label(actor))
    return {"preview_actors":len(actors),"names":names,"all_preview_visible":True}
'''
    result=json.loads(call(group,'execute_tool_script',script=script))
    cities=json.loads((ROOT/'SourceArt/Terrain/HansaWorld/Prototype_20260918/terrain-manifest.json').read_text(encoding='utf-8'))['cities']
    expected={prefix+c['id'] for c in cities for prefix in ('DEV_CityMarker_','DEV_CityLabel_')}
    assert set(result.pop('names'))==expected
    print(json.dumps(result))

if __name__=='__main__':
    parser=argparse.ArgumentParser();modes=parser.add_mutually_exclusive_group()
    modes.add_argument('--repair',action='store_true');modes.add_argument('--preview',action='store_true')
    args=parser.parse_args()
    if args.preview:audit_preview()
    else:run(args.repair)
