"""Read-only P20/P30/P31 inventory using freshly described Unreal MCP schemas."""
import json, sys
from pathlib import Path
REPO = Path(__file__).resolve().parents[1]
JOB = REPO / 'Saved/GenerationJobs/city-life_P20_20260908'
sys.path.insert(0, str(REPO / 'SourceArt/Generated/Buildings/HansaSawmill_P13_20260908/scripts'))
from ue_batch import invoke, call

if __name__ == '__main__':
    (JOB / 'evidence').mkdir(parents=True, exist_ok=True)
    for name in ('scene.SceneTools', 'actor.ActorTools', 'asset.AssetTools', 'object.ObjectTools', 'static_mesh.StaticMeshTools'):
        group = 'editor_toolset.toolsets.' + name
        schema = invoke('describe_toolset', {'toolset_name': group})
        (JOB / 'evidence' / ('schema-' + name + '.json')).write_text(json.dumps(schema, indent=2))
        for tool in schema.get('tools', []):
            if tool['name'].split('.')[-1] in ('get_current_level', 'get_actors', 'get_components', 'get_properties', 'list_properties', 'is_dirty', 'get_dependencies', 'get_bounds'):
                print(json.dumps(tool))
    print('LEVEL', call('editor_toolset.toolsets.scene.SceneTools', 'get_current_level'))
