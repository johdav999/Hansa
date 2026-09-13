"""Survey-only daylight review rig. No ground, water, buildings, or gameplay changes."""
import json
from RostockTerrainSession import JOB, call

TERRAIN = 'HansaEditor.HansaCityTerrainToolset'
G = 'editor_toolset.toolsets.'
context = json.loads(call(TERRAIN,'InspectAuthoringContext'))
level = '/Game/Hansa/Generated/Staging/RostockTerrain_P31_20260908/L_Rostock_Survey_WP'
assert context['map'] == level and not context['pieRunning'] and not context['dirtyPackages']
roles = [('SurveyReviewSun','DirectionalLight',-40,-55),
         ('SurveyReviewSky','SkyLight',0,0),('SurveyReviewAtmosphere','SkyAtmosphere',0,0)]
for name,kind,pitch,yaw in roles:
    classes = call(G+'object.ObjectTools','search_subclasses',base_class={'refPath':'/Script/Engine.Actor'},class_name=kind)
    assert {'refPath':'/Script/Engine.'+kind} in classes
    assert not call(G+'scene.SceneTools','find_actors',name=name,tag='',collision_channels=[]), 'Do not create duplicate rig'
result = {}
for name,kind,pitch,yaw in roles:
    actor = call(G+'scene.SceneTools','add_to_scene_from_class',actor_type={'refPath':'/Script/Engine.'+kind},name=name,
                 xform={'location':{'x':0,'y':0,'z':15000},'rotation':{'pitch':pitch,'yaw':yaw,'roll':0}})
    components = call(G+'actor.ActorTools','get_components',actor=actor)
    result[name] = {'actor':actor,'components':components}
    for component in components:
        properties = call(G+'object.ObjectTools','list_properties',instance=component)
        (JOB/'evidence'/('rostock-'+name+'-'+component['refPath'].split('.')[-1]+'-properties.json')).write_text(properties)
    print(json.dumps(result[name]),flush=True)
(JOB/'evidence/rostock-review-rig.json').write_text(json.dumps(result,indent=2))
