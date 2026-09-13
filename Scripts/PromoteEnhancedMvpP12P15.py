"""Explicitly approved P12-P15 promotion through discovered Unreal MCP tools.

Run plan, assets, or bind. Never overwrites an existing destination or saves maps.
Evidence and before-values make partial work inspectable; no provider calls.
"""
import hashlib
import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / 'SourceArt/Generated/Buildings/HansaSawmill_P13_20260908/scripts'))
from ue_batch import call

A = 'editor_toolset.toolsets.asset.AssetTools'
O = 'editor_toolset.toolsets.object.ObjectTools'
M = 'editor_toolset.toolsets.material.MaterialTools'
S = 'editor_toolset.toolsets.static_mesh.StaticMeshTools'
B = 'editor_toolset.toolsets.blueprint.BlueprintTools'
EVIDENCE = REPO / 'Saved/GenerationJobs/approved-p12-p15-20260908'
FAMILIES = [
    ('P12', 'LumberCamp_P12', 'hansa-lumber-camp', [('BP_LumberCamp_Review', 'LumberCamp', 'workBuilding')]),
    ('P13', 'Sawmill_P13', 'hansa-sawmill', [('BP_Sawmill_Review', 'Sawmill', 'workBuilding')]),
    ('P14', 'Residences_P14', 'hansa-residences', [('BP_Residence_Laborer_Review', 'Residence_Laborer', 'variantA'), ('BP_Residence_Artisan_Review', 'Residence_Artisan', 'variantA')]),
    ('P15', 'Market_P15', 'hansa-market', [('BP_Market_Review', 'Market', 'courtHall')]),
]

def ref(path):
    if '.' not in path.rsplit('/', 1)[-1]:
        path += '.' + path.rsplit('/', 1)[-1]
    return {'refPath': path}

def fields(obj):
    return json.loads(call(O, 'list_properties', instance=obj))

def get(obj, names):
    return json.loads(call(O, 'get_properties', instance=obj, properties=names))

def set_values(obj, values):
    assert call(O, 'set_properties', instance=obj, values=json.dumps(values)), (obj, values)

def save(data, name):
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    (EVIDENCE / name).write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8')

def plan():
    result = []
    for prompt, folder, slug, bps in FAMILIES:
        source = '/Game/Hansa/Generated/Staging/' + folder
        dest = '/Game/Mesh/' + slug
        assert not call(A, 'exists', path=dest), 'Existing destination: ' + dest
        queue = [source + '/' + bp for bp, _, _ in bps]
        records = {}
        while queue:
            path = queue.pop()
            if path in records:
                continue
            assert path.startswith(source + '/'), path
            assert not call(A, 'is_dirty', asset_path=path), 'Unsaved source: ' + path
            kind = call(A, 'get_asset_class', asset_path=path)
            if path in [source + '/' + bp for bp, _, _ in bps] and kind == path.rsplit('/', 1)[1] + '_C':
                kind = 'Blueprint'
            assert kind in ('Blueprint', 'StaticMesh', 'Material', 'Texture2D'), (path, kind)
            deps = call(A, 'get_dependencies', asset_path=path)
            for dep in deps:
                if '/Generated/Staging/' in dep:
                    assert dep.startswith(source + '/'), (path, dep)
                    queue.append(dep)
                elif dep.startswith('/Game/'):
                    raise AssertionError('Unexpected external game dependency: ' + dep)
            file = REPO / 'Content' / (path.removeprefix('/Game/') + '.uasset')
            records[path] = {'source': path, 'destination': path.replace(source, dest, 1), 'class': kind,
                             'sha256': hashlib.sha256(file.read_bytes()).hexdigest(), 'dependencies': deps}
        definitions = []
        for bp, definition, main in bps:
            path = '/Game/Hansa/Core/Buildings/DA_Building_' + definition
            assert not call(A, 'is_dirty', asset_path=path), 'Unsaved definition: ' + path
            names = list(fields(ref(path)))
            values = get(ref(path), names)
            bp_fields = fields(ref(source + '/' + bp))
            definitions.append({'path': path, 'before': values, 'blueprint': source + '/' + bp,
                                'blueprintFields': bp_fields, 'mainField': main})
        result.append({'prompt': prompt, 'source': source, 'destination': dest,
                       'assets': list(records.values()), 'definitions': definitions})
    save(result, 'plan.json')
    print(json.dumps([{'prompt': r['prompt'], 'assets': len(r['assets']), 'definitions': [d['path'] for d in r['definitions']]} for r in result]), flush=True)

def remap(value, family):
    if isinstance(value, str):
        return value.replace(family['source'] + '/', family['destination'] + '/')
    if isinstance(value, dict):
        return {k: remap(v, family) for k, v in value.items()}
    if isinstance(value, list):
        return [remap(v, family) for v in value]
    return value

def assets(families):
    for family in families:
        # An existing root means a partial/uncertain run, never implicit overwrite.
        assert not call(A, 'exists', path=family['destination']), family['destination']
        for item in family['assets']:
            file = REPO / 'Content' / (item['source'].removeprefix('/Game/') + '.uasset')
            assert hashlib.sha256(file.read_bytes()).hexdigest() == item['sha256'], item['source']
        for item in sorted(family['assets'], key=lambda i: {'Texture2D': 0, 'Material': 1, 'StaticMesh': 2, 'Blueprint': 3}[i['class']]):
            assert call(A, 'duplicate', path=item['source'], new_path=item['destination']), item
            obj = ref(item['destination'])
            if item['class'] == 'Material':
                for expression in call(M, 'get_expressions', material_or_function=obj):
                    if 'texture' in fields(expression):
                        old = get(expression, ['texture'])
                        new = remap(old, family)
                        if new != old:
                            set_values(expression, new)
                call(M, 'recompile', material_or_function=obj)
            elif item['class'] == 'StaticMesh':
                for slot in call(S, 'get_material_slots', mesh=obj):
                    old = call(S, 'get_material', mesh=obj, slot_name=slot)
                    new = remap(old, family)
                    if new != old:
                        assert call(S, 'set_material', mesh=obj, slot_name=slot, material=new)
            elif item['class'] == 'Blueprint':
                props = fields(obj)
                names = [k for k, v in props.items() if v.get('title', '').endswith(('StaticMeshComponent', 'InstancedStaticMeshComponent', 'StaticMesh'))]
                values = get(obj, names)
                for name, value in values.items():
                    if props[name].get('title', '').endswith('StaticMesh'):
                        if remap(value, family) != value:
                            set_values(obj, {name: remap(value, family)})
                    elif isinstance(value, dict) and value.get('refPath'):
                        assert value['refPath'].startswith(family['destination']), value
                        component_fields = fields(value)
                        keys = [k for k in ('staticMesh', 'overrideMaterials') if k in component_fields]
                        old = get(value, keys)
                        if remap(old, family) != old:
                            set_values(value, remap(old, family))
                call(B, 'compile_blueprint', blueprint=obj)
            call(A, 'update_metadata_tags', asset_path=item['destination'], set_tags={
                'Hansa.Approval': 'User approved ' + family['prompt'] + ' promotion on 2026-09-08',
                'Hansa.PromotionSource': item['source'], 'Hansa.PromotionSourceSHA256': item['sha256']})
            assert call(A, 'save_assets', asset_paths=[item['destination']])
            print('Saved ' + item['destination'], flush=True)
        audit_assets([family])

def audit_assets(families):
    for family in families:
        audit = []
        for item in family['assets']:
            try:
                deps = call(A, 'get_dependencies', asset_path=item['destination'])
            except RuntimeError as error:
                if item['class'] != 'Texture2D' or "'NoneType' object is not iterable" not in str(error):
                    raise
                assert call(A, 'exists', path=item['destination'])
                assert call(A, 'get_asset_class', asset_path=item['destination']) == 'Texture2D'
                deps = []
            assert not any('/Generated/Staging/' in p or '/Developer/' in p for p in deps), (item['destination'], deps)
            audit.append({'path': item['destination'], 'dependencies': deps})
        save(audit, family['prompt'] + '-dependencies.json')

def bind(families):
    for family in families:
        assert (EVIDENCE / (family['prompt'] + '-dependencies.json')).exists()
        for definition in family['definitions']:
            obj = ref(definition['path'])
            assert not call(A, 'is_dirty', asset_path=definition['path'])
            assert get(obj, list(definition['before'])) == definition['before'], 'Definition changed since preview'
            bp = remap(definition['blueprint'], family)
            props = fields(ref(bp))
            main = definition['mainField']
            if main == 'variantAMesh' and 'variantA' in props:
                main = 'variantA'
            assert main in props, (bp, main, list(props))
            mesh = get(ref(bp), [main])[main]
            if props[main].get('title', '').endswith('StaticMeshComponent'):
                fields(mesh)
                mesh = get(mesh, ['staticMesh'])['staticMesh']
            assert mesh['refPath'].startswith(family['destination'] + '/'), mesh
            actor = bp + '.' + bp.rsplit('/', 1)[1] + '_C'
            after = {'presentationActorClass': ref(actor), 'presentationMesh': mesh,
                     'authoredRevision': definition['before']['authoredRevision'] + 1}
            save({'before': definition['before'], 'changes': after}, 'diff-' + definition['path'].rsplit('/', 1)[1] + '.json')
            set_values(obj, after)
            assert call(A, 'save_assets', asset_paths=[definition['path']])
            actual = get(obj, list(definition['before']))
            for key in definition['before']:
                if key not in ('presentationActorClass', 'presentationMesh', 'authoredRevision', 'contentHash'):
                    assert actual[key] == definition['before'][key], key
            save(actual, 'bound-' + definition['path'].rsplit('/', 1)[1] + '.json')
            print('Bound ' + definition['path'], flush=True)

if __name__ == '__main__':
    mode = sys.argv[1]
    if mode == 'plan':
        plan()
    else:
        families = json.loads((EVIDENCE / 'plan.json').read_text())
        if len(sys.argv) > 2:
            families = [f for f in families if f['prompt'] == sys.argv[2]]
        {'assets': assets, 'audit': audit_assets, 'bind': bind}[mode](families)
