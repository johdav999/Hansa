"""P16 task-local MCP discovery and evidence. No provider calls."""
import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / 'SourceArt/Generated/Buildings/HansaSawmill_P13_20260908/scripts'))
from ue_batch import invoke, call

JOB = REPO / 'Saved/GenerationJobs/hansa-warehouse_P16_completion_20260908'
for folder in ('evidence', 'renders'):
    (JOB / folder).mkdir(parents=True, exist_ok=True)

def retain(name, value):
    (JOB / 'evidence' / (name + '.json')).write_text(json.dumps(value, indent=2), encoding='utf-8')

if __name__ == '__main__':
    if sys.argv[1] == 'discover':
        retain('toolsets', invoke('list_toolsets', {}))
        for group in ('asset.AssetTools', 'static_mesh.StaticMeshTools', 'material.MaterialTools', 'texture.TextureTools', 'object.ObjectTools', 'blueprint.BlueprintTools', 'actor.ActorTools', 'scene.SceneTools'):
            retain('schema-' + group.split('.')[-1], invoke('describe_toolset', {'toolset_name': 'editor_toolset.toolsets.' + group}))
        retain('schema-EditorAppToolset', invoke('describe_toolset', {'toolset_name': 'EditorToolset.EditorAppToolset'}))
        print('Discovered current schemas:', JOB)
