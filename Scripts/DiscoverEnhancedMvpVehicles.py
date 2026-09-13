"""Read-only discovery and identity gate before P19 editor mutations."""
import sys,json
from pathlib import Path
REPO=Path(__file__).resolve().parents[1]
JOB=REPO/'Saved/GenerationJobs/hansa-vehicles_P19_20260908'
sys.path.insert(0,str(REPO/'SourceArt/Generated/Buildings/HansaSawmill_P13_20260908/scripts'))
from ue_batch import invoke,call
groups=['editor_toolset.toolsets.'+x for x in ('asset.AssetTools','static_mesh.StaticMeshTools','material.MaterialTools','texture.TextureTools','object.ObjectTools','blueprint.BlueprintTools','scene.SceneTools','actor.ActorTools')]+['AutomationTestToolset.AutomationTestToolset','EditorToolset.EditorAppToolset','SlateInspectorToolset.SlateInspectorToolset']
selected={'exists','is_dirty','save_assets','import_file','get_bounds','get_material_slots','generate_lods','set_lod_thresholds','get_triangle_count','generate_convex_collisions','is_nanite_enabled','create_material','list_expression_classes','add_expression','connect_to_output','recompile','get_size','set_material','create','get_default_object','compile_blueprint','get_components','set_properties','get_properties','list_properties','DiscoverTests','RunTestsByFilter','GetTestResults','GetTestStatus','Windows','get_current_level'}
for group in groups:
    schema=invoke('describe_toolset',{'toolset_name':group})
    (JOB/'evidence'/('schema-'+group.split('.')[-1]+'.json')).write_text(json.dumps(schema,indent=2))
    for tool in schema.get('tools',[]):
        if tool['name'].split('.')[-1] in selected:print(json.dumps({'name':tool['name'],'inputSchema':tool['inputSchema']}))
print('LEVEL',call(groups[6],'get_current_level'))
print('WINDOWS',call(groups[-1],'Windows',action='list'))
